#pragma once

#include <Arduino.h>


class VESIM10 {
public:
  VESIM10(uint8_t analogPinNumber, float shuntResistance_Ohm, float fullScaleRange_MPa);

  float getCurrent_mA();
  float getPressure_MPa();
  void calibrateBlocking(uint8_t samplingCount);

private:
  uint8_t _analogPinNumber;

  float _shuntResistance_Ohm;

  float _maxOutputCurrent_mA = 20.0;
  float _minOutputCurrent_mA = 4.0;

  // y = a*x + b
  float _a;
  float _b;

  float _offsetCurrent_mA = 0;
};
