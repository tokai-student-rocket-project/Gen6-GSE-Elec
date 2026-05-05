#pragma once
#include <Arduino.h>
#include <Wire.h>

class Lib_24LC256 {
private:
  uint8_t _deviceAddress;

public:
  Lib_24LC256(uint8_t i2cAddress = 0x50);

  void begin();

  void writeByte(uint16_t memAdress, uint8_t data);

  uint8_t readByte(uint16_t memAdress);
};