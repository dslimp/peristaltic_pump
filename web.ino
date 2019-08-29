#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <AccelStepper.h>
#include <WiFiUdp.h>
#include <ESP_EEPROM.h>
#include <FS.h>
#include <ESP8266HTTPClient.h>


HTTPClient http;

unsigned int localPort = 2390;

#define dirPin D4
#define stepPin D3
#define ssid      "dlink"     
#define password  "1122334455"  
#define PROCESSOFF 0
#define PROCESSPUMP 1
#define PROCESSDOZE 2
#define PROCESSCALIBRATE 3


AccelStepper stepper(1, stepPin, dirPin); 

IPAddress timeServerIP; 
const char* ntpServerName = "0.europe.pool.ntp.org";
const int NTP_PACKET_SIZE = 48; 
byte packetBuffer[NTP_PACKET_SIZE]; 
WiFiUDP udp;
int addr = 0;
int CurrentProcessProgress=0;
long StepToGo=0;

const uint8_t GPIOPIN[4] = {D5, D6, D7, D8}; // Led
float   t = 0 ;
float   h = 0 ;
float   p = 0;
byte Calibrate = 0;
long int WaterML = 0;

ESP8266WebServer server ( 80 );

struct SettingsStruct {
  unsigned long int EEPROMActivated;
  int MAXSTEPPERSECOND;
  int STEPML;
  int ACCELERATION;
  byte PUMPENABLED;
  int PUMPSPEEDL;  
  int LIMITCAPACITY;
  int LIMITHOUR;
  byte EXTERNALMAN;
  int BUILD;
  int WATERMLCALIBRATE;  
  byte ENABLELIMITCAPACITY;
  byte ENABLELIMITHOUR;
  byte CURRENTPROCESS;
  int LIMITDOZE;  
};

SettingsStruct Settings;

String getPage() {
   String page;
   File f = SPIFFS.open("/index.html", "r");
            while (f.available()){
            page += char(f.read());
          }
          f.close();   
  page.replace("$STEPML",String(Settings.STEPML));
  page.replace("$MAXSTEPPERSECOND",String(Settings.MAXSTEPPERSECOND));
  page.replace("$ACCELERATION",String(Settings.ACCELERATION));
  page.replace("$WATERMLCALIBRATE",String(Settings.WATERMLCALIBRATE));  
  page.replace("$PUMPENABLED",String(Settings.PUMPENABLED) ? "checked":"");  
  page.replace("$PUMPSPEEDL",String(Settings.PUMPSPEEDL));  
  page.replace("$LIMITCAPACITY",String(Settings.LIMITCAPACITY));  
  page.replace("$LIMITHOUR",String(Settings.LIMITHOUR));        
  page.replace("$EXTERNALMAN",String(Settings.EXTERNALMAN));        
  page.replace("$BUILD",String(Settings.BUILD));        
  page.replace("$ENABLELIMITCAPACITY",String(Settings.ENABLELIMITCAPACITY));     
  page.replace("$ENABLELIMITHOUR",String(Settings.ENABLELIMITHOUR));     
  page.replace("$CURRENTPROCESS",String(Settings.CURRENTPROCESS));     
  page.replace("$LIMITDOZE",String(Settings.LIMITDOZE));        
  return page;
}
void updateeeprom()
{
 EEPROM.put(0,Settings);
 EEPROM.commit();
 Serial.println("Saved to eeprom");
}
void handleRoot() {
 // Serial.println("HandleRoot");
if ( server.hasArg("cmd") ) {
      if (server.arg("cmd")=="GetCurrentProcessProgress") {
            switch (Settings.CURRENTPROCESS) {
             case PROCESSCALIBRATE:
                    CurrentProcessProgress=int(((StepToGo-stepper.distanceToGo()  )*100) / (StepToGo)) ; 
                    break;
            
             case PROCESSDOZE: 
                    //StepToGo=
                    Serial.print("Get current progress doze ");
                    Serial.println(int(((StepToGo -stepper.distanceToGo()  )*100) / (StepToGo)) );
                    CurrentProcessProgress=int(((StepToGo -stepper.distanceToGo()  )*100) / (StepToGo)) ; 
                    break;
             };
                          
              server.send ( 200, "text/html",String(CurrentProcessProgress));
                } else 
                
                  if (server.arg("cmd")=="GetCurrentProcess") {
                    server.send ( 200, "text/html",String(Settings.CURRENTPROCESS));
                  }

      } else 
    if (Settings.CURRENTPROCESS==0 && server.hasArg("cmd") )  {
      switch (server.arg("cmd").toInt()) {
     case PROCESSCALIBRATE: 
              Settings.CURRENTPROCESS=PROCESSCALIBRATE;
              StepToGo=Settings.STEPML*Settings.WATERMLCALIBRATE;
              stepper.move(StepToGo);
              server.send (200, "text/html","Process calibrate started");
              Serial.println("Process calibrate started");
            break;
     case PROCESSDOZE: 
              Settings.CURRENTPROCESS=PROCESSDOZE;
              Settings.LIMITDOZE=server.arg("LIMITDOZE").toInt();
              StepToGo=Settings.STEPML*Settings.LIMITDOZE;
              stepper.move(StepToGo);
              server.send ( 200, "text/html","Process doze started" );
              Serial.println("Process doze started");
              updateeeprom();
            break;            
      }
    }
    
   else if ( server.hasArg("EnablePUMPWeb") ) {      
       Serial.println("EnablePUMPWeb entering");
      if (Settings.CURRENTPROCESS==0 && server.arg("EnablePUMPWeb").toInt()==1) {
        Settings.PUMPENABLED=1;
        Settings.CURRENTPROCESS=1;
        updateeeprom();
        server.send ( 200, "text/html", "State PUMPENABLED on");
      }
       else if (Settings.CURRENTPROCESS==0 && server.arg("EnablePUMPWeb").toInt()==0) {
        Settings.PUMPENABLED=0;
        Settings.CURRENTPROCESS=0;
        updateeeprom();
        server.send ( 200, "text/html", "State PUMPENABLED off");
       }
       else if ( Settings.CURRENTPROCESS!=0 )
       {
        server.send ( 200, "text/html", "Other process running");
       }
      }
else if (server.hasArg("UPDATECONFIG") )
{
  Serial.println("AJAX update config from page");
  server.send ( 200, "text/html","ok");
}

else if (server.hasArg("PUMPCONFIG") )
{
  Serial.println("AJAX PUMP Config Query from site");
  Settings.PUMPSPEEDL=server.arg("PUMPSPEEDL").toInt();
  
  Settings.ENABLELIMITCAPACITY=server.arg("ENABLELIMITCAPACITY").toInt();
  Settings.LIMITCAPACITY=server.arg("LIMITCAPACITY").toInt();
  
  Settings.ENABLELIMITHOUR=server.arg("ENABLELIMITHOUR").toInt();  
  Settings.LIMITHOUR=server.arg("LIMITHOUR").toInt();         

  updateeeprom();
  
  server.send ( 200, "text/html","ok");
}

    else if ( server.hasArg("UpdateIndex"))  {
      Serial.println("UpdateIndex");
    String url = String("http://pump.fpv.su/index.html") ;
    Serial.println(url);
    File f = SPIFFS.open("index.html", "w");
    if (f) {
      http.begin(url);
      int httpCode = http.GET();
      if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
          http.writeToStream(&f);
        }
      } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
      f.close();
    }
    http.end();    
    server.send ( 200, "text/html", "Ok" );
  }
  
  else {
   // Serial.println("get index");
    server.send ( 200, "text/html", getPage() );
  }
}



void setup() {
  Serial.begin ( 115200 );
  SPIFFS.begin();
  EEPROM.begin(sizeof(Settings)+1);
  EEPROM.get(0, Settings);
  if (Settings.EEPROMActivated!=73395) {
    Settings= {73395,1401,163,300,0,30,0,0,0,1,100,0,1,0,400};  
    Serial.println("New eeprom init");
    updateeeprom();
  } 
   else 
   {
    Serial.println("Exist eeprom init");  
   }
  
  stepper.setMaxSpeed(Settings.MAXSTEPPERSECOND);
  stepper.setAcceleration(Settings.ACCELERATION);
  //stepper.setSpeed(1500); 

  for ( int x = 0 ; x < 5 ; x++ ) {
    pinMode(GPIOPIN[x], OUTPUT);
  }
  WiFi.begin ( ssid, password );
  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 ); Serial.print ( "." );
  }
  Serial.println ( "" );
  Serial.print ( "Connected to " ); Serial.println ( ssid );
  Serial.print ( "IP address: " ); Serial.println ( WiFi.localIP() );

  server.on ( "/", handleRoot );

  server.begin();
  Serial.println ( "HTTP server started" );

  udp.begin(localPort);
  
  WiFi.hostByName(ntpServerName, timeServerIP);
  sendNTPpacket(timeServerIP);
}

void loop() {

  server.handleClient();

  int cb = udp.parsePacket();
  
  if (!cb) {
  }
  else {
    Serial.println(cb);
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    Serial.print("Seconds since Jan 1 1900 = " );
    Serial.println(secsSince1900);
    Serial.print("Unix time = ");
    const unsigned long seventyYears = 2208988800UL;
    unsigned long epoch = secsSince1900 - seventyYears;
    Serial.println(epoch);
    Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
    Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
    Serial.print(':');
    if ( ((epoch % 3600) / 60) < 10 ) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
    Serial.print(':');
    if ( (epoch % 60) < 10 ) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.println(epoch % 60); // print the second
  }


  t = 90;
  h = 67;
  p = 100.0;

  switch (Settings.CURRENTPROCESS) {
    case PROCESSCALIBRATE:
        if (stepper.distanceToGo()>0) {
          stepper.setSpeed(1200);
          //WaterML = stepper.currentPosition() % Settings.STEPML;
          stepper.run();
        }
              else {
                    Serial.print("Process end");
                    Settings.CURRENTPROCESS=0;
                    CurrentProcessProgress=100;
                  }
    break;
    case PROCESSDOZE:
        if (stepper.distanceToGo()>0) {
          stepper.setSpeed(1200);
          //WaterML = stepper.currentPosition() % Settings.STEPML;
          stepper.run();
        }
              else {
                    Serial.print("Process end");
                    Settings.CURRENTPROCESS=0;
                    CurrentProcessProgress=100;
                  }
    break;    
  } ;

  if (Settings.PUMPENABLED == 1)
  { 
    stepper.setSpeed(23*Settings.PUMPSPEEDL);
   //stepper.setSpeed(1200);
    stepper.runSpeed();
   // Serial.println(23*Settings.PUMPSPEEDL);
    
  }
}


unsigned long sendNTPpacket(IPAddress& address)
{
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
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
  delay(300);
}
