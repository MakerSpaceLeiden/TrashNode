#include "Button.h"

Button::Button(Adafruit_MCP23XXX *mcp, int pin, btn_callback_t callback) {
  _mcp = mcp;
  _pin = pin;
  _state = HIGH;
  _callback = callback;
}

void Button::check() {
  int r = _mcp->digitalRead(_pin);
  if (r != _state) _callback(_pin, r);
  _state = r;
}
