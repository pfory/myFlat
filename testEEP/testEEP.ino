#include <EEPROM.h>

unsigned int   EEPROMaddress = 0;
unsigned long writesToEEPROM=999;
unsigned long pulseTotal=3926277;

char inData[10];
int index;
boolean started = false;
boolean ended = false;

void setup() {
  Serial.begin(9600);
  saveDataToEEPROM();
  Serial.println(pulseTotal);
  Serial.println(readPulsesCountFromEEPROM());
  Serial.println(readWritesCountFromEEPROM());
}

void loop() {
  while(Serial.available() > 0)
  {
      char aChar = Serial.read();
      if(aChar == '>' && started == false)
      {
          Serial.println("start");
          started = true;
          index = 0;
          inData[index] = '\0';
      }
      else if(aChar == '>')
      {
          ended = true;
          Serial.println("end");
      }
      else if(started)
      {
          inData[index] = aChar;
          index++;
          inData[index] = '\0';
      }
  }

  if(started && ended)
  {
      // Convert the string to an integer
      Serial.println(inData);
      int inInt = atoi(inData);

      // Use the value

      // Get ready for the next time
      started = false;
      ended = false;

      index = 0;
      inData[index] = '\0';
      Serial.println(inInt);
  }
}

// b1 b2 b3 b4   //Writes
// 0  1  2  3    //address
unsigned long readPulsesCountFromEEPROM() {
  byte b4=EEPROM.read(EEPROMaddress+7);
  byte b3=EEPROM.read(EEPROMaddress+6);
  byte b2=EEPROM.read(EEPROMaddress+5);
  byte b1=EEPROM.read(EEPROMaddress+4);
  Serial.println(b1);
  Serial.println(b2);
  Serial.println(b3);
  Serial.println(b4);
  
  return (b1<<24)|(b2<<16)|(b3<<8)|b4;
}

// b1 b2 b3 b4   //Pulses
// 4  5  6  7    //address
unsigned long readWritesCountFromEEPROM() {
  byte b4=EEPROM.read(EEPROMaddress+3);
  byte b3=EEPROM.read(EEPROMaddress+2);
  byte b2=EEPROM.read(EEPROMaddress+1);
  byte b1=EEPROM.read(EEPROMaddress);
  
  return b1<<24|b2<<16|b3<<8|b4;
}


void saveDataToEEPROM() {
  unsigned long m=0xFF;
  writesToEEPROM++;
  m=0xFF000000;
  byte b1=(byte)((writesToEEPROM&m)>>24);
  EEPROM.write(EEPROMaddress, b1); 
  m=0x00FF0000;
  b1=(byte)((writesToEEPROM&m)>>16);
  EEPROM.write(EEPROMaddress+1, b1); 
  m=0x0000FF00;
  b1=(byte)((writesToEEPROM&m)>>8);
  EEPROM.write(EEPROMaddress+2, b1); 
  m=0x000000FF;
  b1=(byte)(writesToEEPROM&m);
  EEPROM.write(EEPROMaddress+3, b1); 

  m=0xFF000000;
  b1=(byte)((pulseTotal&m)>>24);
  EEPROM.write(EEPROMaddress+4, b1); 
  Serial.println(b1);
  m=0x00FF0000;
  b1=(byte)((pulseTotal&m)>>16);
  EEPROM.write(EEPROMaddress+5, b1); 
  Serial.println(b1);
  m=0x0000FF00;
  b1=(byte)((pulseTotal&m)>>8);
  EEPROM.write(EEPROMaddress+6, b1); 
  Serial.println(b1);
  m=0x000000FF;
  b1=(byte)(pulseTotal&m);
  EEPROM.write(EEPROMaddress+7, b1); 
  Serial.println(b1);
}
