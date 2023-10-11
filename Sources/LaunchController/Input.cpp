#include "Input.hpp"


/// @brief コンストラクタ
/// @param pinNumber ピン番号
Input::Input(uint8_t pinNumber) {
  _pinNumber = pinNumber;
  pinMode(_pinNumber, INPUT);
}


/// @brief 入力がHIGHかを返す
/// @return 入力がHIGHか
bool Input::isHigh() {
  return digitalRead(_pinNumber) == HIGH;
}
