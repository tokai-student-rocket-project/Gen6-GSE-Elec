#include "SemiAutoControl.hpp"


// 自動制御と手動制御の出力をもとにピンに出力する
void SemiAutoControl::set() {
  _pin->set(_autoIsHigh || _manualIsHigh);
}



/// @brief コンストラクタ
/// @param pinNumber ピン番号
SemiAutoControl::SemiAutoControl(uint8_t pinNumber) {
  _pin = new OutputPin(pinNumber);
  _pin->setLow();
}


/// @brief 自動制御の出力を設定する
void SemiAutoControl::autoSet(bool isHigh) {
  _autoIsHigh = isHigh;
  set();
}


/// @brief 手動制御の出力を設定する
void SemiAutoControl::manualSet(bool isHigh) {
  _manualIsHigh = isHigh;
  set();
}
