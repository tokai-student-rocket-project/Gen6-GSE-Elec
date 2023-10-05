#pragma once
#include <Arduino.h>


class ThermalMonitor {
  uint8_t _pinNumber;

  float _lowerResistance;

  const float T0 = 25.0 + 273.15;
  const float R0 = 10000.0;
  const float B = 3380.0;

public:
  ThermalMonitor(uint8_t pinNumber, float lowerResistance);

  float getTemperature_degC();
};
