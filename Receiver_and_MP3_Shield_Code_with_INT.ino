// Don't forget to copy the v44k1q05.img patch to the micro SD Card

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

#define RESET -1      // VS1053 reset pin (output)
#define CS 7        // VS1053 chip select pin (output)
#define DCS 6        // VS1053 Data/command select pin (output)
#define CARDCS 4     // Card chip select pin
#define DREQ 3       // VS1053 Data request, ideally an Interrupt pin

//vars for receiver
#define INTpin 0
#define RESETpin 1
volatile byte IRQflag;
short band = 22;
byte status_rsp[4];     // holds response bytes following commands

//vars for mp3 shield
Adafruit_VS1053_FilePlayer musicPlayer = Adafruit_VS1053_FilePlayer(RESET, CS, DCS, DREQ, CARDCS);

File recording;  // the file we will save our recording to
#define RECBUFFSIZE 128  // 64 or 128 bytes.
uint8_t recording_buffer[RECBUFFSIZE];
uint8_t isRecording = false;

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

void setupReceiver()
{
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

void setupShield(){

  {
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

  Serial.println("Adafruit VS1053 Ogg Recording Test");

  // initialise the music player
  if (!musicPlayer.begin()) {
    Serial.println("VS1053 not found");
    while (1);  // don't do anything more
  }

  musicPlayer.sineTest(0x44, 500);    // Make a tone to indicate VS1053 is working
 
  if (!SD.begin(CARDCS)) {
    Serial.println("SD failed, or not present");
    while (1);  // don't do anything more
  }
  Serial.println("SD OK!");
  
  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(10,10);
    
  // load plugin from SD card! We'll use mono 44.1KHz, high quality
  if (!musicPlayer.prepareRecordOgg("v44k1q05.img")) {
     Serial.println("Couldn't load plugin!");
     while (1);
     } 

}

void loopRadio()
{
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

uint16_t saveRecordedData(boolean isrecord) {
  uint16_t written = 0;
  
    // read how many words are waiting for us
  uint16_t wordswaiting = musicPlayer.recordedWordsWaiting();
  
  // try to process 256 words (512 bytes) at a time, for best speed
  while (wordswaiting > 256) {
    //Serial.print("Waiting: "); Serial.println(wordswaiting);
    // for example 128 bytes x 4 loops = 512 bytes
    for (int x=0; x < 512/RECBUFFSIZE; x++) {
      // fill the buffer!
      for (uint16_t addr=0; addr < RECBUFFSIZE; addr+=2) {
        uint16_t t = musicPlayer.recordedReadWord();
        //Serial.println(t, HEX);
        recording_buffer[addr] = t >> 8; 
        recording_buffer[addr+1] = t;
      }
      if (! recording.write(recording_buffer, RECBUFFSIZE)) {
            Serial.print("Couldn't write "); Serial.println(RECBUFFSIZE); 
            while (1);
      }
    }
    // flush 512 bytes at a time
    recording.flush();
    written += 256;
    wordswaiting -= 256;
  }
  
  wordswaiting = musicPlayer.recordedWordsWaiting();
  if (!isrecord) {
    Serial.print(wordswaiting); Serial.println(" remaining");
    // wrapping up the recording!
    uint16_t addr = 0;
    for (int x=0; x < wordswaiting-1; x++) {
      // fill the buffer!
      uint16_t t = musicPlayer.recordedReadWord();
      recording_buffer[addr] = t >> 8; 
      recording_buffer[addr+1] = t;
      if (addr > RECBUFFSIZE) {
          if (! recording.write(recording_buffer, RECBUFFSIZE)) {
                Serial.println("Couldn't write!");
                while (1);
          }
          recording.flush();
          addr = 0;
      }
    }
    if (addr != 0) {
      if (!recording.write(recording_buffer, addr)) {
        Serial.println("Couldn't write!"); while (1);
      }
      written += addr;
    }
    musicPlayer.sciRead(VS1053_SCI_AICTRL3);
    if (! (musicPlayer.sciRead(VS1053_SCI_AICTRL3) & _BV(2))) {
       recording.write(musicPlayer.recordedReadWord() & 0xFF);
       written++;
    }
    recording.flush();
  }

  return written;
}

void loopShield()
{
  if (!isRecording) {
    Serial.println("Begin recording");
    isRecording = true;
    
    // Check if the file exists already
    char filename[15];
    strcpy(filename, "RECORD00.OGG");
    for (uint8_t i = 0; i < 100; i++) {
      filename[6] = '0' + i/10;
      filename[7] = '0' + i%10;
      // create if does not exist, do not open existing, write, sync after write
      if (! SD.exists(filename)) {
        break;
      }
    }
    Serial.print("Recording to "); Serial.println(filename);
    recording = SD.open(filename, FILE_WRITE);
    if (! recording) {
       Serial.println("Couldn't open file to record!");
       while (1);
    }
    musicPlayer.startRecordOgg(false); // use microphone (for linein, pass in 'false')
  }

  //delay(300000); //temporary value for testing will fill in real value for launch and other tests
  while(true) {saveRecordedData(isRecording);}  
  /*Serial.println("End recording");
  musicPlayer.stopRecordOgg();
  isRecording = false;
  // flush all the data!
  saveRecordedData(isRecording);
  // close it up
  recording.close();
  delay(1000);*/
}

void setup() 
{
  Serial.begin(9600);
  //setupReceiver();
  setupShield();  
  //loopRadio();
  //loopShield();
}

void loop()
{
  loopRadio();
  loopShield();
}




