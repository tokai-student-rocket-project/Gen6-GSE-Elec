#pragma once


#include <Arduino.h>
#include "OutputPin.hpp"


/// @brief オンオフ制御
class Control {
public:
  /// @brief コンストラクタ
  /// @param pinNumber ピン番号
  Control(uint8_t pinNumber);

  /// @brief ONにする
  void turnOn();

  /// @brief OFFにする
  void turnOff();

protected:
  OutputPin* _pin;
};
