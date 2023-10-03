#include "AccessLED.hpp"


/// @brief コンストラクタ
/// @param pinNumber ピン番号
AccessLED::AccessLED(uint8_t pinNumber) {
  _pin = new OutputPin(pinNumber);
  _pin->setLow();
}


/// @brief 一瞬光らせる
void AccessLED::blink() {
  // TODO 一定時間で消灯する処理を追加する（仮で点滅）
  _pin->setToggle();
}
