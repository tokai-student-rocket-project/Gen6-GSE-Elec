#include "Lib_GseSignal.hpp"


GseSignal::GseSignal(uint8_t pinNumber) {
  _pinNumber = pinNumber;
  pinMode(_pinNumber, INPUT_PULLUP);
}


bool GseSignal::isSignaled() {
  return digitalRead(_pinNumber) == HIGH;
}
