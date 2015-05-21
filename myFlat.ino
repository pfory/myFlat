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
unsigned long consumption=0;
unsigned int cycles=0;

#define STATUS_LED 3

#include <Ethernet.h>
#include <Xively.h>
#include <XivelyClient.h>
#include <XivelyDatastream.h>
#include <XivelyFeed.h>
#include <Time.h> 
#include <HttpClient.h>
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; 
EthernetClient client;
//char server[] = "api.cosm.com";   // name address for cosm API
//IPAddress ip(192,168,2,100);

//#define dataServer
#ifdef dataServer
EthernetServer server(80);
#endif
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

#define watchdog
#ifdef watchdog
#include <avr/wdt.h>
#endif

//----------display
//#define display
#ifdef display
#include <IIC_without_ACK.h>
#include "oledfont.c"   //codetab

#define OLED_SDA 8
#define OLED_SCL 9

IIC_without_ACK lucky(OLED_SDA, OLED_SCL);
#endif

float versionSW=0.25;
char versionSWString[] = "myFlat v"; //SW name & version

byte status=0;

unsigned int const SERIAL_SPEED=9600;

//#define verbose

//------------------------------------------------------------- S E T U P -------------------------------------------------------
void setup() {
  blik(1, 500);
#ifdef watchdog
	wdt_enable(WDTO_8S);
#endif
#ifdef display
  lucky.Initial();
  delay(10);
  lucky.Fill_Screen(0x00);
  lucky.Char_F6x8(0,2,"myFlat");
#endif
  Serial.begin(SERIAL_SPEED);
  //Serial.print(versionSWString);
  Serial.println(versionSW);
  pinMode(STATUS_LED, OUTPUT);      // sets the digital pin as output
  Ethernet.begin(mac);
  blik(1, 100);
#ifdef verbose
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
#endif
#ifdef watchdog
	wdt_reset();
#endif  
  
#ifdef dataServer
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
#endif  
  int ret = xivelyclient.get(feed, xivelyKey);
  Serial.println(ret);
  if (ret > 0) {
		pulseTotal = datastreams[2].getFloat()*1000;
		pulseDay = datastreams[4].getFloat()*1000;
		pulseHour = datastreams[3].getFloat()*1000;
#ifdef verbose
		Serial.print("Nacteno Energy:");
		Serial.print(pulseTotal);
		Serial.println("Wh");
		Serial.print("Nacteno EnergyDay:");
		Serial.print(pulseDay);
		Serial.println("Wh");
		Serial.print("Nacteno EnergyHour:");
		Serial.print(pulseHour);
		Serial.println("Wh");
#endif
  }
  datastreams[0].setFloat(versionSW);  
#ifdef watchdog
	wdt_reset();
#endif  

  lastSendTime = millis();
  Udp.begin(localPort);
  Serial.print("waiting 20s for time sync...");
  setSyncProvider(getNtpTime);

  unsigned long lastSetTime=millis();
  while(timeStatus()==timeNotSet && millis()<lastSetTime+20000); // wait until the time is set by the sync provider, timeout 20sec
  Serial.println("Time sync interval is set to 3600 second.");
  setSyncInterval(3600); //sync each 1 hour
#ifdef verbose
  Serial.print("Now is ");
  printDateTime();
  Serial.println(" CET.");
#endif
  pinMode(counterPin, INPUT);      
  attachInterrupt(counterInterrupt, counterISR, CHANGE);
  
  blik(10, 20);
}

//------------------------------------------------------------ L O O P -----------------------------------------------------------------------
void loop() {
#ifdef watchdog
	wdt_reset();
#endif  
#ifdef dataServer
  client = server.available();
  if (client) {
    showStatus();
  }
#endif
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
    //Serial.println("Start pulse");
  } else {
    digitalWrite(STATUS_LED,LOW);
    pulseLength = millis()-startPulse;
    //Serial.println("End pulse");
    //Serial.print("Pulse length:");
    //Serial.println(pulseLength);
    if ((pulseLength)>35 && (pulseLength)<100) {
      pulseCount++;
      if (lastPulse>0) {
        consumption+=3600000/(millis()-lastPulse);
        cycles++;
#ifdef verbose
        Serial.print("Prikon:");
        Serial.print(3600000/(millis()-lastPulse));
        Serial.println(" W");
#endif
#ifdef display
        lucky.Char_F6x8(0,2,"Prikon:");
#endif
      }
      lastPulse=millis();
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
  if (cycles>0) {
    datastreams[5].setInt(consumption/cycles); //spotreba W
  } else {
      datastreams[5].setInt(0); //spotreba W
  }
#ifdef verbose
  Serial.print("Uploading data to Xively ");
  printDateTime();
#endif
#ifdef watchdog
	wdt_disable();
#endif

  int ret = xivelyclient.put(feed, xivelyKey);
  
  if (ret==200) {
    if (status==0) status=1; else status=0;
    /*Serial.print("pulseTotal:");
    Serial.println(pulseTotal);
    Serial.print("pulseHour:");
    Serial.println(pulseHour);
    Serial.print("pulseDay:");
    Serial.println(pulseDay);*/
    if (cycles>0) {
      cycles=0;
      consumption=0;
    }
    if (minute()==0) {
      pulseHour=0;
    }
    if (minute()==5 && hour()==0) {
      pulseDay=0;
    }
#ifdef verbose
    Serial.print(" OK:");
#endif
	} else {
#ifdef verbose
    Serial.print(" ERR: ");
#endif
  }
#ifdef verbose
  Serial.println(ret);
#endif
  
#ifdef watchdog
	wdt_enable(WDTO_8S);
#endif
  //lastSendTime = millis();
}

float Wh2kWh(unsigned long Wh) {
  return (float)Wh/1000.f;
}

#ifdef verbose
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
#endif

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

#ifdef dataServer
void showStatus() {
  Serial.println("new client");
  // an http request ends with a blank line
  boolean currentLineIsBlank = true;
  while (client.connected()) {
    if (client.available()) {
      char c = client.read();
      Serial.write(c);
      // if you've gotten to the end of the line (received a newline
      // character) and the line is blank, the http request has ended,
      // so you can send a reply
      if (c == '\n' && currentLineIsBlank) {
        // send a standard http response header
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println("Connection: close");  // the connection will be closed after completion of the response
        //client.println("Refresh: 5");  // refresh the page automatically every 5 sec
        client.println();
        client.println("<!DOCTYPE HTML>");
        client.println("<html>");
        // output the value of each analog input pin
        client.print(versionSWString);
        client.print(versionSW);
        client.println("</html>");
        break;
      }
      if (c == '\n') {
        // you're starting a new line
        currentLineIsBlank = true;
      }
      else if (c != '\r') {
        // you've gotten a character on the current line
        currentLineIsBlank = false;
      }
    }
  }
  // give the web browser time to receive the data
  delay(1);
  // close the connection:
  client.stop();
  Serial.println("client disconnected");
}
#endif

void blik(byte count, unsigned int del) {
  for (int i=0; i<count; i++) {
    delay(del);
    digitalWrite(STATUS_LED,HIGH);
    delay(del);
    digitalWrite(STATUS_LED,LOW);
  }
}