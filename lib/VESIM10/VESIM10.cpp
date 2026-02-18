#include "VESIM10.hpp"

VESIM10::VESIM10(uint8_t analogPinNumber, float shuntResistance_Ohm,
                 float fullScaleRange_MPa)
{
  _analogPinNumber = analogPinNumber;
  _shuntResistance_Ohm = shuntResistance_Ohm;

  setFullScale(fullScaleRange_MPa);
}

void VESIM10::setFullScale(float fullScale_MPa)
{
  _a = fullScale_MPa / 16.0;
  _b = -(fullScale_MPa / 4.0);
}

void VESIM10::setCalibration(float a, float b)
{
  _a = a;
  _b = b;
}

void VESIM10::setDummyCurrent(float current_mA)
{
  _isDummyMode = true;
  _dummyCurrent_mA = current_mA;
}

void VESIM10::disableDummy() { _isDummyMode = false; }

void VESIM10::setFilterCoefficient(float k)
{
  if (k > 0.0 && k <= 1.0)
  {
    _k = k;
  }
}

void VESIM10::sample()
{
  if (_isDummyMode)
    return;
  _adcSum += analogRead(_analogPinNumber);
  _adcCount++;
}

float VESIM10::read(bool raw)
{
  float currentRaw_mA = 0.0;
  if (_isDummyMode)
  {
    currentRaw_mA = _dummyCurrent_mA;
  }
  else
  {
    if (_adcCount > 0)
    {
      float avgAdc = (float)_adcSum / (float)_adcCount;
      float voltage_V = avgAdc * 5.0 / 1024.0;
      currentRaw_mA = voltage_V / _shuntResistance_Ohm * 1000.0;

      // アキュムレーターをリセット
      _adcSum = 0;
      _adcCount = 0;
    }
    else
    {
      // サンプルが無かったらそのまま返す
      currentRaw_mA = _rawCurrent_mA;
    }
  }

  _rawCurrent_mA = currentRaw_mA;

  // 1次遅れフィルタ (LPF)
  if (_filteredCurrent_mA < 0)
  {
    _filteredCurrent_mA = currentRaw_mA; // 初回は直接代入
  }
  else
  {
    _filteredCurrent_mA = (1.0 - _k) * _filteredCurrent_mA + _k * currentRaw_mA;
  }

  return raw ? _rawCurrent_mA : _filteredCurrent_mA;
}

float VESIM10::getCurrent_mA(bool raw)
{
  return raw ? _rawCurrent_mA : _filteredCurrent_mA;
}

float VESIM10::getPressure_MPa()
{
  float current = _filteredCurrent_mA;
  // 運用(Hardware)とシミュレーション(Dummy)を切り分ける
  // DummyMode時は、calibrateBlocking等で取得された物理的なオフセットを無視する
  float effectiveCurrent =
      _isDummyMode ? current : (current - _offsetCurrent_mA);
  return _a * effectiveCurrent + _b;
}

void VESIM10::calibrateBlocking(uint8_t samplingCount)
{
  float currentAverageBuffer_mA = 0.0;

  for (uint8_t i = 0; i < samplingCount; i++)
  {
    sample();
    currentAverageBuffer_mA += read(true); // 校正時は生の値を使用
    delay(10);
  }

  float averageCurrent_mA = currentAverageBuffer_mA / (float)samplingCount;

  _offsetCurrent_mA = averageCurrent_mA - _minOutputCurrent_mA;
  Serial.print(">offsetCurrent_mA: ");
  Serial.println(_offsetCurrent_mA);
}
