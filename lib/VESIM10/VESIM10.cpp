#include <VESIM10.hpp>

VESIM10::VESIM10(uint8_t analogPinNumber, float shuntResistance_Ohm,
                 float fullScaleRange_MPa) {
  _analogPinNumber = analogPinNumber;
  _shuntResistance_Ohm = shuntResistance_Ohm;

  setFullScale(fullScaleRange_MPa);
}

void VESIM10::setFullScale(float fullScale_MPa) {
  _a = fullScale_MPa / 16.0;
  _b = -(fullScale_MPa / 4.0);
}

void VESIM10::setCalibration(float a, float b) {
  _a = a;
  _b = b;
}

void VESIM10::setDummyCurrent(float current_mA) {
  _isDummyMode = true;
  _dummyCurrent_mA = current_mA;
}

void VESIM10::disableDummy() { _isDummyMode = false; }

float VESIM10::getCurrent_mA() {
  if (_isDummyMode) {
    return _dummyCurrent_mA;
  }
  float voltage_V = (float)analogRead(_analogPinNumber) * 5.0 / 1024.0;
  return voltage_V / _shuntResistance_Ohm * 1000.0;
}

float VESIM10::getPressure_MPa() {
  float current = getCurrent_mA();
  // 運用(Hardware)とシミュレーション(Dummy)を切り分ける
  // DummyMode時は、calibrateBlocking等で取得された物理的なオフセットを無視する
  float effectiveCurrent =
      _isDummyMode ? current : (current - _offsetCurrent_mA);
  return _a * effectiveCurrent + _b;
}

void VESIM10::calibrateBlocking(uint8_t samplingCount) {
  float currentAverageBuffer_mA = 0.0;

  for (uint8_t i = 0; i < samplingCount; i++) {
    currentAverageBuffer_mA += getCurrent_mA();
    delay(100);
  }

  float averageCurrent_mA = currentAverageBuffer_mA / (float)samplingCount;

  _offsetCurrent_mA = averageCurrent_mA - _minOutputCurrent_mA;
  Serial.print(">offsetCurrent_mA: ");
  Serial.println(_offsetCurrent_mA);
}
