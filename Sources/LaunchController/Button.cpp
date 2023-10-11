#include "Button.hpp"


/// @brief コンストラクタ
/// @param pinNumber ピン番号
Button::Button(uint8_t pinNumber) {
  _pin = new InputPin(pinNumber);
}


/// @brief ボタンが押されているかを返す
/// @return ボタンが押されているか
bool Button::isPushed() {
  return _pin->isHigh();
}
