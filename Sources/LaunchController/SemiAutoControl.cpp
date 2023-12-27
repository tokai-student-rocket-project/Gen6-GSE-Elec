#include "SemiAutoControl.hpp"


// 自動制御と手動制御の出力をもとにピンに出力する
void SemiAutoControl::updateOutput() {
  _ledPin->set(_autoIsHigh || _manualIsHigh || _testIsHigh);
}


/// @brief コンストラクタ
/// @param controlPinNumber ボタンかスイッチのピン番号
/// @param ledPinNumber LEDのピン番号
SemiAutoControl::SemiAutoControl(uint8_t controlPinNumber, uint8_t ledPinNumber) {
  _buttonPin = new Input(controlPinNumber);
  _ledPin = new Output(ledPinNumber);
}


/// @brief 自動制御の出力をオンに設定する
void SemiAutoControl::setAutomaticOn() {
  _autoIsHigh = true;
  updateOutput();
}


/// @brief 自動制御の出力をオフに設定する
void SemiAutoControl::setAutomaticOff() {
  _autoIsHigh = false;
  updateOutput();
}


/// @brief 手動制御の出力を設定する
void SemiAutoControl::setManual() {
  _manualIsHigh = _buttonPin->isHigh();
  updateOutput();
}


/// @brief テストの出力をオンに設定する
void SemiAutoControl::setTestOn() {
  _testIsHigh = true;
  updateOutput();
}


/// @brief テストの出力をオフに設定する
void SemiAutoControl::setTestOff() {
  _testIsHigh = false;
  updateOutput();
}


bool SemiAutoControl::isManualRaised() {
  return _manualIsHigh;
}


bool SemiAutoControl::isAutomaticRaised() {
  return _autoIsHigh;
}


bool SemiAutoControl::isRaised() {
  return _autoIsHigh || _manualIsHigh || _testIsHigh;
}
