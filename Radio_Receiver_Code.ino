//Define libraries
#include <Wire.h>            //Define I2C library
#include <Adafruit_VS1053.h> //MP3 library
#include <SD.h>
#include <SPI.h>

#define Si_ADDRESS 0x11      //I2C address
#define ATDD_POWER_DOWN 0x11 //power down address
#define ATDD_POWER_UP 0xE1   //power up address
#define ATDD_XOWAIT 0X40     //crystal stuff
#define RX_VOLUME 0x40       //volume command
#define ATDD_GET_STATUS 0xE0

//vars for receiver
#define INTpin 2
#define RESETpin 9
volatile byte IRQflag;
short band = 22;
byte status_rsp[4];     // holds response bytes following commands

void SiISR()
{
    IRQflag=1;
}

void SiReset()
{
  Serial.println("Reseting Si");
  // reset Si4844 and back up
  // switch I2C to 10kHz
  I2C_10kHz();
  Serial.println("1");

  IRQflag=0;
  Serial.println("2");

  digitalWrite(RESETpin,LOW);
  Serial.println("3");
  
  delayMicroseconds(200);
  Serial.println("4");
  
  digitalWrite(RESETpin,HIGH);
  Serial.println("5");
  
  delayMicroseconds(200);
  Serial.println("6");
  
  // wait for an IRQ rising edge
  while(!IRQflag);
  Serial.println("7");
  
  // wait for 2.5 ms
  delayMicroseconds(2500);
  Serial.println("8");
}

void SiGetStatus()
{
 
  // send a get status command and wait for 2 ms before testing response byte
  Serial.println("Getting status ");
  Wire.beginTransmission(Si_ADDRESS);
  Wire.write(ATDD_GET_STATUS);  
  Wire.endTransmission();  
  delayMicroseconds(2000);
  // test response byte
  Wire.requestFrom(Si_ADDRESS, 0x04);
  for(int x=0;x<4;x++)
  {
    status_rsp[x]=Wire.read();
  }
}

// NOTE: These speed routines for I2C are for an 8mhz, 3.3v Pro Mini
// They need to be changed for a 16 mHz UNO or other Arduinos
void I2C_10kHz()
{
// set I2C speed to 10Khz on an 8mHz chip
  TWBR=12.5;
  TWSR|=bit(TWPS0);
}

void I2C_50kHz()
{
  // set I2C speed to 50Khz on an 16mHz chip
  TWBR=2.5;
  TWSR|=bit(TWPS0);
} 

void get_status()
{
  // do SiGetStatus until we have valid frequency info in status_rsp
  // note: in testing we don't loop but it is a good practice
  // also, an error out routine can be added here so it can escape
  // after a certain number of attempts
  Waitforgo:               
      SiGetStatus(); 
      if( ((bitRead(status_rsp[0],4)==0) || ( (status_rsp[2]==0) && (status_rsp[3]==0))) )
        {
        goto Waitforgo;       
        }
}

void setup() {
  Serial.println("Setting up radio receiver ");
    //interruption pin
    pinMode(INTpin, INPUT);
  
    //reset pin
    pinMode(RESETpin, OUTPUT);
    digitalWrite(RESETpin, HIGH);
    attachInterrupt(0, SiISR, RISING);
    SiReset(); 
  
    get_status();
  
    Serial.println("Setup Complete");
}

void loop() {
  if(IRQflag)
   {
     Wire.beginTransmission(Si_ADDRESS);
     Wire.write(ATDD_GET_STATUS);  
     Wire.endTransmission();  
     Wire.requestFrom(Si_ADDRESS, 0x04);
     status_rsp[0]=Wire.read();
     status_rsp[1]=Wire.read();
     status_rsp[2]=Wire.read();
     status_rsp[3]=Wire.read();
     IRQflag=0;
   }

  // assuming CTS is ok to send, assume current_band is set
  // will do a power up AND a get status setting status_rsp[0-4]
  byte cb;
  cb=band;

  bitSet(cb,7);                // bit 7=1 for external crystal
  bitClear(cb,6);              // bit 6=0 for normal crystal time
                               // arg[2]-[7] are not used in this mode
  IRQflag=0;
  // send it
  Wire.beginTransmission(Si_ADDRESS);
  Wire.write(ATDD_POWER_UP);  
  Wire.write(cb);
  Wire.endTransmission();  
  // wait for an IRQ for tune wheel frequency ready
  delayMicroseconds(2500);
  while(!IRQflag);
  // wait for 2.5 ms because our IRQ is rising edge
  delayMicroseconds(2500);
  I2C_50kHz();
  // send a get status and wait for 2 ms before testing response byte
  get_status();
}
