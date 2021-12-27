/*
  ButtonDebounce.h - Library for Button Debounce.
  Created by Maykon L. Capellari, September 30, 2017.
  Released into the public domain.
*/
#ifndef MCPButtonDebounce_h
#define MCPButtonDebounce_h

#include "Arduino.h"
#include "Ticker.h"
#include <functional>
#include <Adafruit_MCP23X17.h>

class MCPButtonDebounce{
  public:
    MCPButtonDebounce(Adafruit_MCP23XXX *mcp, int pin, unsigned long delay = 250);
    Ticker _ticker;
    int state();
    typedef std::function<void(int)> btn_callback_t;
    void setCallback(btn_callback_t callback);
    void update();
  private:
    Adafruit_MCP23XXX *_mcp;
    int _pin;
    unsigned long _delay;
    unsigned long _lastDebounceTime;
    unsigned long _lastChangeTime;
    int _lastStateBtn;
    btn_callback_t _callback;
};

#endif
