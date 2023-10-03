#pragma once


#include <Arduino.h>


/// @brief INPUT設定ピンの抽象的なクラス
class InputPin {
public:
  /// @brief コンストラクタ
  /// @param pinNumber ピン番号
  InputPin(uint8_t pinNumber);

  /// @brief 入力がHIGHかを返す
  /// @return 入力がHIGHか
  bool isHigh();

private:
  uint8_t _pinNumber;
};
