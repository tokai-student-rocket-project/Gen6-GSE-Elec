#pragma once
#include <Arduino.h>
#include "InputPin.hpp"


// "switch"は予約語で使えないので"button"という言葉を使っている

/// @brief 2状態のボタンかスイッチ
class Button {
public:
  /// @brief コンストラクタ
  /// @param pinNumber ピン番号
  /// @param isExternalPullup 外部回路でプルアップしているか
  Button(uint8_t pinNumber, bool isExternalPullup);

  /// @brief ボタンが押されているかを返す
  /// @return ボタンが押されているか
  bool isPushed();

private:
  InputPin* _pin;
  bool _isExternalPullup;
};
