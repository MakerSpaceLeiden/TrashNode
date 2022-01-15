#pragma once
#include "Ticker.h"
#include <Adafruit_MCP23X17.h>

class MyLED {
public:
   typedef enum { LED_OFF, LED_FLASH, LED_SLOW, LED_FAST, LED_ERROR, LED_PENDING, LED_IDLE, LED_ON, NEVERSET } led_state_t;

   MyLED(const byte pin = -1, bool inverted = false, Adafruit_MCP23XXX *mcp = NULL);

   void begin();
   void set(led_state_t state);

   // Not really public - but needed in the ticker callbacks.
   void _on();
   void _off();
   void _set(bool on);
   void _update();

private:
   unsigned int _pin,_tock;
   const bool _inverted;
   Ticker _ticker;
   led_state_t _lastState;
   Adafruit_MCP23XXX *_mcp;
};
