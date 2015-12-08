#include <Xively.h>
#include <XivelyClient.h>
#include <XivelyDatastream.h>
#include <XivelyFeed.h>
#include <HttpClient.h>
#include <Time.h> 
#include <MQTT.h>
#include <EEPROM.h>

extern "C" {
#include "user_interface.h"
}

#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS_IN        2
#define COUNTER_PIN           13
//#define STATUS_LED            15

//#define xively
#define IoT

OneWire oneWireIN(ONE_WIRE_BUS_IN);
DallasTemperature sensorsIN(&oneWireIN);

float                 tempIN        = 0;

#include <ESP8266WiFi.h>
WiFiClient client;

#ifdef xively
char xivelyKey[] 			= "VgoUxsEYlsN65fvTsBh8Sp9919cpMmv1cI2MoOzgHPnNADMa";
#define xivelyFeed 				395999474

char VersionID[]	 	    = "V";
char StatusID[]	 	      = "H";
char EnergyID[]	        = "Energy";
char EnergyHourID[]	    = "EnergyHour";
char EnergyDayID[]	    = "EnergyDay";
char ConsumptionID[]	  = "Consumption";
char TempINID[]	        = "TemperatureIN";


XivelyDatastream datastreams[] = {
	XivelyDatastream(VersionID, 		    strlen(VersionID), 	      DATASTREAM_FLOAT),
	XivelyDatastream(StatusID, 		      strlen(StatusID), 		    DATASTREAM_INT),
	XivelyDatastream(EnergyID, 		      strlen(EnergyID), 		    DATASTREAM_FLOAT),
	XivelyDatastream(EnergyHourID, 		  strlen(EnergyHourID), 		DATASTREAM_FLOAT),
	XivelyDatastream(EnergyDayID, 		  strlen(EnergyDayID), 		  DATASTREAM_FLOAT),
	XivelyDatastream(ConsumptionID, 		strlen(ConsumptionID), 		DATASTREAM_INT),
	XivelyDatastream(TempINID, 		      strlen(TempINID), 		    DATASTREAM_FLOAT)
};

XivelyFeed feed(xivelyFeed, 			datastreams, 			7);
XivelyClient xivelyclient(client);
#endif

const char* ssid     = "Datlovo";
const char* password = "Nu6kMABmseYwbCoJ7LyG";

byte status=0;
unsigned long       lastSendTime      = 0;
const unsigned long sendInterval      = 20000; //ms
volatile unsigned long       startPulse        = 0;
volatile unsigned int        pulseLength       = 0;
volatile unsigned int        pulseCount        = 0;
unsigned long                pulseTotal        = 0;
unsigned long                pulseHour         = 0;
unsigned long                pulseDay          = 0;
unsigned long                lastPulse         = 0;
unsigned long                consumption       = 0;
unsigned long                cycles            = 0;
bool                         saveEEPROMEnergy  = false;
#ifdef IoT
#define AP_SSID "Datlovo"
#define AP_PASSWORD "Nu6kMABmseYwbCoJ7LyG"

#define EIOTCLOUD_USERNAME "datel"
#define EIOTCLOUD_PASSWORD "mrdatel"

// create MQTT object
#define EIOT_CLOUD_ADDRESS        "cloud.iot-playground.com"
MQTT myMqtt("", EIOT_CLOUD_ADDRESS, 1883);

String instanceId                   = "564241f9cf045c757f7e6301";
String valueStr("");
boolean result;
String topic("");
bool stepOk                         = false;

#define CONFIG_START 0
#define CONFIG_VERSION "v01"

struct StoreStruct {
  // This is for mere detection if they are your settings
  char    version[4];
  uint    moduleId;  // module id
  float   energy;
} storage = {
  CONFIG_VERSION,
  3,
  3620.33,
};

#endif

#include <WiFiUdp.h>
unsigned int localPort = 8888;      // local port to listen for UDP packets

/* Don't hardwire the IP address or we won't get the benefits of the pool.
 *  Lookup the IP address for the host name instead */
//IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.windows.com";
const int timeZone = 1;     // Central European Time

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;
unsigned long lastSetTime;
#define DATE_DELIMITER "."
#define TIME_DELIMITER ":"
#define DATE_TIME_DELIMITER " "


void setup() {
  Serial.begin(115200);
  //pinMode(STATUS_LED, OUTPUT);      // sets the digital pin as output
  //blik(5, 50);
  // We start by connecting to a WiFi network
  EEPROM.begin(512);
  loadConfig();

  Serial.println();
  Serial.println();
  
  wifiConnect();
  WiFi.printDiag(Serial);

  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());


  //lastSendTime = millis();
  
  Serial.println("waiting 20s for time sync...");
  setSyncProvider(getNtpTime);
  while(timeStatus()==timeNotSet && millis()<lastSetTime+20000); // wait until the time is set by the sync provider, timeout 20sec
  Serial.println("Time sync interval is set to 3600 second.");
  setSyncInterval(3600); //sync each 1 hour
  Serial.print("Now is ");
  printDateTime();
  Serial.println(" CET.");
  lastSetTime=millis();

  sensorsIN.begin(); // IC Default 9 bit. If you have troubles consider upping it 12. Ups the delay giving the IC more time to process the temperature measurement
  sensorsIN.setResolution(12);
  sensorsIN.setWaitForConversion(false);

  Serial.print("Sensor(s) ");
  Serial.print(sensorsIN.getDeviceCount());
  Serial.print(" on bus IN - pin ");
  Serial.println(ONE_WIRE_BUS_IN);

  setMQTT();

  pinMode(COUNTER_PIN, INPUT);      
  attachInterrupt(COUNTER_PIN, counterISR, HIGH);
  
  //ESP.wdtEnable(WDTO_8S);
  //Serial.setDebugOutput(true);
 
  pulseTotal=storage.energy*1000.f;
  Serial.print("Energy from EEPROM: ");
  Serial.print(storage.energy);
  Serial.println(" kWh");
}

//------------------------------------------------------------ L O O P -----------------------------------------------------------------------
void loop() {
  //ESP.wdtFeed();
  
  if(!client.connected() && (millis() - lastSendTime > sendInterval)) {
    lastSendTime = millis();
    startMeas(); 
    delay(1000);
    //while (sensorsIN.getCheckForConversion()==false) {
      tempIN = sensorsIN.getTempCByIndex(0);
    //}
    displayTemp();
    #ifdef xively
    sendDataXively();
    #endif
    #ifdef IoT
    sendParamMQTT();
    #endif
  }
  if (saveEEPROMEnergy) {
    storage.energy=pulseCount/1000;
    Serial.print("Save energy to EEPROM: ");
    Serial.print(storage.energy);
    Serial.println(" kWh");
    saveConfig();
    saveEEPROMEnergy=false;
  }

}

//------------------------------------------------------------ F U N C T I O N S --------------------------------------------------------------
void counterISR() { 
  //Serial.println("PULSE");
  //if (digitalRead(COUNTER_PIN)==HIGH) {
    //digitalWrite(STATUS_LED,HIGH);
    //startPulse=millis();
    //Serial.println("Start pulse");
  //} else {
    //digitalWrite(STATUS_LED,LOW);
    //pulseLength = millis()-startPulse;
    //Serial.println("End pulse");
    //Serial.print("Pulse length:");
    //Serial.println(pulseLength);
    //if ((pulseLength)>35 && (pulseLength)<100) {
    pulseCount++;
    cycles++;
    if ((pulseCount%1000)==0) {
      saveEEPROMEnergy=true;
    }
    //}
  //}
}

void startMeas() {
  // call sensors.requestTemperatures() to issue a global temperature 
  // request to all devices on the bus
  Serial.print("Requesting temperatures...");
  sensorsIN.requestTemperatures(); // Send the command to get temperatures
  Serial.println("DONE");
}


void displayTemp() {
  Serial.print("Temp IN: ");
  Serial.print(tempIN); // Why "byIndex"? You can have more than one IC on the same bus. 0 refers to the first IC on the wire
  Serial.println();
}

#ifdef xively
void sendDataXively() {
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
  datastreams[6].setFloat(tempIN); //kWh/den
  Serial.print("Uploading data to Xively ");

  printDateTime();
  //ESP.wdtDisable();
  int ret = xivelyclient.put(feed, xivelyKey);
  //ESP.wdtEnable(WDTO_8S);
  Serial.println();
  printDateTime();
  if (ret==200) {
    if (status==0) status=1; else status=0;
    /*Serial.print("pulseTotal:");
    Serial.println(pulseTotal);
    Serial.print("pulseHour:");
    Serial.println(pulseHour);
    Serial.print("pulseDay:");
    Serial.println(pulseDay);*/
    Serial.print(" OK:");
	} else {
    Serial.print(" ERR: ");
  }
  Serial.println(ret);

}
#endif

float Wh2kWh(unsigned long Wh) {
  return (float)Wh/1000.f;
}

void wifiConnect() {
  Serial.print("Connecting to AP");
  WiFi.begin(AP_SSID, AP_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");  
}

void sendParamMQTT() {
  pulseTotal+=pulseCount;
  pulseHour+=pulseCount;
  pulseDay+=pulseCount;
  pulseCount=0;
  valueStr = String(tempIN);
  topic = "/Db/" + instanceId + "/3/Sensor.Parameter1";
  result = myMqtt.publish(topic, valueStr);
  valueStr = String(Wh2kWh(pulseTotal));
  topic = "/Db/" + instanceId + "/3/Sensor.Parameter2";
  result = myMqtt.publish(topic, valueStr);
  
  if (lastPulse>0) {
    if (cycles>0) {
      consumption=(3600000/(millis()-lastPulse)/cycles);
    } else {
      consumption = 0;
    }
    Serial.print("Prikon:");
    Serial.print(consumption);
    Serial.println(" W");
  }
  lastPulse=millis();
  
  Serial.print("Cycles:");
  Serial.print(cycles);
  Serial.print(" Cons:");
  Serial.println(consumption);
  
  valueStr = String(consumption);
  topic = "/Db/" + instanceId + "/3/Sensor.Parameter3";
  result = myMqtt.publish(topic, valueStr);
  consumption=0;
}


void myConnectedCb() {
  Serial.println("connected to MQTT server");
}

void myDisconnectedCb() {
  Serial.println("disconnected. try to reconnect...");
  delay(500);
  myMqtt.connect();
}

void myPublishedCb() {
  Serial.println(" - published.");
}

void myDataCb(String& topic, String& data) {  
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(data);

  if (topic == String("/Db/InstanceId"))
  {
    //instanceId = data;
    stepOk = true;
  }
  else if (topic ==  String("/Db/"+instanceId+"/NewModule"))
  {
    storage.moduleId = data.toInt();
    stepOk = true;
  }
  else if (topic == String("/Db/"+instanceId+"/"+String(storage.moduleId)+ "/Sensor.Parameter1/NewParameter"))
  {
    stepOk = true;
  }
  else if (topic == String("/Db/"+instanceId+"/"+String(storage.moduleId)+ "/Settings.Icon1/NewParameter"))
  {
    stepOk = true;
  } /*else if (topic == String("/Db/"+instanceId+"/3/Sensor.Parameter2")) {
    energy = data.toFloat();
    Serial.print("Energy = ");
    Serial.println(energy);
    //saveConfig();
  }*/
}

String macToStr(const uint8_t* mac) {
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}

void setMQTT() {
  String clientName;
  //clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);
  myMqtt.setClientId((char*) clientName.c_str());


  Serial.print("MQTT client id:");
  Serial.println(clientName);

  // setup callbacks
  myMqtt.onConnected(myConnectedCb);
  myMqtt.onDisconnected(myDisconnectedCb);
  myMqtt.onPublished(myPublishedCb);
  myMqtt.onData(myDataCb);
  
  //////Serial.println("connect mqtt...");
  myMqtt.setUserPwd(EIOTCLOUD_USERNAME, EIOTCLOUD_PASSWORD);  
  myMqtt.connect();

  delay(500);

  //get instance id
  //////Serial.println("suscribe: Db/InstanceId");
  myMqtt.subscribe("/Db/InstanceId");

  waitOk();

  Serial.print("ModuleId: ");
  Serial.println(storage.moduleId);

  /*
  Serial.print("Subscribe ");
  myMqtt.subscribe("/Db/"+instanceId+"/3/Sensor.Parameter2");
  Serial.println(" OK.");
  */

}

void waitOk()
{
  while(!stepOk)
    delay(100);
 
  stepOk = false;
}

void saveConfig() {
  for (unsigned int t=0; t<sizeof(storage); t++)
    EEPROM.write(CONFIG_START + t, *((char*)&storage + t));

  EEPROM.commit();
}


void blik(byte count, unsigned int del) {
  for (int i=0; i<count; i++) {
    delay(del);
    //digitalWrite(STATUS_LED,HIGH);
    delay(del);
    //digitalWrite(STATUS_LED,LOW);
  }
}

void loadConfig() {
  // To make sure there are settings, and they are YOURS!
  // If nothing is found it will use the default settings.
  if (EEPROM.read(CONFIG_START + 0) == CONFIG_VERSION[0] &&
      EEPROM.read(CONFIG_START + 1) == CONFIG_VERSION[1] &&
      EEPROM.read(CONFIG_START + 2) == CONFIG_VERSION[2])
    for (unsigned int t=0; t<sizeof(storage); t++)
      *((char*)&storage + t) = EEPROM.read(CONFIG_START + t);
}

/*-------- NTP code ----------*/
long getNtpTime() {
  WiFi.hostByName(ntpServerName, timeServerIP); 

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServerIP);
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
  Serial.println("sending NTP packet...");
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