#include "OutputPin.hpp"


/// @brief コンストラクタ
/// @param pinNumber ピン番号
OutputPin::OutputPin(uint8_t pinNumber) {
  _pinNumber = pinNumber;
  pinMode(_pinNumber, OUTPUT);
}


/// @brief 出力をHIGHにする
void OutputPin::setHigh() {
  digitalWrite(_pinNumber, HIGH);
}


/// @brief 出力をLOWにする
void OutputPin::setLow() {
  digitalWrite(_pinNumber, LOW);
}


/// @brief 出力を入れ替える（HIGHならLOW LOWならHIGH）
void OutputPin::setToggle() {
  digitalWrite(_pinNumber, !digitalRead(_pinNumber));
}