#pragma once
#include <Arduino.h>


class VoltageMonitor {
  uint8_t _pinNumber;

  float _scaleFactor = 0;

public:
  VoltageMonitor(uint8_t pinNumber, float upperResistance, float lowerResistance);

  float getVoltage_V();
};
