#include "AutoControl.hpp"


// 自動制御と手動制御の出力をもとにピンに出力する
void AutoControl::updateOutput() {
  _ledPin->set(_autoIsHigh || _testIsHigh);
}


/// @brief コンストラクタ
/// @param controlPinNumber ボタンかスイッチのピン番号
/// @param ledPinNumber LEDのピン番号
AutoControl::AutoControl(uint8_t ledPinNumber) {
  _ledPin = new Output(ledPinNumber);
}


/// @brief 自動制御の出力をオンに設定する
void AutoControl::setAutomaticOn() {
  _autoIsHigh = true;
  updateOutput();
}


/// @brief 自動制御の出力をオフに設定する
void AutoControl::setAutomaticOff() {
  _autoIsHigh = false;
  updateOutput();
}


void AutoControl::setAutomatic(bool isHigh) {
  _autoIsHigh = isHigh;
  updateOutput();
}


/// @brief テストの出力をオンに設定する
void AutoControl::setTestOn() {
  _testIsHigh = true;
  updateOutput();
}


/// @brief テストの出力をオフに設定する
void AutoControl::setTestOff() {
  _testIsHigh = false;
  updateOutput();
}


bool AutoControl::isAutomaticRaised() {
  return _autoIsHigh;
}


bool AutoControl::isRaised() {
  return _autoIsHigh;
}
