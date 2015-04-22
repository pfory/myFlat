const byte counterPin = 2; 
const byte counterInterrupt = 0; // = pin D2
unsigned long startPulse=0;
unsigned int pulseLength=0;
unsigned int pulseCount=0;
//unsigned long lastSendTime;

#define Ethernetdef

#ifdef Ethernetdef
#include <SPI.h>
#include <Dhcp.h>
#include <Dns.h>
#include <Ethernet.h>
#include <EthernetClient.h>
#include <EthernetServer.h>
#include <EthernetUdp.h>
#include <Xively.h>
#include <XivelyClient.h>
#include <XivelyDatastream.h>
#include <XivelyFeed.h>
byte mac[] = { 0x00, 0xE0, 0x07D, 0xCE, 0xC6, 0x6F };
EthernetClient client;
char server[] = "api.cosm.com";   // name address for cosm API
bool checkConfigFlag = false;
IPAddress ip(192,168,1,55);

//XIVELY
#include <Xively.h>
#include <HttpClient.h>

char xivelyKey[] 			= "nJklwM9ts0HnRj3FbD6x2lA8CLOx8MADYx1WGGUSom9DRk9C";

#define xivelyFeed 				711798850
//https://personal.xively.com/feeds/711798850

char VersionID[]	 	    = "V";
char StatusID[]	 	      = "H";
char EnergyID[]	        = "Energy";


XivelyDatastream datastreams[] = {
	XivelyDatastream(VersionID, 		    strlen(VersionID), 	      DATASTREAM_FLOAT),
	XivelyDatastream(StatusID, 		      strlen(StatusID), 		    DATASTREAM_INT),
	XivelyDatastream(EnergyID, 		      strlen(EnergyID), 		      DATASTREAM_INT)
};

XivelyFeed feed(xivelyFeed, 			datastreams, 			3);
XivelyClient xivelyclient(client);
#endif

float versionSW=0.1;
char versionSWString[] = "myFlat v"; //SW name & version

byte status=0;


void setup() {
  pinMode(counterPin, INPUT);      
  attachInterrupt(counterInterrupt, counterISR, CHANGE);
}

void loop() {
#ifdef Ethernetdef
  sendData();
#endif
}


void counterISR() { 
  if (digitalRead(counterPin)==LOW) {
    startPulse=millis();
  } else {
    pulseLength = millis()-startPulse;
    if ((pulseLength)>35 && (pulseLength)<80) {
      pulseCount++;
    }
  }
}


#ifdef Ethernetdef
void sendData() {
  datastreams[0].setFloat(versionSW);  
  datastreams[1].setInt(status);  
  datastreams[2].setInt(pulseCount);  

//#ifdef verbose
  Serial.println("Uploading data to Xively");
//#endif
#ifdef watchdog
	wdt_disable();
#endif

  int ret = xivelyclient.put(feed, xivelyKey);
  
  if (ret==200) {
    if (status==0) status=1; else status=0;
    pulseCount++;
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
#endif
