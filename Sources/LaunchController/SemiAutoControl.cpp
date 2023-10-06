#include "SemiAutoControl.hpp"


// 自動制御と手動制御の出力をもとにピンに出力する
void SemiAutoControl::updateOutput() {
  _ledPin->set(_autoIsHigh || _manualIsHigh);
}


/// @brief コンストラクタ
/// @param controlPinNumber ボタンかスイッチのピン番号
/// @param ledPinNumber LEDのピン番号
/// @param onManualRisingTask 手動制御の立ち上がりエッジで実行するタスク名
SemiAutoControl::SemiAutoControl(uint8_t controlPinNumber, uint8_t ledPinNumber, String onManualRisingTask) {
  _buttonPin = new Button(controlPinNumber, false);
  _ledPin = new OutputPin(ledPinNumber);
  _onManualRisingTask = onManualRisingTask;
}


/// @brief 自動制御の出力をオンに設定する
void SemiAutoControl::setAutomaticOn() {
  _autoIsHigh = true;
  Serial.print(_onManualRisingTask);
  Serial.println(" ON");
  updateOutput();
}


/// @brief 自動制御の出力をオフに設定する
void SemiAutoControl::setAutomaticOff() {
  _autoIsHigh = false;
  Serial.print(_onManualRisingTask);
  Serial.println(" OFF");
  updateOutput();
}


/// @brief 手動制御の出力を設定する
void SemiAutoControl::setManual() {
  bool isHigh = _buttonPin->isPushed();

  // 立ち上がりエッジでタスクを実行する
  if (isHigh && !_manualIsHigh) {
    Tasks[_onManualRisingTask]->startOnceAfterMsec(200);
  }

  _manualIsHigh = isHigh;
  updateOutput();
}
