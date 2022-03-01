#include <PowerNodeV11.h> // -- this is an olimex board.
#include <ACNode.h>
#include "MachineState.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <time.h>
#include <Adafruit_MCP23X17.h>

#include "acmerootcert.h"
#include "Button.h"
#include "MyRFID.h" // (local) NFC version
#include "MyLED.h"

#define NODE "trash"

#ifndef OTA_PASSWD
#define OTA_PASSWD "Foo"
#warning "Setting easy to guess/hardcoded OTA password."
#endif

// I2C 
const uint8_t I2C_SDA_PIN = 13; // i2c SDA Pin, ext 2, pin 10
const uint8_t I2C_SCL_PIN = 16; // i2c SCL Pin, ext 2, pin 7
const uint32_t I2C_FREQ = 100000U; 

const int I2C_POWER_PIN = 15;

// MCP I/O-extender, with buttons
// NodeStandard buttons 1..3 are on MCP GPA0..GPA2, see https://github.com/adafruit/Adafruit-MCP23017-Arduino-Library
const uint8_t MCP_I2C_ADDR = 0x20;

const int BUTTONPIN_RED = 0; // GPA0
const int BUTTONPIN_YELLOW = 1; // GPA1
const int BUTTONPIN_GREEN = 2; // GPA2

const int FETPIN_1 = 11; // GPB3
const int FETPIN_2 = 12; // GPB4

// I2C RFID Reader
const int CHECK_NFC_READER_AVAILABLE_TIME_WINDOW =10000; // in ms  
const bool USE_CACHE_FOR_TAGS = true ;// TODO: what exactly does it do?

// LED's are on UEXT on ESP32
const int LEDPIN_ORANGE = 14;
const int LEDPIN_RED  = 5;
const int LEDPIN_YELLOW = 4;
const int LEDPIN_GREEN = 2;

// NTP (daylightOffset_sec is handled quite strange, read the source...)
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

// The ACNode "NODE"
ACNode node = ACNode(NODE);
OTA ota = OTA(OTA_PASSWD); // TODO: find out how this actually works
MqttLogStream mqttlogStream = MqttLogStream();

MachineState machinestate = MachineState();
enum { ACTIVE = MachineState::START_PRIVATE_STATES, DEACTIVATING };

TelnetSerialStream telnetSerialStream = TelnetSerialStream();
WiFiClientSecure client;

// MCP object with buttons wired to it, with callback
Adafruit_MCP23X17 mcp;

void onButtonPressed(int pin, int state);

Button buttonRed(&mcp, BUTTONPIN_RED, onButtonPressed);
Button buttonYellow(&mcp, BUTTONPIN_YELLOW, onButtonPressed);
Button buttonGreen(&mcp, BUTTONPIN_GREEN, onButtonPressed);

// LEDS wired to UEXT
MyLED orange(LEDPIN_ORANGE);
MyLED red(LEDPIN_RED);
MyLED yellow(LEDPIN_YELLOW);
MyLED green(LEDPIN_GREEN);
MyLED fet1(FETPIN_1, false, &mcp);
MyLED fet2(FETPIN_2, false, &mcp);

// RFID reader

// RFID.cpp uses https://github.com/nurun/arduino_NFC/blob/master/PN532_I2C.cpp  for NFC cards
// look like the i2c address is hard-coded there (luckely: on the actual addres 0x24)
MyRFID reader = MyRFID(&Wire, USE_CACHE_FOR_TAGS); // use tags are stored in cache, to allow access in case the MQTT server is down; also use NFC RFID card
unsigned long lastCheckNFCReaderTime = 0;

// The 'application state'

unsigned long now;
bool dirty = true;

unsigned long lastUpdatedChores = 0; // last refreshed chores from API, in ms
time_t nextCollection = 0; // the very next 'collection', in Unix EPOCH seconds
unsigned long lastUpdatedNumberOfPeople =0;
int numberOfPeople = -1; // invalid/unknown

// the position in which we 'want' the trash container vs. the position it actually is
int wantedPosition=-1;  // initially unknown 
int actualPosition=-1; // iniitally unknown

void resetNFCReader(boolean force) {
  if (!force) {
    force = !reader.CheckPN53xBoardAvailable();
    if (force) {
      Log.println("error in RFID read, resetting");
    }
  }
  lastCheckNFCReaderTime = now;
  if (force) { 
    pinMode(I2C_SCL_PIN, OUTPUT);
    digitalWrite(I2C_SCL_PIN, 0);
    pinMode(I2C_SDA_PIN, OUTPUT);
    digitalWrite(I2C_SDA_PIN, 0);
    digitalWrite(I2C_POWER_PIN, 1);
    delay(500);
    digitalWrite(I2C_POWER_PIN, 0);
    reader.begin();
  }
}

DynamicJsonDocument doc(4096);

boolean fetchJSONDocument(String url) {
  
  HTTPClient http;
  if (!http.begin(client, url )) {
    Log.printf("failed to json from: %s\n", url);
    return false;
  }

  int httpStatus = http.GET();

  if (httpStatus != 200) {
    Log.printf("GET %s failed, error: %d\n", url, httpStatus);
    return false;
  };

  DeserializationError error = deserializeJson(doc, http.getString());
  if (error) {
    Log.printf("error parsing json\n");
    return false;
  }

  return true;
}

void fetchNumberOfPeople() {

  if (fetchJSONDocument("https://makerspaceleiden.nl/crm/api/v1/state")) {
      numberOfPeople = doc["num_users_in_space"];
      Log.printf("json says: number of people in space: %d\n", numberOfPeople);
  }
  
}

void fetchChores() {

  if (!fetchJSONDocument("https://makerspaceleiden.nl/crm/chores/api/v1/list/empty_trash_small")) { return ; }

  time_t timestamp = 0;
  JsonArray chores = doc["chores"].as<JsonArray>();
  if (!chores) {
    Log.println("doc[\"chores\"] is not an array");
    return;
  }
  
  for (JsonVariant chore : chores) {
    JsonArray events = chore["events"].as<JsonArray>();
    if (!events) {
      Log.println("chore[\"events\"] is not an array");
      return;
    }
    for (JsonVariant event : events) {
      timestamp = event["when"]["timestamp"]; // unix epoch
      if (timestamp > 0) break;
    }
    if (timestamp > 0) break;
  }
  if (timestamp == 0) {
    Log.println("did not find a timestamp for next collection in json");
    return;
  }
  Log.printf("json says: next collection @%d\n", timestamp);
  nextCollection = timestamp;
}

time_t epoch() {
  // get the current EPOCH time
  time_t now = 0;
  struct tm timeinfo;
  // TODO: examples on the internet include this call to getLocalTime(), but why it that needed? (result in timeinfo is not really used)
  if (!getLocalTime(&timeinfo)) {
    Log.println("failed to get time");
    return now;
  }
  // Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  time(&now);
  return now;
}

void onButtonPressed(int pin, int state) {
   if (state == HIGH) {
     // active Low
     return; 
   }
   if (machinestate.state() == ACTIVE) {
      actualPosition = pin;
      Log.printf("tag %s indicated position is now %d\n", reader.getLastTag(), actualPosition);
      switch (actualPosition) {
        case BUTTONPIN_RED:
            node.request_approval(reader.getLastTag(), "outside", "small-container", false);
          break;
        case BUTTONPIN_YELLOW:
            node.request_approval(reader.getLastTag(), "lost", "small-container", false);
          break;
        case BUTTONPIN_GREEN:
            node.request_approval(reader.getLastTag(), "inside", "small-container", false);
          break;
      }
   } 
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n\n");
  Serial.println("Booted: " __FILE__ " " __DATE__ " " __TIME__ );

  Serial.printf("MQTT_MAX_PACK_SIZE == %d\n", MQTT_MAX_PACKET_SIZE);
  
  now = millis();

  // TODO: find out more on this MQTT-stuff
  node.set_mqtt_prefix("ac");
  node.set_master("master");

  // i2C Setup
  pinMode(I2C_POWER_PIN, OUTPUT);
  digitalWrite(I2C_POWER_PIN, LOW);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ);

  // define machine state
  machinestate.defineState(ACTIVE, "Active", LED::LED_ERROR, 7 * 1000, DEACTIVATING);
  machinestate.defineState(DEACTIVATING, "Deactivating", LED::LED_ERROR, 3 * 1000, MachineState::WAITINGFORCARD);

  if (!mcp.begin_I2C(MCP_I2C_ADDR, &Wire)) {
    Log.println("cannot initialize MCP I/O-extender");
  }

  // begin buttons, now that i2c and mcp are started
  buttonRed.begin();
  buttonYellow.begin();
  buttonGreen.begin();

  // begin leds
  orange.begin();
  red.begin();
  yellow.begin();
  green.begin();
  fet1.begin();
  fet2.begin();

  fet1.set(MyLED::LED_ON);
  fet2.set(MyLED::LED_OFF);
  
  client.setCACert(rootCACertificate);

  machinestate.setOnChangeCallback(MachineState::ALL_STATES, [](MachineState::machinestate_t last, MachineState::machinestate_t current) -> void {
    dirty = true;
  });

  node.onConnect([]() {
    Log.println("Connected");
    //init and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    epoch();
    machinestate = MachineState::WAITINGFORCARD;
  });

  node.onDisconnect([]() {
    Log.println("Disconnected");
    machinestate = MachineState::NOCONN;
  });

  node.onError([](acnode_error_t err) {
    Log.printf("Error %d\n", err);
    machinestate = MachineState::WAITINGFORCARD;
  });

  node.onApproval([](const char * machine) {
    if (machinestate.state() == MachineState::CHECKINGCARD) {
      // approval of the swipe to turn node ACTIVE
      machinestate = ACTIVE;
    } else {
      Log.printf("approve while state %d\n", machinestate.state());
    }
  });

  node.onDenied([](const char * machine) {
    // machinestate = REJECTED;
    //Log.printf("denied %s\n", machine);
    machinestate = DEACTIVATING;
  });

  node.onReport([](JsonObject  & report) {
    Log.println("reporting...");
    // report["swipeouts"] = swipeouts_count;
  });

  reader.onSwipe([](const char * tag) -> ACBase::cmd_result_t {
    if (machinestate.state() == MachineState::WAITINGFORCARD) {
      node.request_approval(tag, "energize", "small-container", false);
      machinestate = MachineState::CHECKINGCARD;
    }
    resetNFCReader(false);
    return ACBase::CMD_CLAIMED;
  });
  
  reader.set_debug(false);

  node.addHandler(&ota);
  node.addHandler(&machinestate);
  node.addHandler(&reader);

  Log.addPrintStream(std::make_shared<MqttLogStream>(mqttlogStream));

  auto t = std::make_shared<TelnetSerialStream>(telnetSerialStream);
  Log.addPrintStream(t);
  Debug.addPrintStream(t);

  node.begin(BOARD_OLIMEX); // OLIMEX

  resetNFCReader(true);
  
  Log.println("Booted: " __FILE__ " " __DATE__ " " __TIME__ );
}

void showState() {  

  if (!dirty) return;
  
  if (machinestate.state() == DEACTIVATING) {
    orange.set(MyLED::LED_FLASH);
  } else if (machinestate.state() == ACTIVE) {
    orange.set(MyLED::LED_ON);
  } else {
    orange.set(MyLED::LED_OFF);
  }
    
  // the code below is intentionally quite verbatim, let's optimize later
    
  if ((actualPosition < 0 && wantedPosition < 0) || machinestate.state() < MachineState::WAITINGFORCARD) {
    // everything is unknown or not in operating state
    red.set(MyLED::LED_FLASH);
    yellow.set(MyLED::LED_FLASH); 
    green.set(MyLED::LED_FLASH);
    return;
  }
  
  if (actualPosition >= 0 && wantedPosition <0) {
    // actualPosition known, wanted not
    fet2.set(MyLED::LED_OFF);
    if (actualPosition == BUTTONPIN_RED) {
      red.set(MyLED::LED_ON); // it in outside
      yellow.set(MyLED::LED_FLASH);
      green.set(MyLED::LED_FLASH);
    } else if (actualPosition == BUTTONPIN_YELLOW) {
      red.set(MyLED::LED_FLASH);
      yellow.set(MyLED::LED_ON); // it is lost
      green.set(MyLED::LED_FLASH);
    } else if (actualPosition == BUTTONPIN_GREEN) {
      red.set(MyLED::LED_FLASH);
      yellow.set(MyLED::LED_FLASH);
      green.set(MyLED::LED_ON); // it is inside
    }
    return;
  }

  if (actualPosition < 0 && wantedPosition >= 0) {
    // actual Position not known, wanted known
    fet2.set(MyLED::LED_OFF);
    if (wantedPosition == BUTTONPIN_RED) {
      red.set(MyLED::LED_FLASH); // it should be outside     
      yellow.set(MyLED::LED_OFF);
      green.set(MyLED::LED_OFF);
    } else {
      red.set(MyLED::LED_OFF);
      yellow.set(MyLED::LED_OFF);
      green.set(MyLED::LED_FLASH); // it should be inside
    }
    return;
  }

  // happy
  if (actualPosition == BUTTONPIN_RED) {
    red.set(MyLED::LED_ON); // it is outside
    yellow.set(MyLED::LED_OFF);
    green.set(MyLED::LED_OFF);
  } else if (actualPosition == BUTTONPIN_YELLOW) {
    red.set(MyLED::LED_OFF);
    yellow.set(MyLED::LED_ON); // it is lost
    green.set(MyLED::LED_OFF);
  } else if (actualPosition == BUTTONPIN_GREEN) {
    red.set(MyLED::LED_OFF);
    yellow.set(MyLED::LED_OFF);
    green.set(MyLED::LED_ON); // it is inside
  }
  if (actualPosition == wantedPosition) {
    fet2.set(MyLED::LED_OFF);
  } else if (wantedPosition == BUTTONPIN_RED) {
      red.set(MyLED::LED_FLASH); // it should be outside but it is not
      if (numberOfPeople > 0 && numberOfPeople <= 3) {
        // it should be outside, it is not and there are 3 or less people inside
        fet2.set(MyLED::LED_FLASH); 
      } else {
        fet2.set(MyLED::LED_ON);
      }
  } else {
    green.set(MyLED::LED_FLASH); // it should be inside but it is not              
    fet2.set(MyLED::LED_OFF);
  }  
}

void loop() {
  node.loop();
  fet1.loop();
  fet2.loop();
  now = millis();

  buttonRed.check();
  buttonYellow.check();
  buttonGreen.check();

  if ((now - lastCheckNFCReaderTime) > CHECK_NFC_READER_AVAILABLE_TIME_WINDOW) {
    resetNFCReader(false);
  }
  
  time_t epochNow = epoch();
  switch (machinestate.state()) {
    case MachineState::WAITINGFORCARD:

      if ((lastUpdatedNumberOfPeople == 0) || (now - lastUpdatedNumberOfPeople) > 1 * 5 * 60 * 1000) {
        Log.println("updating number of people");
        lastUpdatedNumberOfPeople = now;
        fetchNumberOfPeople();
        dirty = true;
      }

      if ((lastUpdatedChores == 0) || (now - lastUpdatedChores) > 1 * 60 * 60 * 1000) {
        Log.println("updating chores");
        lastUpdatedChores = now;
        fetchChores();
      }

      // yeah! actual business logic :-)
      if (nextCollection == 0 || epochNow == 0) {
        // panic!
        wantedPosition = -1; // unknown
        dirty = true;
      } else {
        if ((nextCollection - epochNow) < 15 * 60 * 60) {
          wantedPosition = BUTTONPIN_RED; // outside
        } else {
          wantedPosition = BUTTONPIN_GREEN; // inside
        }
        dirty = true;
      }
      break;
    case ACTIVE:
      break;
    case DEACTIVATING:
      break;
    default:
      break;
  }
  showState();

}
