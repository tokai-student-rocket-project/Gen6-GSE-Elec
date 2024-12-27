#pragma once

#include <Arduino.h>

class TM1637
{
public:
  TM1637(uint8_t clockPinNumber, uint8_t dataPinNumber);
  void initialize();

  void clearDisplay();
  void displayNumber(float value);

private:
  int8_t numberSegments[10] = {
      0b0011'1111,
      0b0000'0110,
      0b0101'1011,
      0b0100'1111,
      0b0110'0110,
      0b0110'1101,
      0b0111'1101,
      0b0000'0111,
      0b0111'1111,
      0b0110'1111,
  };

  uint8_t _clockPinNumber;
  uint8_t _dataPinNumber;

  void start();
  void stop();

  void writeByte(int8_t data);
  int8_t segment(int8_t data, bool includesDot);
  void display(uint8_t address, int8_t data);

  void bitDelay();
};
