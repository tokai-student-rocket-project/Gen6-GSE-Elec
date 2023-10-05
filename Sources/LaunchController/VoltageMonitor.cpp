#include "VoltageMonitor.hpp"


VoltageMonitor::VoltageMonitor(uint8_t pinNumber, float upperResistance, float lowerResistance) {
  _pinNumber = pinNumber;

  _scaleFactor = (upperResistance + lowerResistance) / lowerResistance;
}


float VoltageMonitor::getVoltage_V() {
  float readingValue = (float)analogRead(_pinNumber);
  float voltage = readingValue / 1024.0 * 5.0;

  return voltage * _scaleFactor;
}
