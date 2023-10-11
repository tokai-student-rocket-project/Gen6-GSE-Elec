#pragma once
#include <Arduino.h>


/// @brief OUTPUT設定ピンの抽象的なクラス
class Output {
  bool _isHigh = false;
  bool _testIsHigh = false;

  void updateOutput();

public:
  /// @brief コンストラクタ
  /// @param pinNumber ピン番号
  Output(uint8_t pinNumber);

  /// @brief 出力をHIGHにする
  void on();

  /// @brief 出力をLOWにする
  void off();

  /// @brief 出力を入れ替える（HIGHならLOW LOWならHIGH）
  void toggle();

  /// @brief 一瞬だけON
  void blink();

  /// @brief 出力を設定する
  void set(bool isHigh);

  /// @brief テストの出力を設定する
  void setTestOn();
  void setTestOff();

private:
  uint8_t _pinNumber;
};
