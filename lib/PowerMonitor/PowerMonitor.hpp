#pragma once
#include <Arduino.h>
#include <Adafruit_INA219.h>


class PowerMonitor {
  Adafruit_INA219* _ina219;

public:
  PowerMonitor(uint8_t address);
  void begin();

  float getAmpere_A();
  float getVoltage_V();
  float getPower_W();
};
