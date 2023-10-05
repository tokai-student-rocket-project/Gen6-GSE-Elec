#pragma once
#include <Arduino.h>
#include <TaskManager.h>
#include "OutputPin.hpp"


/// @brief 手動と自動のオンオフ制御
class SemiAutoControl {
  OutputPin* _pin;
  String _audioTaskName;

  // 自動制御と手動制御の出力記憶しておく変数たち
  // ピンの実際の出力は以下の変数をOR演算する
  bool _autoIsHigh = false;
  bool _manualIsHigh = false;

  // 自動制御と手動制御の出力をもとにピンに出力する
  void set();

public:
  /// @brief コンストラクタ
  /// @param pinNumber ピン番号
  SemiAutoControl(uint8_t pinNumber, String audioTaskName);

  /// @brief 自動制御の出力を設定する
  void autoSet(bool isHigh);

  /// @brief 手動制御の出力を設定する
  void manualSet(bool isHigh);
};
