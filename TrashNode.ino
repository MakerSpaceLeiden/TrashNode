
#include <PowerNodeV11.h> // -- this is an olimex board.

#include <ACNode.h>
#include "MachineState.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>

#include "acmerootcert.h"
#include "MCPButtonDebounce.h"

#define MACHINE "trash"

const uint8_t I2C_SDA_PIN = 13; // i2c SDA Pin, ext 2, pin 10
const uint8_t I2C_SCL_PIN = 16; // i2c SCL Pin, ext 2, pin 7

const uint8_t mcp_i2c_addr = 0x20;

#define LEDPIN_RED 5
#define LEDPIN_YELLOW 4
#define LEDPIN_GREEN 2

// buttons 1..3 on MCP GPA0..GPA2, see https://github.com/adafruit/Adafruit-MCP23017-Arduino-Library
#define BUTTONPIN_RED 0 
#define BUTTONPIN_YELLOW 1
#define BUTTONPIN_GREEN 2

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

ACNode node = ACNode(MACHINE);

TwoWire i2cBus = TwoWire((uint8_t)0);

MachineState machinestate = MachineState();
enum { ACTIVE = MachineState::START_PRIVATE_STATES, DEACTIVATING };

Adafruit_MCP23X17 mcp;

MCPButtonDebounce buttonRed(&mcp, BUTTONPIN_RED, 250);
MCPButtonDebounce buttonYellow(&mcp, BUTTONPIN_YELLOW, 250);
MCPButtonDebounce buttonGreen(&mcp, BUTTONPIN_GREEN, 250);

LED red(LEDPIN_RED);
LED yellow(LEDPIN_YELLOW);
LED green(LEDPIN_GREEN);

TelnetSerialStream telnetSerialStream = TelnetSerialStream();
WiFiClientSecure client;

DynamicJsonDocument doc(4096);
unsigned long lastUpdatedChores = 0;
time_t nextCollection = 0;

void fetchChores() {

  HTTPClient http;
  if (!http.begin(client, "https://makerspaceleiden.nl/crm/chores/api/v1/list/empty_trash_small" )) {
    Log.println("failed to load chores from server");
    return;
  }

  int httpStatus = http.GET();

  if (httpStatus != 200) {
    Log.printf("GET chores failed, error: %d\n", httpStatus);
    return;
  };

  DeserializationError error = deserializeJson(doc, http.getString());
  if (error) {
    Log.println("error parsing json");
    return;
  }

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
  time_t now = 0;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Log.println("failed to get time");
    return now;
  }
  // Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  time(&now);
  return now;
}

int wantedPosition=-1; 
int actualPosition=-1;

int previousWanted=-2;
int previousActual=-2;

void onButtonPressed(int pin) {
  Log.printf("button pressed: %d\n", pin);
  if (machinestate.state() == MachineState::WAITINGFORCARD) {
    // machinestate = ACTIVE;
    actualPosition = pin;
  } else {
    Log.println("button pressed while not ready");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n\n");
  Serial.println("Booted: " __FILE__ " " __DATE__ " " __TIME__ );

  // i2C Setup
  i2cBus.begin(I2C_SDA_PIN, I2C_SCL_PIN); // , 50000);
  
  machinestate.defineState(ACTIVE, "Active", LED::LED_ERROR, 5 * 1000, DEACTIVATING);
  machinestate.defineState(DEACTIVATING, "Deactivating", LED::LED_ERROR, 1 * 1000, MachineState::WAITINGFORCARD);

  if (!mcp.begin_I2C(mcp_i2c_addr, &i2cBus)) {
    Log.println("error TODO");
  }
  
  red.set(LED::LED_OFF);
  yellow.set(LED::LED_OFF);
  green.set(LED::LED_OFF);

  client.setCACert(rootCACertificate);

  machinestate.setOnChangeCallback(MachineState::ALL_STATES, [](MachineState::machinestate_t last, MachineState::machinestate_t current) -> void {
    Log.print("state changed: "); Log.println(current);
    if (current == ACTIVE) {
      red.set(LED::LED_SLOW);
      yellow.set(LED::LED_FLASH);
      green.set(LED::LED_FLASH);
    } else {
      red.set(LED::LED_OFF);
      yellow.set(LED::LED_OFF);
      green.set(LED::LED_OFF);
    }
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

  buttonRed.setCallback([](int state) {
    onButtonPressed(BUTTONPIN_RED);
  });

  buttonYellow.setCallback([](int state) {
    onButtonPressed(BUTTONPIN_YELLOW);
  });

  buttonGreen.setCallback([](int state) {
    onButtonPressed(BUTTONPIN_GREEN);
  });

  node.addHandler(&machinestate);

  auto t = std::make_shared<TelnetSerialStream>(telnetSerialStream);
  Log.addPrintStream(t);
  Debug.addPrintStream(t);

  node.begin(BOARD_OLIMEX); // OLIMEX

  Log.println("Booted: " __FILE__ " " __DATE__ " " __TIME__ );
}

void showState() {
  if (previousWanted == wantedPosition && previousActual == actualPosition) return; // no change
  
  Log.printf("actual %d wanted %d\n", actualPosition, wantedPosition);\

  if (actualPosition <= 0 && wantedPosition <= 0) {
    // everything is unknown  
    red.set(LED::LED_FLASH);
    yellow.set(LED::LED_FLASH); 
    green.set(LED::LED_FLASH);
  }
  
  if (actualPosition > 0 && wantedPosition <=0) {
    // actualPosition known, wanted not
    if (actualPosition == BUTTONPIN_RED) {
      red.set(LED::LED_ON); // it in outside
      yellow.set(LED::LED_FLASH);
      green.set(LED::LED_FLASH);
    } else if (actualPosition == BUTTONPIN_YELLOW) {
      red.set(LED::LED_FLASH);
      yellow.set(LED::LED_ON); // it is lost
      green.set(LED::LED_FLASH);
    } else if (actualPosition == BUTTONPIN_GREEN) {
      red.set(LED::LED_FLASH);
      yellow.set(LED::LED_FLASH);
      green.set(LED::LED_ON); // it is inside
    }
    return;
  }

  if (actualPosition <= 0 && wantedPosition > 0) {
    // actual Position not known, wanted known
    if (wantedPosition == BUTTONPIN_RED) {
      red.set(LED::LED_FLASH); // it should be outside     
    } else {
      green.set(LED::LED_FLASH); // it should be inside
    }
    return;
  }

  // happy
  if (actualPosition == BUTTONPIN_RED) {
    red.set(LED::LED_ON); // it in outside
    yellow.set(LED::LED_OFF);
    green.set(LED::LED_OFF);
  } else if (actualPosition == BUTTONPIN_YELLOW) {
    red.set(LED::LED_OFF);
    yellow.set(LED::LED_ON); // it is lost
    green.set(LED::LED_OFF);
  } else if (actualPosition == BUTTONPIN_GREEN) {
    red.set(LED::LED_OFF);
    yellow.set(LED::LED_OFF);
    green.set(LED::LED_ON); // it is inside
  }
  if (actualPosition != wantedPosition) {
    if (wantedPosition == BUTTONPIN_RED) {
      red.set(LED::LED_FLASH); // it should be outside but it is not      
    } else {
      green.set(LED::LED_FLASH); // it should be inside but it is not              
    }
  }  
  previousWanted = wantedPosition;
  previousActual = actualPosition;
}

void loop() {
  node.loop();
  long now = millis();
  time_t epochNow = epoch();
  switch (machinestate.state()) {
    case MachineState::WAITINGFORCARD:
      if ((lastUpdatedChores == 0) || (now - lastUpdatedChores) > 1 * 60 * 60 * 1000) {
        Log.println("updating chores");
        lastUpdatedChores = now;
        fetchChores();
      }
      // yeah! actual business logic :-)
      if (nextCollection == 0 || epochNow == 0) {
        // panic!
        wantedPosition = -1; // unknown
      } else {
        if ((nextCollection - epochNow) < 15 * 60 * 60) {
          wantedPosition = BUTTONPIN_RED; // outside
        } else {
          wantedPosition = BUTTONPIN_GREEN; // inside
        }
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
