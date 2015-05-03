//Elektroměr DDS-1Y-18L digitální, jednofázový 1 impuls = 1Wh

const byte counterPin = 2; 
const byte counterInterrupt = 0; // = pin D2
unsigned long startPulse=0;
unsigned int pulseLength=0;
unsigned int pulseCount=0;
unsigned long pulseTotal=0;
unsigned long pulseHour=0;
unsigned long pulseDay=0;
unsigned long lastSendTime;
unsigned long sendInterval=60000; //ms
unsigned long lastPulse=0;
unsigned int consumption=0;

#define STATUS_LED 13

//#include <SPI.h>
//#include <Dhcp.h>
//#include <Dns.h>
#include <Ethernet.h>
#include <EthernetClient.h>
//#include <EthernetServer.h>
#include <EthernetUdp.h>
#include <Xively.h>
#include <XivelyClient.h>
#include <XivelyDatastream.h>
#include <XivelyFeed.h>
#include <Time.h> 
#include <HttpClient.h>
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; 
EthernetClient client;
char server[] = "api.cosm.com";   // name address for cosm API
bool checkConfigFlag = false;
//IPAddress ip(192,168,2,55);

char xivelyKey[] 			= "nJklwM9ts0HnRj3FbD6x2lA8CLOx8MADYx1WGGUSom9DRk9C";

#define xivelyFeed 				711798850
//https://personal.xively.com/feeds/711798850

char VersionID[]	 	    = "V";
char StatusID[]	 	      = "H";
char EnergyID[]	        = "Energy";
char EnergyHourID[]	    = "EnergyHour";
char EnergyDayID[]	    = "EnergyDay";
char ConsumptionID[]	  = "Consumption";


XivelyDatastream datastreams[] = {
	XivelyDatastream(VersionID, 		    strlen(VersionID), 	      DATASTREAM_FLOAT),
	XivelyDatastream(StatusID, 		      strlen(StatusID), 		    DATASTREAM_INT),
	XivelyDatastream(EnergyID, 		      strlen(EnergyID), 		    DATASTREAM_FLOAT),
	XivelyDatastream(EnergyHourID, 		  strlen(EnergyHourID), 		DATASTREAM_FLOAT),
	XivelyDatastream(EnergyDayID, 		  strlen(EnergyDayID), 		  DATASTREAM_FLOAT),
	XivelyDatastream(ConsumptionID, 		strlen(ConsumptionID), 		DATASTREAM_INT)
};

XivelyFeed feed(xivelyFeed, 			datastreams, 			6);
XivelyClient xivelyclient(client);

EthernetUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets
unsigned long getNtpTime();
void sendNTPpacket(IPAddress &address);
IPAddress timeServer(132, 163, 4, 101); // time-a.timefreq.bldrdoc.gov
// IPAddress timeServer(132, 163, 4, 102); // time-b.timefreq.bldrdoc.gov
// IPAddress timeServer(132, 163, 4, 103); // time-c.timefreq.bldrdoc.gov
const int timeZone = 1;     // Central European Time
#define DATE_DELIMITER "."
#define TIME_DELIMITER ":"
#define DATE_TIME_DELIMITER " "

//----------display
#include <Adafruit_GFX.h>
#include <IIC_without_ACK.h>
#include "oledfont.c"   //codetab

#define OLED_SDA 8
#define OLED_SCL 9

IIC_without_ACK lucky(OLED_SDA, OLED_SCL);//9 -- sda,10 -- scl


float versionSW=0.21;
char versionSWString[] = "myFlat v"; //SW name & version

byte status=0;

unsigned int const SERIAL_SPEED=9600;

//------------------------------------------------------------- S E T U P -------------------------------------------------------
void setup() {
  lucky.Initial();
  delay(10);
  lucky.Fill_Screen(0x00);

  Serial.begin(SERIAL_SPEED);
  Serial.println(versionSW);
  datastreams[0].setFloat(versionSW);  
  pinMode(counterPin, INPUT);      
  attachInterrupt(counterInterrupt, counterISR, CHANGE);
  pinMode(STATUS_LED, OUTPUT);      // sets the digital pin as output
  //Ethernet.begin(mac, ip);
  Ethernet.begin(mac);
  Serial.println("EthOK");
  Serial.print("\nIP:");
  Serial.println(Ethernet.localIP());
  Serial.print("Mask:");
  Serial.println(Ethernet.subnetMask());
  Serial.print("Gateway:");
  Serial.println(Ethernet.gatewayIP());
  Serial.print("DNS:");
  Serial.println(Ethernet.dnsServerIP());
  Serial.println();
  
  int ret = xivelyclient.get(feed, xivelyKey);
  Serial.println(ret);
  if (ret > 0) {
		pulseTotal = datastreams[2].getFloat()*1000;
		Serial.print("Nacteno Energy:");
		Serial.print(pulseTotal);
		Serial.println("Wh");
  }
  
  lastSendTime = millis();
  Udp.begin(localPort);
  Serial.print("waiting 20s for time sync...");
  setSyncProvider(getNtpTime);

  unsigned long lastSetTime=millis();
  while(timeStatus()==timeNotSet && millis()<lastSetTime+20000); // wait until the time is set by the sync provider, timeout 20sec
  Serial.println("Time sync interval is set to 3600 second.");
  setSyncInterval(3600); //sync each 1 hour
  
  Serial.print("Now is ");
  printDateTime();
  Serial.println(" CET.");
}

//------------------------------------------------------------ L O O P -----------------------------------------------------------------------
void loop() {
  if(!client.connected() && (millis() - lastSendTime > sendInterval)) {
    lastSendTime = millis();
    sendData();
  }
}

//------------------------------------------------------------ F U N C T I O N S --------------------------------------------------------------
void counterISR() { 
  if (digitalRead(counterPin)==HIGH) {
    digitalWrite(STATUS_LED,HIGH);
    startPulse=millis();
    Serial.println("Start pulse");
  } else {
    digitalWrite(STATUS_LED,LOW);
    pulseLength = millis()-startPulse;
    Serial.print("End pulse. Pulse length:");
    Serial.println(pulseLength);
    if ((pulseLength)>35 && (pulseLength)<100) {
      pulseCount++;
      if (lastPulse>0) {
        consumption=3600000/millis()-lastPulse;
        lastPulse=millis();
      }
    }
  }
}


void sendData() {
  datastreams[1].setInt(status);  
  pulseTotal+=pulseCount;
  pulseHour+=pulseCount;
  pulseDay+=pulseCount;
  pulseCount=0;
  datastreams[2].setFloat(Wh2kWh(pulseTotal));  //kWh
  datastreams[3].setFloat(Wh2kWh(pulseHour)); //kWh/hod
  datastreams[4].setFloat(Wh2kWh(pulseDay)); //kWh/den
  datastreams[5].setInt(consumption); //spotreba W
//#ifdef verbose
  Serial.println("Uploading data to Xively");
//#endif
#ifdef watchdog
	wdt_disable();
#endif

  int ret = xivelyclient.put(feed, xivelyKey);
  
  if (ret==200) {
    if (status==0) status=1; else status=0;
    Serial.print("pulseTotal:");
    Serial.println(pulseTotal);
    Serial.print("pulseHour:");
    Serial.println(pulseHour);
    Serial.print("pulseDay:");
    Serial.println(pulseDay);
    if (minute()==0) {
      pulseHour=0;
    }
    if (minute()==5 && hour()==0) {
      pulseDay=0;
    }
    Serial.print("Xively OK:");
	} else {
  //#ifdef verbose
    Serial.print("Xively err: ");
  //#endif
  }
  Serial.println(ret);
  
#ifdef watchdog
	wdt_enable(WDTO_8S);
#endif
  //lastSendTime = millis();
}

float Wh2kWh(unsigned long Wh) {
  return (float)Wh/1000.f;
}

void printDateTime() {
	Serial.print(day());
	Serial.print(DATE_DELIMITER);
	Serial.print(month());
	Serial.print(DATE_DELIMITER);
	Serial.print(year());
	Serial.print(DATE_TIME_DELIMITER);
	printDigits(hour());
	Serial.print(TIME_DELIMITER);
	printDigits(minute());
	Serial.print(TIME_DELIMITER);
	printDigits(second());
}
void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  if(digits < 10) {
    Serial.print('0');
  }
  Serial.print(digits);
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

unsigned long getNtpTime() {
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                 
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}



