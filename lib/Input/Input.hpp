#pragma once
#include <Arduino.h>

/// @brief INPUT設定ピンの抽象的なクラス
class Input
{
public:
  /// @brief コンストラクタ
  /// @param pinNumber ピン番号
  Input(uint8_t pinNumber, bool hasExternalPullup);

  /// @brief 入力がHIGHかを返す
  /// @return 入力がHIGHか
  bool isHigh();

private:
  uint8_t _pinNumber;
  bool _hasExternalPullup;
};
