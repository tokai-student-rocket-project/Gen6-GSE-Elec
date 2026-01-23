#pragma once

#include <Arduino.h>
#include <SPI.h>


class FRAM {
public:
  static const uint32_t LENGTH = 1048576;

  FRAM(uint32_t cs);

  void setWriteEnable();
  void getStatus(uint8_t* buffer);
  void getId(uint8_t* buffer);

  uint8_t read(uint32_t address);
  void write(uint32_t address, uint8_t data);
  void write(uint32_t address, const uint8_t* data, uint32_t size);

  void clear();
  void dump();

private:
  typedef enum {
    WREN = 0b00000110,
    RDSR = 0b00000101,
    READ = 0b00000011,
    WRITE = 0b00000010,
    RDID = 0b10011111
  } ope_code_t;

  SPISettings _setting;
  uint32_t _cs;
};