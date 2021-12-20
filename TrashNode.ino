

#include <PowerNodeV11.h> // -- this is an olimex board.

#include <ACNode.h>
#include "MachineState.h"
#include <ButtonDebounce.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WiFi.h>

#define MACHINE "trash"
#define LEDPIN_RED 5
#define LEDPIN_YELLOW 4
#define LEDPIN_GREEN 2

#define BUTTON 34

ACNode node = ACNode(MACHINE);
ButtonDebounce button(BUTTON, 250);
MachineState machinestate = MachineState();
enum { ACTIVE = MachineState::START_PRIVATE_STATES, DEACTIVATING };

LED red(LEDPIN_RED);
LED yellow(LEDPIN_YELLOW);
LED green(LEDPIN_GREEN);

TelnetSerialStream telnetSerialStream = TelnetSerialStream();
WiFiClient client;

void testClient(const char * host, uint16_t port)
{
  Serial.print("\nconnecting to ");
  Serial.println(host);

  WiFiClient client;
  if (!client.connect(host, port)) {
    Serial.println("connection failed");
    return;
  }
  client.printf("GET /chore HTTP/1.1\r\nHost: %s\r\n\r\n", host);
  while (client.connected() && !client.available());
  while (client.available()) {
    Serial.write(client.read());
  }

  Serial.println("closing connection\n");
  client.stop();
}

void fetch() {
  
  HTTPClient http;
  if (!http.begin(client, "http://10.1.0.168:8080/chore" )) {
    Log.println("Failed to create fetch of state from server.");
    return;
  }

  int httpStatus = http.GET();

  if (httpStatus != 200) {
    Log.printf("GET... failed, error: %d\n", httpStatus);
    return;
  };

  String payload = http.getString();

  Log.println(payload);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n\n");
  Serial.println("Booted: " __FILE__ " " __DATE__ " " __TIME__ );

  machinestate.defineState(ACTIVE, "Active", LED::LED_ERROR, 5 * 1000, DEACTIVATING);
  machinestate.defineState(DEACTIVATING, "Deactivating", LED::LED_ERROR, 1 * 1000, MachineState::WAITINGFORCARD);
  
  red.set(LED::LED_OFF);
  yellow.set(LED::LED_OFF);
  green.set(LED::LED_OFF);

  // client.setInsecure();//skip verification

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

  button.setCallback([](int state) {
    machinestate = ACTIVE;
  });

  node.addHandler(&machinestate);

  auto t = std::make_shared<TelnetSerialStream>(telnetSerialStream);
  Log.addPrintStream(t);
  Debug.addPrintStream(t);

  node.begin(BOARD_OLIMEX); // OLIMEX


  Log.println("Booted: " __FILE__ " " __DATE__ " " __TIME__ );
}

unsigned long lastUpdatedChores = 0L;

void loop() {
  node.loop();
  long now = millis();
  if (now - lastUpdatedChores > 10*1000) {
    lastUpdatedChores = now;
    fetch();
  }
  switch (machinestate.state()) {
    case ACTIVE:
      break;
    case DEACTIVATING:
      break;
    default:
      break;
   }

}
