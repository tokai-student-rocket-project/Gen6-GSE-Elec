#pragma once
#include <Arduino.h>

/// @brief OUTPUT設定ピンの抽象的なクラス
class Output {
  bool _isHigh = false;
  bool _testIsHigh = false;
  unsigned long _lastPulseTime = 0;
  uint16_t _pulseDuration = 0;

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

  /// @brief 一定時間だけ点灯させる
  void pulse(uint16_t duration_ms);

  /// @brief 内部状態の更新（ループ内で呼ぶ必要がある）
  void update();

  bool isHigh();

  /// @brief テストの出力を設定する
  void setTestOn();
  void setTestOff();

private:
  uint8_t _pinNumber;
};
