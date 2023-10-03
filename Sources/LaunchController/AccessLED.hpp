#pragma once


#include <Arduino.h>
#include "OutputPin.hpp"


/// @brief 一瞬光るLED
class AccessLED {
public:
  /// @brief コンストラクタ
  /// @param pinNumber ピン番号
  AccessLED(uint8_t pinNumber);

  /// @brief 一瞬光らせる
  void blink();

private:
  OutputPin* _pin;
};
