#pragma once
#include <Arduino.h>


class ThermalMonitor {
  uint8_t _pinNumber;

  float _lowerResistance;

public:
  ThermalMonitor(uint8_t pinNumber, float lowerResistance);

  float getTemperature_degC();
};
