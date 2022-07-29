#include "Arduino.h" // can them thu vien nay khi dung thu vien RFID
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "SPI.h" // SPI library
#include "MFRC522.h" // RFID library
#include <EEPROM.h>

// khai báo chân kết nối
#define BUZZER 2
#define SIMPOWER 8

#define DOORSERVO 7


#define pinRST  9
#define pinSDA  10

#define Keycol1  A0
#define Keycol2  A1
#define Keycol3  A2
#define Keycol4  A3

#define KeyrowA  3
#define KeyrowB  4
#define KeyrowC  5
#define KeyrowD  6

// khai báo mã phím
#define KEY0 0
#define KEY1 1
#define KEY2 2
#define KEY3 3
#define KEY4 4
#define KEY5 5
#define KEY6 6
#define KEY7 7
#define KEY8 8
#define KEY9 9
#define KEYA 10
#define KEYB 11
#define KEYC 12
#define KEYD 13

#define KEYSTAR 14
#define KEYHASH 15
#define NOKEY 16

// định nghĩa các trạng thái mạch
#define STAMAIN 0
#define STAUNLOCKNYCARD 1
#define STAUNLOCKBYCODE 2
#define STASETUPCARD 3
#define STADELETECARD 4
#define STACHANGEPASS 5
#define STAMENU 6
#define STACHECKPASS 7

// định nghĩa hằng số YES, NO
#define YES 1
#define NO 0





// định nghĩa cổng Serial kết nối module sim dùng Serial phần cứng, 
#define SimUART Serial

// các mã lệnh giao tiếp Module SIM qua tập lệnh AT
String CMDAT="AT";  // return OK
String CMDECHOOFF="ATE0"; // return OK
String CMDSETBAUD="AT+IPR=9600"; // return OK
String CMDSETSMSTEXTMODE="AT+CMGF=1"; // return OK
String CMDSAVEPARAMETER="AT&W"; // return OK
String CMDSETNOSAVESMS="AT+CNMI=2,2,0,0"; // return OK
String CMDCHECKNETWORKSTATUS="AT+CREG?"; // return OK  +CREG: 0,3; no register GSM to net work. +CREG: 0,1 -> good 0 mean GSM, 1 mean registed
String CMDSENDSMS="AT+CMGS=";

// biến lưu nội dung tin nhắn
String SMSCome;


LiquidCrystal_I2C LCDDisplay(0x27,16,2); 

// khai báo các biến lưu giá trị RFID , pass word
byte CurrentState=STAMAIN;
byte NextState;

byte CurrentRFID[4];
byte RFIDActive=NO;
byte CurrentPass[6];
byte PassWord[6];
byte CheckingRFID[4];
byte NumerOfCard=0;
byte DeleteCardIndex;

Servo DoorServo;

byte CurrentPassIndex;
byte DoorOpenState=NO;
unsigned long DoorOpenTimeCount;
unsigned long TimeCount;
byte WrongPassCount=0;

MFRC522 MyRFID(pinSDA, pinRST);


void SavePasswordToEEPROM() // hàm lưu pass vào EEPROM
{
  EEPROM.write(0x00,PassWord[0]);
  EEPROM.write(0x01,PassWord[1]);
  EEPROM.write(0x02,PassWord[2]);
  EEPROM.write(0x03,PassWord[3]);
  EEPROM.write(0x04,PassWord[4]);
  EEPROM.write(0x05,PassWord[5]);
  
}

void GetPasswordFromEEPROM() // hàm đọc pass từ EEPROM
{
  byte i;
  byte CurrentValue;
  {
    for(i=0;i<6;i++)
    {
      CurrentValue=EEPROM.read(i);
      if(CurrentValue==0xFF)// nếu rom trắng thì ghi pass mặc đinh 024681
      {
        PassWord[0]=0;
        PassWord[1]=2;
        PassWord[2]=4;
        PassWord[3]=6;
        PassWord[4]=8;
        PassWord[5]=1;
        SavePasswordToEEPROM();
        return;
      }
      else
      {
        PassWord[i]=CurrentValue;
      }
    }
  }
}
 
void GetNumberOfCard() // hàm lấy số lượng thẻ hiện tại đã được lưu trong eeprom
{
  NumerOfCard=EEPROM.read(0x06);
  if(NumerOfCard==0xFF)// blank EEPROM is 0 card
  {
    NumerOfCard=0;
    EEPROM.write(0x06,NumerOfCard);
  }
   
}

void SaveCardToEEPROM() // hàm lưu mã thẻ vào EEPROM
{
 
  EEPROM.write(0x07+(4*NumerOfCard),CurrentRFID[0]);
  EEPROM.write(0x08+(4*NumerOfCard),CurrentRFID[1]);
  EEPROM.write(0x09+(4*NumerOfCard),CurrentRFID[2]);
  EEPROM.write(0x0A+(4*NumerOfCard),CurrentRFID[3]);
  NumerOfCard++;
  EEPROM.write(0x06,NumerOfCard);
   
}

byte CheckRFIDCardToUnlock() // hàm kiểm tra mã RFID so với mã đã lưu nếu tìm thấy trả về vị trí mã lưu trong eeprom nếu ko tìm thấy trả về 0xFF
{
  char CardIndex;
  for(CardIndex=0;CardIndex<9;CardIndex++)
  {
    CheckingRFID[0]=EEPROM.read(0x07+(4*CardIndex));
    CheckingRFID[1]=EEPROM.read(0x08+(4*CardIndex));
    CheckingRFID[2]=EEPROM.read(0x09+(4*CardIndex));
    CheckingRFID[3]=EEPROM.read(0x0A+(4*CardIndex));
    if(CurrentRFID[0]==CheckingRFID[0]&&CurrentRFID[1]==CheckingRFID[1]&&CurrentRFID[2]==CheckingRFID[2]&&CurrentRFID[3]==CheckingRFID[3])
    {
      return CardIndex;
    }
  }
  return 0xFF;
  
}



void DeleteOldCard(byte CardIndexToDelete) // hàm xóa 1 thẻ trong EEPROM
{
      char EEPROMData;
      NumerOfCard--;
      EEPROM.write(0x06,NumerOfCard);
      if(NumerOfCard==CardIndexToDelete)// nếu thẻ cần xóa ở cuối mảng thì xóa thông tin
      {
      EEPROM.write(0x07+(4*CardIndexToDelete),0xFF);
      EEPROM.write(0x08+(4*CardIndexToDelete),0xFF);
      EEPROM.write(0x09+(4*CardIndexToDelete),0xFF);
      EEPROM.write(0x0A+(4*CardIndexToDelete),0xFF);
      }
      else // nếu thẻ cần xóa ở giữa mảng thì chuyển dữ liệu lên cuối mảng lên rồi xóa dữ liệu cuối mảng
      {
      
      
      EEPROMData=EEPROM.read(0x07+(4*NumerOfCard));
      EEPROM.write(0x07+(4*CardIndexToDelete),EEPROMData);
      EEPROMData=EEPROM.read(0x08+(4*NumerOfCard));
      EEPROM.write(0x08+(4*CardIndexToDelete),EEPROMData);
      EEPROMData=EEPROM.read(0x09+(4*NumerOfCard));
      EEPROM.write(0x09+(4*CardIndexToDelete),EEPROMData);
      EEPROMData=EEPROM.read(0x0A+(4*NumerOfCard));
      EEPROM.write(0x0A+(4*CardIndexToDelete),EEPROMData);
      // delete last card
      EEPROM.write(0x07+(4*NumerOfCard),0xFF);
      EEPROM.write(0x08+(4*NumerOfCard),0xFF);
      EEPROM.write(0x09+(4*NumerOfCard),0xFF);
      EEPROM.write(0x0A+(4*NumerOfCard),0xFF);

      }
}


String PhoneNumber1 = "+84768897635"; //-> So dien thoai may chu nhan 

// biến lưu trạng thái GSM 
bool GSMModuleStatus=false;

// hàm xóa bộ nhờ đệm cổng Serial 
void FlushSIMUART()
{
  byte UARTData;
  while(SimUART.available())
  {
    UARTData=SimUART.read();
  }
}


// hàm gửi lệnh giao tiếp module SIM và chờ kết quả trả về OK
bool SendCommandWaitforOK(String Command,unsigned long TimeOut)
{
  
  String SimData;
  char UARTData;
  unsigned long TimeStamp;
 
  
  //DebugSerial.print("Send Command: ");
  //DebugSerial.println(Command);
  SimUART.println(Command); // gửi lệnh qua cổng Serial 
  delay(100);
  TimeStamp=millis();
  while(millis()-TimeStamp<TimeOut)
  {
    while(SimUART.available()) // khi có kết quả trả về thì đọc kết quả
    {
      UARTData=SimUART.read();
      SimData+=UARTData;
      if(SimData.indexOf("OK")>=0) // nếu kết quả trả về OK thì kết thúc 
      {
         //DebugSerial.println("OK");
         FlushSIMUART();
         return true;
         
      }
    }
  }
    //DebugSerial.println("FAIL"); // nếu chờ hết thời gian mà không thấy OK thì báo fail
    FlushSIMUART();
    return false;

}


// hàm kiểm tra trạng thái GSM
bool CheckGSMStatus(byte RetryNumber)
{
  byte Retry;
  String SimData;
  char UARTData;

  FlushSIMUART();
   
  for(Retry=0;Retry<RetryNumber;Retry++)
  {
    SimData="";
    SimUART.println(CMDCHECKNETWORKSTATUS); // gửi lệnh kiểm tra GSM
    delay(1000);
    
    while(SimUART.available())
    {
      UARTData=SimUART.read();
      SimData+=UARTData;

       if(SimData.indexOf("+CREG: 0,1")>=0)  // nếu trả về mã +CREG: 0,1 là đã đăng ký mạng thành công
      {
        FlushSIMUART();
        return true;
      }
    }
    
    
    delay(2000);
  }
  
   FlushSIMUART();
   return false;
   
}

//hàm chờ thời gian 20 giây sau khi cắm nguồn để module SIM nhận mạng
void WaitforSIMReady()// wait in xx Second 
{
  int Time;
  LCDDisplay.clear();
  LCDDisplay.print("DANG KHOI DONG...");
  LCDDisplay.setCursor(0,1);
  for(Time=0;Time<10;Time++)
  {
    LCDDisplay.print(">");
    delay(2000);
  }
  
}

void SIMpower() // ham kich hoat module SIM chan 9
{
  pinMode(SIMPOWER, OUTPUT); 
  digitalWrite(SIMPOWER,LOW);
  delay(1000);
  digitalWrite(SIMPOWER,HIGH);
  delay(2000);
  digitalWrite(SIMPOWER,LOW);
  delay(3000);
}

void InitSim() // hàm cấu hình ban đầu module SIM
{
  byte Retry;
  

  Retry=0;
  GSMModuleStatus=false;
  GSMModuleStatus=SendCommandWaitforOK(CMDAT,1000); // gửi lệnh AT
  while(GSMModuleStatus==false&&Retry<3) 
  {
    GSMModuleStatus=SendCommandWaitforOK(CMDAT,1000); // gửi lệnh AT
    Retry++;
  }
  
  if(GSMModuleStatus==false) return;
  
  LCDDisplay.print(">");
  
  GSMModuleStatus=SendCommandWaitforOK(CMDECHOOFF,1000); // gửi lệnh ATE0 
  if(GSMModuleStatus==false) return;
  
  LCDDisplay.print(">");
  
  GSMModuleStatus=SendCommandWaitforOK(CMDSETBAUD,1000); // cài tốc độ baud
  if(GSMModuleStatus==false) return;
  
  LCDDisplay.print(">");
  
  GSMModuleStatus=SendCommandWaitforOK(CMDSETSMSTEXTMODE,1000); // cài chế độ tin nhắn dạng TEXT
  if(GSMModuleStatus==false) return;
  
  LCDDisplay.print(">");
  
  GSMModuleStatus=SendCommandWaitforOK(CMDSETNOSAVESMS,1000); // Cài chế độ nhận tin nhắn khong lưu vào SIM
  if(GSMModuleStatus==false) return;
  
  LCDDisplay.print(">");
  
  FlushSIMUART(); // xóa bộ nhớ đệm
  
}

void DisplayStart() // hàm hiển thị đang khởi động
{
  LCDDisplay.clear();
  LCDDisplay.setCursor(0,0);
  LCDDisplay.print(" HT CUA TU DONG ");
  LCDDisplay.setCursor(0,1);
  LCDDisplay.print(" Khoi dong...");
}

void DisplayMain() // hàm hiển thị màn hình chính
{
  LCDDisplay.clear();
  LCDDisplay.setCursor(0,0);
  LCDDisplay.print("  MOI QUET THE  ");
  LCDDisplay.setCursor(0,1);
  LCDDisplay.print(" NHAP MAT KHAU  ");
}



// hàm gửi 1 tin nhắn
void SendSMS(String Content)
{

  SimUART.print(CMDSENDSMS);
  SimUART.write(0x22);// character "
  SimUART.print(PhoneNumber1);
  SimUART.write(0x22);// character "
  SimUART.write(0x0D);
  SimUART.write(0x0A);
  delay(500);
  SimUART.print(Content);
  SimUART.write(0x1A);
  delay(3000);
  FlushSIMUART(); // xóa dữ liệu trả về khi gửi xong tin nhắn 
}


void DisplayGSMReady() // hàm hiển thị GSM sẵn sàng
{
  LCDDisplay.setCursor(0,1);
  LCDDisplay.print(" GSM SAN SANG!  ");
}

void OnBuzzer() // bật loa
{
  digitalWrite(BUZZER,HIGH);
}

void OffBuzzer() // tắt loa
{
  digitalWrite(BUZZER,LOW);
}

void Beep(byte Repeat) // hàm phát tiếng bip
{
  byte Index;
  for(Index=0;Index<Repeat;Index++)
  {
    OnBuzzer();
    delay(200);
    OffBuzzer();
    delay(200);
  }
}

void LongBeep() // hàm phát tiếng bip dài
{
    OnBuzzer();
    delay(1000);
    OffBuzzer();
}

void DisplayGSMFail() // hàm hiển thị ghi lỗi kết nối GSM
{
  LCDDisplay.setCursor(0,1);
  LCDDisplay.print("GSM LOI KET NOI!");
}

void DisplayModuleSIMFail() // hàm hiển thị module sim bị lỗi
{
  LCDDisplay.setCursor(0,1);
  LCDDisplay.print("MODULE SIM FAIL!");
}

void DisplaySMSCome(String Message) // hàm hiển thị tin nhắn đến
{
  LCDDisplay.clear();
  LCDDisplay.print("Co Tin Den:");
  LCDDisplay.setCursor(0,1);
  LCDDisplay.print(Message);
}

void CheckUARTSMS() // hàm kiểm tra dữ liệu tin nhắn đến 
{
   char SerialData;
   String SerialString;
   unsigned long TimeWait;

   int StartIndex;
   int EndIndex;
   
   TimeWait=millis();
   while(millis()-TimeWait<1000) // doc lien tuc trong 1 giay
  { 
    while(SimUART.available()) 
    {
      SerialData=SimUART.read();
      SerialString+=SerialData;
    }   
  }

  FlushSIMUART();
  if(SerialString.indexOf("CMT")>=0) // phần tách nội dung tin nhắn
  {
     StartIndex=SerialString.indexOf("\n",5); 
     EndIndex=SerialString.indexOf("\r",StartIndex+3);
     SMSCome=SerialString.substring(StartIndex+1,EndIndex);
     //DebugSerial.print("SMS COME: ");
     //DebugSerial.println(SMSCome);
     DisplaySMSCome(SMSCome);
     if(SerialString.indexOf("OPEN")>=0||SerialString.indexOf("Open")>=0||SerialString.indexOf("open")>=0) // nếu nội dung open thì mở cửa
     {
        OpenDoor();
        LongBeep();
        DoorOpenState=YES;
        DoorOpenTimeCount=millis();
     }
     delay(3000);
     DisplayMain();
  }
 
}

void StartModuleSIM() // hàm khởi động module SIM
{
  SIMpower();
  delay(1000);
  WaitforSIMReady();
  InitSim();
  if(GSMModuleStatus==true) // nếu module sim sẵn sàng
  {
    GSMModuleStatus=CheckGSMStatus(3); // gửi hàm kiểm tra kết nối GSM
    if(GSMModuleStatus==true) // nếu kết nối tốt thì sáng LED GSM
    {
     DisplayGSMReady();
     delay(1000);
     SendSMS("CUA TU DONG SAN SANG"); //gửi tin nhắn hệ thống sẵn sàng
     delay(3000);
    }
    else
    {
       DisplayGSMFail();
       delay(2000);
    }
  }
  else
  {
      DisplayModuleSIMFail();
      delay(2000);
  }
   FlushSIMUART();
}

byte ScanKeyboard(void) // hàm quét phím
{
 digitalWrite(Keycol1,HIGH); 
 digitalWrite(Keycol2,HIGH); 
 digitalWrite(Keycol3,HIGH); 
 digitalWrite(Keycol4,HIGH); 
 
 digitalWrite(Keycol1,LOW);
 if(!digitalRead(KeyrowA)) 
 { 
  delay(100);
  while(!digitalRead(KeyrowA));
  return KEY1;
 }
 if(!digitalRead(KeyrowB)) 
 { 
  delay(100);
  while(!digitalRead(KeyrowB));

  return KEY4;
 }
  if(!digitalRead(KeyrowC)) 
 { 
  delay(100);
  while(!digitalRead(KeyrowC));
  return KEY7;
 }

  if(!digitalRead(KeyrowD)) 
 { 
  delay(100);
  while(!digitalRead(KeyrowD));
  return KEYSTAR;
 }
 digitalWrite(Keycol1,HIGH);
 digitalWrite(Keycol2,LOW);

  if(!digitalRead(KeyrowA)) 
 { 
  delay(100);
  while(!digitalRead(KeyrowA));
  return KEY2;
 }
 if(!digitalRead(KeyrowB)) 
 { 
  delay(100);
  while(!digitalRead(KeyrowB));
  return KEY5;
 }
  if(!digitalRead(KeyrowC)) 
 { 
  delay(100);
  while(!digitalRead(KeyrowC));
  return KEY8;
 }

  if(!digitalRead(KeyrowD)) 
 { 
  delay(100);
  while(!digitalRead(KeyrowD));
  return KEY0;
 }

  digitalWrite(Keycol2,HIGH);
 digitalWrite(Keycol3,LOW);
  if(!digitalRead(KeyrowA)) 
 { 
  delay(100);
  while(!digitalRead(KeyrowA));
  return KEY3;
 }
 if(!digitalRead(KeyrowB)) 
 { 
  delay(100);
  while(!digitalRead(KeyrowB));
  return KEY6;
 }
  if(!digitalRead(KeyrowC)) 
 { 
  delay(100);
  while(!digitalRead(KeyrowC));
  return KEY9;
 }

  if(!digitalRead(KeyrowD)) 
 { 
  delay(100);
  while(!digitalRead(KeyrowD));
  return KEYHASH;
 }
 
 digitalWrite(Keycol3,HIGH);
 digitalWrite(Keycol4,LOW);
  if(!digitalRead(KeyrowA)) 
 { 
  delay(100);
  while(!digitalRead(KeyrowA));

  return KEYA;
 }
 if(!digitalRead(KeyrowB)) 
 { 
  delay(100);
  while(!digitalRead(KeyrowB));

  return KEYB;
 }
  if(!digitalRead(KeyrowC)) 
 { 
  delay(100);
  while(!digitalRead(KeyrowC));

  return KEYC;
 }

  if(!digitalRead(KeyrowD)) 
 { 
  delay(100);
  while(!digitalRead(KeyrowD));

  return KEYD;
 }
 
 digitalWrite(Keycol4,HIGH);
 
 return NOKEY;

}

void ResetRFIDFlag() // hàm reset cờ RFID
{
  RFIDActive=NO;
}

void CloseDoor() // hàm quay servo đóng cửa
{
  DoorServo.attach(DOORSERVO);
  DoorServo.write(90);
  delay(200);
  DoorServo.detach();
}



void OpenDoor() // hàm quay servo mở cửa
{
  DoorServo.attach(DOORSERVO);
  DoorServo.write(0);
  delay(200);
  DoorServo.detach();
}



void DisPlayCheckPass() // hàm hiên thị yêu cầu nhập mật mã
{
  LCDDisplay.clear();
  LCDDisplay.setCursor(3,0);
  LCDDisplay.print("NHAP MAT MA:"); 
  LCDDisplay.setCursor(3,1);

  CurrentPassIndex=0;
  CurrentPass[0]=0;
  CurrentPass[1]=0;
  CurrentPass[2]=0;
  CurrentPass[3]=0;
  CurrentPass[4]=0;
  CurrentPass[5]=0;
}



void DisplayFunction(byte StateDisplay) // hàm hiển thị thực hiện các chức năng theo phím bấm
{
  LCDDisplay.clear();
  switch(StateDisplay)
  {
    case STASETUPCARD: // hiển thị thêm thẻ mới
    {
      if(NumerOfCard<9)
      {
      LCDDisplay.setCursor(0,0);
      LCDDisplay.print("MOI QUET THE MOI"); 
      }
      else
      {
        LCDDisplay.setCursor(3,0);
        LCDDisplay.print("THEM THE MOI"); 
        LCDDisplay.setCursor(3,1);
        LCDDisplay.print("BO NHO DAY!");
        delay(1000);
        CurrentState=STAMAIN;
        DisplayMain();
      
      }
      break;
    }
    case STADELETECARD: // hiển thị xóa thẻ
    {

      LCDDisplay.setCursor(0,0);
      LCDDisplay.print("MOI QUET THE XOA"); 
      break;
    }
    case STACHANGEPASS: // hiển thị đổi mật mã
    {

      LCDDisplay.setCursor(3,0);
      LCDDisplay.print("NHAP MA MOI"); 
      LCDDisplay.setCursor(3,1);
      CurrentPassIndex=0;
      break;
    }
    case STAUNLOCKBYCODE: // hiển thị mở khóa
    {
       WrongPassCount=0;
       OpenDoor();
       LCDDisplay.setCursor(3,0);
       LCDDisplay.print("DA MO CUA!"); 
       LCDDisplay.setCursor(0,1);
       LCDDisplay.print(" CO QUYEN T.LAP");
       LongBeep();
       SendSMS("Da mo cua bang mat khau");
       DoorOpenTimeCount=millis();
       DoorOpenState=YES;
       CurrentState=STAMENU;
     
       break;
    }
    
  }
  
}

bool VerifyPass() // hàm kiểm tra pass
{
  byte i;
  
  for(i=0;i<6;i++)
  {
    if(CurrentPass[i]!=PassWord[i]) return false;
  }

  return true;
}

void ArlarmWarning() // hàm bật cảnh báo khi nhập sai quá 3 lần
{
  OnBuzzer();
  SendSMS("CANH BAO! TRUY CAP SAI NHIEU LAN.");
}


void setup() {
  SimUART.begin(9600);
  //DebugSerial.begin(9600);

  pinMode(BUZZER,OUTPUT);


  pinMode(Keycol1, OUTPUT);
  pinMode(Keycol2, OUTPUT);
  pinMode(Keycol3, OUTPUT);
  pinMode(Keycol4, OUTPUT);
  pinMode(KeyrowA, INPUT_PULLUP);
  pinMode(KeyrowB, INPUT_PULLUP);
  pinMode(KeyrowC, INPUT_PULLUP);
  pinMode(KeyrowD, INPUT_PULLUP);


  SPI.begin(); // open SPI connection
  MyRFID.PCD_Init(); // Initialize Proximity Coupling Device (PCD)
  
  CloseDoor();
  OffBuzzer();

  LCDDisplay.init();                    
  LCDDisplay.backlight();
  DisplayStart();

  StartModuleSIM();
  
  CurrentState=STAMAIN;
  DisplayMain();
  GetPasswordFromEEPROM();
  GetNumberOfCard();
  TimeCount=millis();
}

void loop() {

   byte KeyCode;
   if(SimUART.available()) CheckUARTSMS();
  
   if(millis()-TimeCount>1000) // cứ môi 1 giây thì reset cờ RFID 1 lần
   {
    TimeCount=millis();
    ResetRFIDFlag();
   }

   if(DoorOpenState==YES) // nếu cửa đang mở thì sau 1 giây đóng lại
   {
    if(millis()-DoorOpenTimeCount>10000)
    {
      CloseDoor();
      DoorOpenState=NO;
      if(CurrentState==STAMENU) 
      {
        CurrentState=STAMAIN;
        DisplayMain();
        
      }
    }
   }

   if (MyRFID.PICC_IsNewCardPresent())// (true, if RFID tag/card is present ) PICC = Proximity Integrated Circuit Card // nếu có thẻ chạm
   { 
    //Serial.print("RFID detected!");
    if(MyRFID.PICC_ReadCardSerial()&&RFIDActive==NO) // true, if RFID tag/card was read and no pending process // đọc mã thẻ
    { 
      //Serial.print("RFID TAG ID:");
      //MyLCD.setCursor(3,1);
      for (byte i = 0; i < MyRFID.uid.size; ++i) { // read id (in parts)
        //Serial.print(MyRFID.uid.uidByte[i], HEX); // print id as hex values
        //Serial.print(" "); // add space between hex blocks to increase readability
        //MyLCD.print(MyRFID.uid.uidByte[i],HEX);
        
        CurrentRFID[i]=MyRFID.uid.uidByte[i];
        
      } 
      RFIDActive=YES;     // bật cờ RFIS
      //Serial.println(); // Print out of id is complete.
    }
   }

   if(RFIDActive==YES&&CurrentState==STASETUPCARD) // nếu cờ RFID được bật có thẻ thì và đang ở chế độ thêm thẻ mới thì xử lý thêm thẻ mới
   {
    SaveCardToEEPROM();
    LCDDisplay.setCursor(3,1);
    LCDDisplay.print("DA THEM THE!");
    LCDDisplay.write(NumerOfCard+48);
    CurrentRFID[0]=0;
    CurrentRFID[1]=0;
    CurrentRFID[2]=0;
    CurrentRFID[3]=0;
    Beep(1);
    delay(1000);
    RFIDActive=NO;
    CurrentState=STAMAIN;
    DisplayMain();
   }

   if(RFIDActive==YES&&CurrentState==STADELETECARD) // nếu cờ RFID được bật có thẻ thì và đang ở chế độ xóa thẻ  thì xử lý xóa thẻ
   {
    DeleteCardIndex=CheckRFIDCardToUnlock();
    if(DeleteCardIndex!=0xFF)
    {
       DeleteOldCard(DeleteCardIndex);
       LCDDisplay.setCursor(0,1);
       LCDDisplay.print(" Da Xoa the!    ");
       LCDDisplay.write(DeleteCardIndex+1+48);
       Beep(1);
       delay(1000);
    }
    else
    {

       LCDDisplay.setCursor(0,1);
       LCDDisplay.print("THE KHONG T.TAI ");
       Beep(3);
       delay(1000); 
    }
    RFIDActive=NO;
    CurrentState=STAMAIN;
    DisplayMain();
   }

   if(RFIDActive==YES&&CurrentState==STAMAIN) // nếu cờ RFID được bật có thẻ thì và đang ở chế độ chờ thì kiểm tra thẻ và mở cửa
   {
    if(CheckRFIDCardToUnlock()!=0xFF)
    {
       WrongPassCount=0;
       OpenDoor();
       LCDDisplay.clear();
       LCDDisplay.setCursor(3,0);
       LCDDisplay.print("DA MO CUA!"); 
       LCDDisplay.setCursor(0,1);
       LCDDisplay.print(" CO QUYEN T.LAP");
       LongBeep();
       SendSMS("Da mo cua bang the tu");
       DoorOpenTimeCount=millis();
       DoorOpenState=YES;
       CurrentState=STAMENU;
       RFIDActive=NO;
    }
    else // nếu thẻ không hợp lệ
    {
       LCDDisplay.setCursor(0,1);
       LCDDisplay.print("VUI LONG THU LAI");
       WrongPassCount++;
       if(WrongPassCount<=3)
       {
            Beep(3);
            delay(1000);
        }
       else
       {
          ArlarmWarning();
       }
        RFIDActive=NO;
        CurrentState=STAMAIN;
        DisplayMain();
     }
   
   }
   

   KeyCode=ScanKeyboard(); // quét phím
   if(KeyCode!=NOKEY)  // nếu có phím bấm
   {
    switch(CurrentState) // tùy vào trạng thái hiện tại của mạch mà xử lý
    {
      case STASETUPCARD: // nếu đang ở chế độ thêm hoặc xóa thẻ
      case STADELETECARD:
      {
        if(KeyCode==KEYSTAR) // nếu bấm nút sao thì quay về màn hình chính
        {
          CurrentState=STAMAIN;
          DisplayMain();
        }
        break;
      }
      case STAMAIN: // nếu ở chế độ chờ thì tiến hành nhập mã và check mở cửa
      {
          CurrentState=STACHECKPASS;
          NextState=STAUNLOCKBYCODE;
          DisPlayCheckPass();
          break;
      }
      case STAMENU: // nếu ở chế độ đang mở cửa và chờ setup
      {
        
        if(KeyCode==KEYSTAR) //nếu bấm nút sao thì quay về màn hình chính
        {
          CurrentState=STAMAIN;
          DisplayMain();
        }
        else if(KeyCode==KEYB) // bấm nút B thì vào thêm thẻ
        {
          CurrentState=STASETUPCARD;
          DisplayFunction(CurrentState);
        }
         else if(KeyCode==KEYC) // bấm nút C thì vào xóa thẻ
        {
          CurrentState=STADELETECARD;
          DisplayFunction(CurrentState);
        }
        else if(KeyCode==KEYA) // bấm nút A thì vào đổi pass
        {
          CurrentState=STACHECKPASS;
          NextState=STACHANGEPASS;
          DisPlayCheckPass();
        }
         break;
      }
      
      case STACHECKPASS: // nếu đang ở chế độ nhập pass
      {
        
           switch(KeyCode) // nếu bấm số thì lưu mã
           {
              case KEY0:
              case KEY1:
              case KEY2:
              case KEY3:
              case KEY4:
              case KEY5:
              case KEY6:
              case KEY7:
              case KEY8:
              case KEY9:
              {
                if(CurrentPassIndex<6)// max is 6 digit
                {
                  CurrentPass[CurrentPassIndex]=KeyCode;
                  //LCDDisplay.write(CurrentPass[CurrentPassIndex]+48);
                  LCDDisplay.print("*"); // hiển thị dấu sao
                  CurrentPassIndex++;
                }
                
                break;
              }
              case KEYHASH: // bấm phím thăng thì tiến hành kiểm tra mã đã nhập
              {
                
                if(VerifyPass()==true)  // mã đúng thì tiến hành xử lý
                {
                  WrongPassCount=0;
                  CurrentState=NextState;
                  DisplayFunction(CurrentState);  
                }
                else  // mã si báo thử lại và đếm số lần sai
                {
                   LCDDisplay.setCursor(0,1);
                   LCDDisplay.print("VUI LONG THU LAI");
                   WrongPassCount++;
                   if(WrongPassCount<=3) // nếu sai ít hơn 3 lần thì cho nhập lại
                   {
                      Beep(3);
                      delay(1000);
                   }
                   else // sai nhiều hơn 3 lần thì bật cảnh báo
                   {
                     ArlarmWarning();
                   }
                   CurrentPassIndex=0;
                   CurrentPass[0]=0;
                   CurrentPass[1]=0;
                   CurrentPass[2]=0;
                   CurrentPass[3]=0;
                   CurrentPass[4]=0;
                   CurrentPass[5]=0;
                   LCDDisplay.setCursor(0,1);
                   LCDDisplay.print("                ");
                   LCDDisplay.setCursor(3,1);
                      
                }
                break;
              }
               case KEYSTAR: // nêu bấm phím * thì quay về màn hình chính
              {
                   CurrentPassIndex=0;
                   CurrentPass[0]=0;
                   CurrentPass[1]=0;
                   CurrentPass[2]=0;
                   CurrentPass[3]=0;
                   CurrentPass[4]=0;
                   CurrentPass[5]=0;
                   CurrentState=STAMAIN;
                   DisplayMain();
              }
            
           }
        
         break;
      }
      case STACHANGEPASS: // nếu đang chế độ đổi pass
      {
        switch(KeyCode)
           {
              case KEY0:
              case KEY1:
              case KEY2:
              case KEY3:
              case KEY4:
              case KEY5:
              case KEY6:
              case KEY7:
              case KEY8:
              case KEY9:
              {
                if(CurrentPassIndex<6)// max is 6 digit
                {
                  CurrentPass[CurrentPassIndex]=KeyCode;
                  //LCDDisplay.write(CurrentPass[CurrentPassIndex]+48);
                   LCDDisplay.print("*");
                  CurrentPassIndex++;
                }
                
                break;
              }
              case KEYHASH: // bấm phím thăng để kiểm tra và đổi pass
              {
                
                if(CurrentPassIndex<6) 
                {
                  LCDDisplay.setCursor(3,1);
                  LCDDisplay.print("CHUA DU 6 KT");
                  Beep(3);
                   delay(1000);
                   CurrentPassIndex=0;
                   CurrentPass[0]=0;
                   CurrentPass[1]=0;
                   CurrentPass[2]=0;
                   CurrentPass[3]=0;
                   CurrentPass[4]=0;
                   CurrentPass[5]=0;
                   LCDDisplay.setCursor(3,1);
                   LCDDisplay.print("             ");
                   LCDDisplay.setCursor(3,1);
                }
                else
                {
                   PassWord[0]=CurrentPass[0];
                   PassWord[1]=CurrentPass[1];
                   PassWord[2]=CurrentPass[2];
                   PassWord[3]=CurrentPass[3];
                   PassWord[4]=CurrentPass[4];
                   PassWord[5]=CurrentPass[5];
                   SavePasswordToEEPROM();
                   LCDDisplay.setCursor(3,1);
                   LCDDisplay.print("DA LUU MA MOI");
                   Beep(1);
                   delay(1000);
                   CurrentPassIndex=0;
                   CurrentPass[0]=0;
                   CurrentPass[1]=0;
                   CurrentPass[2]=0;
                   CurrentPass[3]=0;
                   CurrentPass[4]=0;
                   CurrentPass[5]=0;
                   CurrentState=STAMAIN;
                   DisplayMain();
                }
                break;
              }
               case KEYSTAR: // bấm phím sao quy về màn hình chính
              {
                   CurrentPassIndex=0;
                   CurrentPass[0]=0;
                   CurrentPass[1]=0;
                   CurrentPass[2]=0;
                   CurrentPass[3]=0;
                   CurrentPass[4]=0;
                   CurrentPass[5]=0;
                   CurrentState=STAMAIN;
                   DisplayMain();
                   break;
              }
            
           }
        break;
      }

     
    }
   }
 

}
