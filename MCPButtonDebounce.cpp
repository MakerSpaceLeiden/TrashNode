#include "Arduino.h"
#include "MCPButtonDebounce.h"

#define SAMPLES_PER_DELAY (8)

static void _update(uint32_t arg) {
	MCPButtonDebounce * c = (MCPButtonDebounce*)arg;
	c->update();
}

MCPButtonDebounce::MCPButtonDebounce(Adafruit_MCP23XXX *mcp, int pin, unsigned long delay) {
   _mcp = mcp;
  _pin = pin;
  _delay = delay;
  _lastDebounceTime = 0;
  _lastChangeTime = 0;
  _lastStateBtn = HIGH;
  _ticker.attach_ms(
    delay/SAMPLES_PER_DELAY,_update,(uint32_t )this
  );
}

void MCPButtonDebounce::update(){
  _lastDebounceTime = millis();
  int btnState = _mcp->digitalRead(_pin);
  if (btnState == _lastStateBtn) {
    _lastChangeTime = millis();
	  return;
  };

  if (millis() - _lastChangeTime < _delay) return;

  _lastStateBtn = btnState;
  if (_callback) 
	_callback(_lastStateBtn);
}

int MCPButtonDebounce::state(){
  return _lastStateBtn;
}

void MCPButtonDebounce::setCallback(btn_callback_t callback) {
  _mcp->pinMode(_pin, INPUT_PULLUP);
  _callback = callback;
}
