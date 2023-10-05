#include "AmpereMonitor.hpp"


AmpereMonitor::AmpereMonitor(uint8_t address) {
  _ina219 = new Adafruit_INA219(address);
}


void AmpereMonitor::begin() {
  _ina219->begin();
}


float AmpereMonitor::getAmpere_A() {
  return _ina219->getCurrent_mA() / 1000.0;
}
