#pragma once
#include <Arduino.h>
#include <Adafruit_INA219.h>


class AmpereMonitor {
  Adafruit_INA219* _ina219;

public:
  AmpereMonitor(uint8_t address);

  void begin();
  float getAmpere_A();
};
