#include "Button.hpp"


/// @brief コンストラクタ
/// @param pinNumber ピン番号
/// @param isExternalPullup 外部回路でプルアップしているか
Button::Button(uint8_t pinNumber, bool isExternalPullup) {
  _pin = new InputPin(pinNumber);
  _isExternalPullup = isExternalPullup;
}


/// @brief ボタンが押されているかを返す
/// @return ボタンが押されているか
bool Button::isPushed() {
  bool isHigh = _pin->isHigh();

  // プルアップしている時は信号が反転する（押されたらLowになる）
  return _isExternalPullup ? !isHigh : isHigh;
}
