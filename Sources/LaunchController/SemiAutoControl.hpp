#pragma once
#include <Arduino.h>
#include <TaskManager.h>
#include "Button.hpp"
#include "OutputPin.hpp"


/// @brief 手動と自動のオンオフ制御
class SemiAutoControl {
  Button* _buttonPin;
  OutputPin* _ledPin;
  String _onManualRisingTask;

  // 自動制御と手動制御の出力記憶しておく変数たち
  // ピンの実際の出力は以下の変数をOR演算する
  bool _autoIsHigh = false;
  bool _manualIsHigh = false;

  // 自動制御と手動制御の出力をもとにピンに出力する
  void updateOutput();

public:
  /// @brief コンストラクタ
  /// @param buttonPinNumber ボタンかスイッチのピン番号
  /// @param ledPinNumber LEDのピン番号
  /// @param onManualRisingTask 手動制御の立ち上がりエッジで実行するタスク名
  SemiAutoControl(uint8_t buttonPinNumber, uint8_t ledPinNumber, String onManualRisingTask);

  /// @brief 自動制御の出力を設定する
  void setAutomatic(bool isHigh);

  /// @brief 手動制御の出力を設定する
  void setManual();
};
