#pragma once
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>


class Neopixel {
  Adafruit_NeoPixel *_neopixel;

public:
  Neopixel(uint8_t pin);
  void init(uint8_t power);
  void off();
  void noticedPink();
  void noticedGreen();
  void noticedRed();
  void noticedBlue();
  void noticedWhite();
  void noticedTime(bool permiddionTime);
  void setHSV(uint16_t hue, uint8_t sat, uint8_t val);
  void show();
  void setBatteryStatus(float voltage);

private:
  uint8_t _power;
};