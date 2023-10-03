#pragma once


#include <Arduino.h>


class Player {
public:
  Player(Serial pinNumber);

  bool isHigh();

private:
  Serial _serial;
};
