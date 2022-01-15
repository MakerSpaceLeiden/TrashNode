#pragma once

#include "Arduino.h"
#include <functional>
#include <Adafruit_MCP23X17.h>

class Button {
  public:
    typedef std::function<void(int,int)> btn_callback_t;  
    Button(Adafruit_MCP23XXX *mcp, int pin, btn_callback_t callback);
    void check();
    void begin();
    
  private:
    Adafruit_MCP23XXX *_mcp;
    int _state;
    int _pin;
    btn_callback_t _callback;
};
