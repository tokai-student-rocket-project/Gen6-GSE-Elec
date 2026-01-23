#pragma once

#include <Arduino.h>


class GseSignal {
public:
  GseSignal(uint8_t pinNumber);

  bool isSignaled();

private:
  uint8_t _pinNumber;
};
