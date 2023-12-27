#pragma once
#include <Arduino.h>
#include <TaskManager.h>
#include "Output.hpp"


/// @brief 手動と自動のオンオフ制御
class AutoControl {
  Output* _ledPin;

  // 自動制御と手動制御の出力記憶しておく変数たち
  // ピンの実際の出力は以下の変数をOR演算する
  bool _autoIsHigh = false;
  bool _testIsHigh = false;

  // 自動制御と手動制御の出力をもとにピンに出力する
  void updateOutput();

public:
  /// @brief コンストラクタ
  /// @param buttonPinNumber ボタンかスイッチのピン番号
  /// @param ledPinNumber LEDのピン番号
  AutoControl(uint8_t ledPinNumber);

  /// @brief 自動制御の出力を設定する
  void setAutomaticOn();
  void setAutomaticOff();
  void setAutomaticToggle();
  void setAutomatic(bool isHigh);

  /// @brief テストの出力を設定する
  void setTestOn();
  void setTestOff();

  bool isAutomaticRaised();
  bool isRaised();
};
