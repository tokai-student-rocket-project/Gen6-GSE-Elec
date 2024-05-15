#include "VESIM10.hpp"


VESIM10::VESIM10(uint8_t analogPinNumber, float shuntResistance_Ohm, float fullScaleRange_MPa) {
  _analogPinNumber = analogPinNumber;
  _shuntResistance_Ohm = shuntResistance_Ohm;

  _a = fullScaleRange_MPa / (_maxOutputCurrent_mA - _minOutputCurrent_mA);
  _b = -(_a * _minOutputCurrent_mA);
}


float VESIM10::getCurrent_mA() {
  float voltage_V = (float)analogRead(_analogPinNumber) * 5.0 / 1024.0;
  return voltage_V / _shuntResistance_Ohm * 1000.0;
}


float VESIM10::getPressure_MPa() {
  return _a * (getCurrent_mA() - _offsetCurrent_mA) + _b;
}


void VESIM10::calibrateBlocking(uint8_t samplingCount) {
  float currentAverageBuffer_mA = 0.0;

  for (uint8_t i = 0; i < samplingCount; i++) {
    currentAverageBuffer_mA += getCurrent_mA();
    delay(100);
  }

  float averageCurrent_mA = currentAverageBuffer_mA / (float)samplingCount;

  _offsetCurrent_mA = averageCurrent_mA - _minOutputCurrent_mA;
}
