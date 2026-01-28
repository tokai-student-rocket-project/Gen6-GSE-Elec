#pragma once

#include "Lib_FRAM.hpp"
#include <Arduino.h>

class Logger {
public:
  Logger(uint32_t csFram0);
  Logger(uint32_t csFram0, SPIClass *theSPI);
  Logger(uint32_t clk, uint32_t miso, uint32_t mosi, uint32_t csFram0);

  void reset();
  void dump();
  void clear();

  uint32_t write(const uint8_t *data, uint32_t size);

  uint32_t getOffset();
  float getUsage();
  uint8_t framNumber();

  FRAM *getFram() { return _fram0; }

private:
  uint32_t _offset = 0;

  FRAM *_fram0;
};