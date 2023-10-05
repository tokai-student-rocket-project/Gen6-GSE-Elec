#pragma once
#include <Arduino.h>


/// @brief OUTPUT設定ピンの抽象的なクラス
class OutputPin {
public:
  /// @brief コンストラクタ
  /// @param pinNumber ピン番号
  OutputPin(uint8_t pinNumber);

  /// @brief 出力をHIGHにする
  void setHigh();

  /// @brief 出力をLOWにする
  void setLow();

  /// @brief 出力を入れ替える（HIGHならLOW LOWならHIGH）
  void setToggle();

  /// @brief 出力を設定する
  void set(bool isHigh);

private:
  uint8_t _pinNumber;
};
