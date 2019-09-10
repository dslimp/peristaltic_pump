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
#define ssid      "etsgroup"
#define password  "dunets7854"
#define PROCESSOFF 0
#define PROCESSPUMP 1
#define PROCESSDOZE 2
#define PROCESSCALIBRATE 3
#define BLINDUP 1
#define BLINDDOWN 2
#define BLINDUPCALIBRATE 3
#define BLINDDOWNCALIBRATE 4
#define BLINDSTOP 0

AccelStepper stepper(1, stepPin, dirPin);

int addr = 0;
unsigned long int StepToGo = 0;
byte MotorEnabled = 0;
// 1 - blindup ; 2 - blind down; 3 blind up calibrate; 4 blind down calibrate; 0 = stop

ESP8266WebServer server (80);

struct SettingsStruct {
  unsigned long int EEPROMActivated;
  byte CURRENTPROCESS;
  unsigned long int STEPCOUNT;
  int ACCELERATION;
  byte CURRENTPOSITION;
  byte INVERSE;
  int BUILD;
  byte ENDSTOP;
  int SPEED;
};

SettingsStruct Settings;

String getPage() {
  Serial.println("get index");
  String page;
  File f = SPIFFS.open("/index.html", "r");
  while (f.available()) {
    page += char(f.read());
  }
  f.close();
  page.replace("$STEPCOUNT", String(Settings.STEPCOUNT));
  page.replace("$CURRENTPROCESS", String(Settings.CURRENTPROCESS));
  page.replace("$ACCELERATION", String(Settings.ACCELERATION));
  page.replace("$CURRENTPOSITION", String(Settings.CURRENTPOSITION));
  page.replace("$INVERSE", String(Settings.INVERSE));
  page.replace("$ENDSTOP", String(Settings.ENDSTOP));
  page.replace("$SPEED", String(Settings.SPEED));
  return page;
}
void updateeeprom()
{
  EEPROM.put(0, Settings);
  EEPROM.commit();
  Serial.println("Saved to eeprom");
}
void handleRoot() {

  if ( server.hasArg("cmd") ) {
 
    if ( server.arg("cmd")=="BUTTONUP" && Settings.CURRENTPOSITION > 0 ) {
      Serial.println("Blinds Up");
      Settings.CURRENTPROCESS = BLINDUP;
      StepToGo = -Settings.CURRENTPOSITION;
      if (Settings.INVERSE)  {
        StepToGo = -StepToGo;
      };
      stepper.moveTo(StepToGo);
      server.send ( 200, "text/html","Blinds Up");
    }
    else if ( server.arg("cmd")=="BUTTONDOWN" && Settings.CURRENTPOSITION < Settings.STEPCOUNT ) {
      Serial.println("Blinds Down");
      Settings.CURRENTPROCESS = BLINDDOWN;
      StepToGo = Settings.STEPCOUNT - Settings.CURRENTPOSITION;
      if (Settings.INVERSE)  {
        StepToGo = -StepToGo;
      };
      stepper.moveTo(StepToGo);
      server.send ( 200, "text/html","Blinds Down");
    }
    else if ( server.arg("cmd")=="BUTTONSTOP" ) {
      Serial.println("Blinds Stop");
      Settings.CURRENTPROCESS = BLINDSTOP;
      updateeeprom();
            server.send ( 200, "text/html","ok");
    }

    else  if (server.hasArg("UPDATECONFIG") ) {
      Serial.println("AJAX update config from page");
      server.send ( 200, "text/html", "ok");
    }
   
  }
  else {
      server.send ( 200, "text/html", getPage() );
    }
}

void setup() {
  Serial.begin ( 115200 );
  SPIFFS.begin();
  EEPROM.begin(sizeof(Settings) + 1);
  EEPROM.get(0, Settings);
  if (Settings.EEPROMActivated != 73397) {
    
    Settings = {73397, 0, 500, 10, 20, 0, 10, 0, 20};
    Serial.println("New EEPROM init");
    updateeeprom();
  }
  else
  {
    Serial.println("Exist EEPROM init");
  }

  stepper.setMaxSpeed(Settings.SPEED);
  stepper.setAcceleration(Settings.ACCELERATION);
  //stepper.setSpeed(1500);

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
}

void printSettings() {
   Serial.println("");
   Serial.print(" Settings.CURRENTPROCESS = ");
   Serial.println(Settings.CURRENTPROCESS);
   Serial.print(" Settings.CURRENTPOSITION = ");
   Serial.println(Settings.CURRENTPOSITION);
   Serial.print(" Settings.STEPCOUNT = ");
   Serial.println(Settings.STEPCOUNT);   
   Serial.print(" Settings.ACCELERATION = ");
   Serial.println(Settings.ACCELERATION);   
   Serial.print(" Settings.SPEED = ");
   Serial.println(Settings.SPEED);   
}

void loop() {
  server.handleClient();

  switch (Settings.CURRENTPROCESS) {
    case BLINDUP:
      if (stepper.distanceToGo() > 0) {
        //stepper.setSpeed(1200);
        //WaterML = stepper.currentPosition() % Settings.STEPML;
        stepper.run();
        Settings.CURRENTPOSITION = stepper.distanceToGo();
        printSettings();
      }
      else {
        Serial.println("Process Blind UP end");
        Settings.CURRENTPROCESS = 0;
        Settings.CURRENTPOSITION = 0;
        updateeeprom();
        printSettings();
      }
      break;
    case BLINDDOWN:
      if (stepper.distanceToGo() > 0) {
        //stepper.setSpeed(1200);
        //WaterML = stepper.currentPosition() % Settings.STEPML;
        Settings.CURRENTPOSITION =  Settings.STEPCOUNT-stepper.distanceToGo();
        stepper.run();
         printSettings();       

      }
      else {
        Serial.print("Process  Blind DOWN end");
        Settings.CURRENTPROCESS = 0;
        Settings.CURRENTPOSITION = Settings.STEPCOUNT;
        updateeeprom();
        printSettings();
      }
      break;
  } ;


  if (Settings.CURRENTPROCESS == BLINDDOWNCALIBRATE || Settings.CURRENTPROCESS == BLINDUPCALIBRATE)
  {
    stepper.setSpeed(Settings.CURRENTPROCESS == BLINDDOWNCALIBRATE ? Settings.SPEED : -Settings.SPEED);
    stepper.runSpeed();
  }
}
