#pragma once
#include <Arduino.h>
#include <TaskManager.h>
#include "Button.hpp"
#include "OutputPin.hpp"


/// @brief 手動と自動のオンオフ制御
class SemiAutoControl {
  Button* _buttonPin;
  OutputPin* _ledPin;

  // 自動制御と手動制御の出力記憶しておく変数たち
  // ピンの実際の出力は以下の変数をOR演算する
  bool _autoIsHigh = false;
  bool _manualIsHigh = false;
  bool _testIsHigh = false;

  // 自動制御と手動制御の出力をもとにピンに出力する
  void updateOutput();

public:
  /// @brief コンストラクタ
  /// @param buttonPinNumber ボタンかスイッチのピン番号
  /// @param ledPinNumber LEDのピン番号
  SemiAutoControl(uint8_t buttonPinNumber, uint8_t ledPinNumber);

  /// @brief 自動制御の出力を設定する
  void setAutomaticOn();
  void setAutomaticOff();
  void setAutomaticToggle();

  /// @brief 手動制御の出力を設定する
  void setManual();

  /// @brief テストの出力を設定する
  void setTestOn();
  void setTestOff();

  bool isManualRaised();
};
