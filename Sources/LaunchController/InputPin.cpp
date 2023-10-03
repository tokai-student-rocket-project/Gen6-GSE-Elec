#include "InputPin.hpp"


/// @brief コンストラクタ
/// @param pinNumber ピン番号
InputPin::InputPin(uint8_t pinNumber) {
  _pinNumber = pinNumber;
  pinMode(_pinNumber, INPUT);
}


/// @brief 入力がHIGHかを返す
/// @return 入力がHIGHか
bool InputPin::isHigh() {
  return digitalRead(_pinNumber) == HIGH;
}
