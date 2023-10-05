#include "ThermalMonitor.hpp"


ThermalMonitor::ThermalMonitor(uint8_t pinNumber, float lowerResistance) {
  _pinNumber = pinNumber;
  _lowerResistance = lowerResistance;
}


float ThermalMonitor::getTemperature_degC() {
  float readingValue = (float)analogRead(_pinNumber);
  float resistance = _lowerResistance * readingValue / (1024.0 - readingValue);

  // TODO 最適化する
  float temperatureBar = 1 / (25.0 + 273.15) + 1 / 3380.0 * log(resistance / 10000.0);
  return 1.0 / temperatureBar - 273.15;
}
