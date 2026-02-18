#pragma once

#include <Arduino.h>

class VESIM10
{
public:
  VESIM10(uint8_t analogPinNumber, float shuntResistance_Ohm,
          float fullScaleRange_MPa);

  float read(bool raw = false);
  void sample();
  float getCurrent_mA(bool raw = false);
  float getPressure_MPa();
  void calibrateBlocking(uint8_t samplingCount);

  void setFilterCoefficient(float k);

  void setFullScale(float fullScale_MPa);
  void setCalibration(float a, float b);

  void setDummyCurrent(float current_mA);
  void disableDummy();

private:
  uint8_t _analogPinNumber;

  float _shuntResistance_Ohm;

  float _maxOutputCurrent_mA = 20.0;
  float _minOutputCurrent_mA = 4.0;

  // y = a*x + b
  float _a;
  float _b;

  float _offsetCurrent_mA = 0;

  float _rawCurrent_mA = 4.0;       // 直近の生データ(mA)
  float _k = 0.2;                   // フィルタ係数 (0.0 < k <= 1.0)
  float _filteredCurrent_mA = -1.0; // 初回フラグ兼フィルタ後の値

  uint32_t _adcSum = 0;   // ADC累積値
  uint16_t _adcCount = 0; // 累積回数

  bool _isDummyMode = false;
  float _dummyCurrent_mA = 0.0;
};
