#include "PowerMonitor.hpp"


PowerMonitor::PowerMonitor(uint8_t address) {
  _ina219 = new Adafruit_INA219(address);
}


void PowerMonitor::begin() {
  _ina219->begin();
}


float PowerMonitor::getAmpere_A() {
  return _ina219->getCurrent_mA() / 1000.0;
}


float PowerMonitor::getVoltage_V() {
  return _ina219->getBusVoltage_V();
}


float PowerMonitor::getPower_W() {
  return _ina219->getPower_mW() / 1000.0;
}
