#include "Control.hpp"


/// @brief コンストラクタ
/// @param pinNumber ピン番号
Control::Control(uint8_t pinNumber) {
  _pin = new OutputPin(pinNumber);
  _pin->setLow();
}


/// @brief ONにする
void Control::turnOn() {
  _pin->setHigh();
}


/// @brief OFFにする
void Control::turnOff() {
  _pin->setLow();
}
