#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <Ultrasonic.h>
#include <Adafruit_MLX90614.h>

#define Pin_Servo                         9
#define Led_Hijau                         7
#define Led_Merah                         6
#define Pin_TekanButton                   8
#define Ultra_TriggerPin                 12
#define Ultra_EchoPin                    13

#define MulaiBerjalan                     'a'
#define MengulangKembali_ESP32            'r'
#define MengulangKembali_SCAN             'n'

#define TutupPalang                     180
#define BukaPalang                      90
#define KunciPalang                     0
#define BukaPalang_TIME                 4000
#define TutupPalang_TIME                1000
#define DELAY_TEMP_SUCCESS              2000
#define Mengulang_TIME                  5000

#define ON                              0
#define OFF                             1

#define Deteksi_Masker_TH               80 
#define mulai_scan_COUNT                15
#define JarakError_COUNT                10
#define ERROR_COUNT                     3
#define SuhuDiterimaMax                 39
#define BatasSuhu                       35

#define JarakMinimum                    35
#define JarakMaximum                    60

#define TidakadaObject                  -1
#define SuhuRendah                      0
#define SuhuTinggi                      1
#define SuhuDiterima                    2

#define SCAN_TidakadaObject             0
#define SCAN_BERHASIL                   1
#define SCAN_ERROR                      2

#define DISP_IP_INDEX                   0
#define DISP_JarakError_INDEX           1

LiquidCrystal_I2C lcd(0x27,16,2);  
Servo myServo;
Ultrasonic ultrasonic(Ultra_TriggerPin, Ultra_EchoPin);
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

char gszIP_Add[20];
unsigned short gusDeteksi_Masker = 0;
unsigned short gusLCD_Index = 0;
unsigned short gusDisp_Index = 0;
unsigned short gusIsNeedDisp = 1;
unsigned short gusIsNeed_Mengulang = 0;
unsigned short gusIsKirimRequest = 0;

unsigned long gulMulai_Timer_LCD = 0;
unsigned long gulMengulang_Timer = 0;
unsigned long gulJarak_Timer = 0;

void setup()
{
  char szData[30];
  unsigned short usExit = 0;
  unsigned short usData_Len = 0;
  short a = 0;
  
  Serial.begin(9600);
  mlx.begin();
  
  lcd.init();                      
  lcd.backlight();
  
  pinMode(Pin_TekanButton,INPUT_PULLUP);
  pinMode(Led_Hijau,OUTPUT);
  pinMode(Led_Merah,OUTPUT);

  vKontrol_Led(SCAN_TidakadaObject);
  vKontrolServo(KunciPalang);
  
  lcd.clear();
  lcd.print("Wifi");
  lcd.setCursor(0,1);
  lcd.print("connecting...");

  memset(szData, '\0', sizeof(szData));
  memset(gszIP_Add, '\0', sizeof(gszIP_Add));
 
  do
  { 
    usData_Len = usBaca_DataSerial(szData, sizeof(szData));
    
    if(usData_Len > 0)
    {
      for(short i=0; i<usData_Len; i++)
      {
        if(szData[i] == '#')
        {
          i++;
          while(szData[i] != ',')
          {
            gszIP_Add[a] = szData[i++];
            a++;
          }
          usExit = 1;
          break;
        }
      }
    }
    else
    {
      if((millis() - gulMengulang_Timer) > Mengulang_TIME)
      {
        Serial.println(MengulangKembali_SCAN);
        gulMengulang_Timer = millis();
      }
    }
  }while(usExit == 0);

  vLCD_Disp_Ip(gszIP_Add);
  gusDisp_Index = DISP_IP_INDEX;
  gulMulai_Timer_LCD = millis();
}

void loop()
{   
  char szData[30];
  unsigned short usExit = 0;
  unsigned short usSuhuKeluar = 0;
  unsigned short usData_Len = 0;
  unsigned short usSuhuDiterima = TidakadaObject;
  unsigned short usIsButuhScanUlang = 0;
  unsigned short usWajah_Jarak = 0;
  unsigned short usStart_Scanning_Count = 0;
  unsigned short usJarakError = 0;
  unsigned short usError_Count = 0;
  short sPersentasiMasker = 0;
  float fSuhuObject = 0;
  
  while(usSuhuKeluar == 0)
  {
    fSuhuObject = mlx.readObjectTempC();

    if(fSuhuObject > SuhuDiterimaMax)
    {
     
      vKontrol_Led(SCAN_ERROR);
      usSuhuDiterima = SuhuTinggi;
      gusIsNeedDisp = 1;
    }
    else if(fSuhuObject > BatasSuhu)
    {
      vKontrol_Led(SCAN_BERHASIL);
      usSuhuDiterima = SuhuDiterima;

      gusIsNeedDisp = 1;
      usSuhuKeluar = 1;
    }
    else 
    {
      vKontrol_Led(SCAN_TidakadaObject);
      usSuhuDiterima = TidakadaObject;
    }
    
    if(gusIsNeedDisp == 1)
    {
      if(usSuhuDiterima != TidakadaObject)
      {
        vDisp_Sensor_Suhu(usSuhuDiterima, fSuhuObject);
      }
      else if(usSuhuDiterima == TidakadaObject)
      {
        vLCD_Disp_Ip(gszIP_Add);
        gusDisp_Index = DISP_IP_INDEX;
      }      
      gusIsNeedDisp = 0;
    }
    vLCD_Disp_Timer_Index();
    sBacButton();
  }
  
  delay(DELAY_TEMP_SUCCESS);
  vKontrol_Led(SCAN_TidakadaObject);
  
  while(usSuhuKeluar == 1)
  {
    usExit = 0;

    usWajah_Jarak = usRead_Jarak();

    if((usWajah_Jarak > JarakMinimum) && (usWajah_Jarak < JarakMaximum))
    {
      usStart_Scanning_Count++;

      vDisp_Scanning(); 
    }
    else
    {
      usStart_Scanning_Count = 0;
    }
    
    if(usStart_Scanning_Count > mulai_scan_COUNT)
    {
      //optimal Jarak to scan Wajah        
      memset(szData, '\0', sizeof(szData));   
         
      do
      {
        usWajah_Jarak = usRead_Jarak(); 
        
        if((usWajah_Jarak > JarakMinimum) && (usWajah_Jarak < JarakMaximum))
        { 
          if(gusIsKirimRequest == 0)
          {
            Serial.println(MulaiBerjalan);
            gusIsKirimRequest = 1;
          }
          
          usData_Len = usBaca_DataSerial(szData, sizeof(szData));
          if(usData_Len > 0)
          {
            if(szData[0] == '*')
            {
              sscanf(szData, "*%d,", &sPersentasiMasker);
              usIsButuhScanUlang = 0;
              gusIsKirimRequest = 0;
              usExit = 1;
            }
          }
        }
        else 
        {
          usJarakError++; 
        }   

        if(usJarakError > JarakError_COUNT)
        {
          usJarakError = 0;
          
          usIsButuhScanUlang = 1;
          usExit = 1;
        }
      }while(usExit == 0);
    

      if(usIsButuhScanUlang == 0)
      {
        vLCD_Disp_Status(sPersentasiMasker);
        
        if(sPersentasiMasker >= Deteksi_Masker_TH) 
        {
          usSuhuKeluar = 0;
          vKontrolPalang(ON);
        }
        else 
        {
          vKontrolPalang(OFF);
          usError_Count++;
          
          if(usError_Count >= ERROR_COUNT)
          {
            usSuhuKeluar = 0;
          }
        }
      }
      else if(usIsButuhScanUlang == 1)
      {
        usError_Count++;
        
        if(usError_Count >= ERROR_COUNT)
        {
          usSuhuKeluar = 0;
        }
        else 
        {
          short a = 0;
          vLCD_Disp_Error_Scan();
          vKontrol_Led(SCAN_ERROR);
          memset(szData, '\0', sizeof(szData));
          Serial.println(MengulangKembali_ESP32);
          gulMengulang_Timer = millis();
          
          do
          {
            usData_Len = usBaca_DataSerial(szData, sizeof(szData)); 
            if(usData_Len > 0)
            {
              gusIsKirimRequest = 0;
              for(short i=0; i<usData_Len; i++)
              {
                if(szData[i] == '#')
                {
                  i++;
                  while(szData[i] != ',')
                  {
                    gszIP_Add[a] = szData[i++];
                    a++;
                  }
                  usExit = 0;
                  break;
                }
              }
            }
            else
            {
              if((millis() - gulMengulang_Timer) > Mengulang_TIME)
              {
                Serial.println(MengulangKembali_SCAN);
                gulMengulang_Timer = millis();
              }
            }
          }while(usExit == 1);
        }
      }
    }
    else
    {
      vKontrol_Led(SCAN_TidakadaObject);
      
      if(gusIsNeedDisp == 1)
      {
        vLCD_Disp_Scan_Wajah(usWajah_Jarak);
        gusDisp_Index = DISP_JarakError_INDEX;
        gusIsNeedDisp = 0;
      }
    }
    vLCD_Disp_Timer_Index();
    sBacButton();
  }
}
void vDisp_Scanning()
{
  lcd.clear();
  lcd.print("SCANNING..."); 
  lcd.setCursor(0,1);
  lcd.print("Pls wait & hold.");
}

void vLCD_Disp_Status(short sPersentasiMasker)
{
  lcd.clear();
  lcd.print("Mask: ");
  lcd.print(sPersentasiMasker);
  lcd.print("%");
  lcd.setCursor(0,1);

  if(sPersentasiMasker >= Deteksi_Masker_TH)
  {
    lcd.print("Enter Allowed.");
  }
  else
  {
    lcd.print("PLEASE WEAR MASK");
  }
}
void vDisp_Sensor_Suhu(short usSuhuDiterima, float fSuhuObject)
{
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Body Temp: ");
  lcd.print(fSuhuObject);
  lcd.print(char(223));
  lcd.print("C");
  lcd.setCursor(0,1);
  
  if(usSuhuDiterima == SuhuDiterima)
  {
    lcd.print("Body Temp OK");
  }
  else if(usSuhuDiterima == SuhuTinggi)
  {
    lcd.print("Body Temp HIGH");
  }
  else if(usSuhuDiterima == SuhuRendah)
  {
    lcd.print("Body Temp LOW");
  }
}
void vLCD_Disp_Ip(char *szIp)
{
  lcd.clear();
  
  if(gusLCD_Index == 0)
  {
    lcd.setCursor(2,0);
    lcd.print("PLEASE SCAN");
    lcd.setCursor(2,1);
    lcd.print("TEMPERATURE");
  }  
  else
  { 
    lcd.setCursor(1,0);
    lcd.print("CONNECTED TO:");
    lcd.setCursor(1,1);
    lcd.print(szIp);
  }
}
void vLCD_Disp_Scan_Wajah(unsigned short usWajah_Jarak)
{
  lcd.clear();
  if(gusLCD_Index == 0)
  {
    lcd.setCursor(0,0);
    lcd.print("Pls Scan ur Wajah");
    lcd.setCursor(0,1);
    lcd.print(JarakMinimum);
    lcd.print("-");
    lcd.print(JarakMaximum);
    lcd.print("cm from cam");
  }
  else if(gusLCD_Index == 1)
  {
    lcd.setCursor(0,0);
    lcd.print("youre ");
    lcd.print(usWajah_Jarak);
    lcd.print("cm away");
  
    lcd.setCursor(0,1);
    if(usWajah_Jarak > JarakMaximum)
    {
      lcd.print("Move Closer.. ");
    }
    else if(usWajah_Jarak < JarakMinimum)
    {
      lcd.print("Take a step back..");
    }
  }
}
void vDisp_Wajah_JarakError(unsigned short usWajah_Jarak)
{
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("youre ");
  lcd.print(usWajah_Jarak);
  lcd.print("cm away");

  lcd.setCursor(0,1);
  if(usWajah_Jarak > JarakMaximum)
  {
    lcd.print("Move Closer.. ");
  }
  else if(usWajah_Jarak < JarakMinimum)
  {
    lcd.print("Take a step back..");
  }
}
void vLCD_Disp_Error_Scan()
{
  lcd.clear();
  lcd.print("Take a step back"); 
  lcd.setCursor(0,1);
  lcd.print("and rescan again");
}
void vKontrol_Led(short sScan_Status)
{
  if(sScan_Status == SCAN_BERHASIL)
  {
    digitalWrite(Led_Hijau,HIGH);
    digitalWrite(Led_Merah,LOW);
  }
  else if(sScan_Status == SCAN_ERROR)
  {
    digitalWrite(Led_Hijau,LOW);
    digitalWrite(Led_Merah,HIGH);
  }
  else 
  {
    digitalWrite(Led_Hijau,LOW);
    digitalWrite(Led_Merah,LOW);
  }
}
void vLCD_Disp_Timer_Index()
{
  if((millis() - gulMulai_Timer_LCD) >= 1000)
  {
    gusLCD_Index++;

    if(gusDisp_Index == DISP_IP_INDEX)
    {
      if(gusLCD_Index > 1)
      {
        gusLCD_Index = 0;
      }
    }
    else if(gusDisp_Index == DISP_JarakError_INDEX)
    {
      if(gusLCD_Index > 1)
      {
        gusLCD_Index = 0;
      }
    }

    gusIsNeedDisp = 1;
    gulMulai_Timer_LCD = millis();
  }
}
short sBacButton()
{
  short sBut_Status = 0;

  sBut_Status = digitalRead(Pin_TekanButton);

  if(sBut_Status == HIGH)
  {
    vKontrolPalang(ON);
    
    gusLCD_Index = 0;
    gulMulai_Timer_LCD = millis();
    gusIsNeedDisp = 1;
  }
}
void vKontrolPalang(short sOnOff)
{
  if(sOnOff == ON)
  {
    vKontrol_Led(SCAN_BERHASIL);
    vKontrolServo(BukaPalang);
    delay(BukaPalang_TIME);
    
    vKontrolServo(TutupPalang);
    delay(TutupPalang_TIME);
  }
  else
  {
    vKontrolServo(KunciPalang);
    vKontrol_Led(SCAN_ERROR);
    delay(BukaPalang_TIME);
  }
}
unsigned short usRead_Jarak()
{
  unsigned short usWajah_Jarak = 0;
  
  usWajah_Jarak = ultrasonic.read();
  delay(50);
  
  return usWajah_Jarak;
}
//CONTROL SERVO BASED ON RESULTS
void vKontrolServo(int sDoor_Status)
{
  myServo.attach(Pin_Servo);
  
  if(sDoor_Status == BukaPalang) //open door
  {
    for(int i = TutupPalang; i > BukaPalang; i--)
    {
      myServo.write(i);
      delay(20);
    }
  }
  else if(sDoor_Status == TutupPalang)
  {
    for(int i = BukaPalang; i < TutupPalang; i++)
    {
      myServo.write(i);
      delay(20);
    }
  }
  else if(sDoor_Status == KunciPalang)
  {
    myServo.write(TutupPalang);
  }

  myServo.detach();
}
//READ SERIAL DATA FROM ESP32-CAM
unsigned short usBaca_DataSerial(char *szData, short sUkuranData)
{
  short i=0;
  
  if(Serial.available())
  {
    *(szData+i) = Serial.read();
    i++;
    delay(2);

    while(Serial.available())
    {
      *(szData+i) = Serial.read();
      i++;
      
      if(i >= sUkuranData)
      {
        break;  
      }
      delay(2);      
    }
  }
  
  Serial.flush();

  return i;
}
