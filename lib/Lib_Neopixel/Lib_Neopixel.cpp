#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <Lib_Neopixel.hpp>

Neopixel::Neopixel(uint8_t pin) {
  _neopixel = new Adafruit_NeoPixel(1, pin, NEO_GRB + NEO_KHZ800);
}

void Neopixel::init(uint8_t power) {
  _power = power;
  pinMode(_power, OUTPUT);
  digitalWrite(_power, HIGH);
  _neopixel->begin();
  _neopixel->show();
}

void Neopixel::off() {
  _mode = Mode::STATIC;
  _neopixel->clear();
  _neopixel->setPixelColor(0, _neopixel->Color(0, 0, 0));
  _neopixel->show();
}

void Neopixel::noticedPink() {
  _mode = Mode::STATIC;
  _neopixel->setPixelColor(0, _neopixel->Color(255, 51, 255));
  _neopixel->show();
}

void Neopixel::noticedGreen() {
  _mode = Mode::STATIC;
  _neopixel->setPixelColor(0, _neopixel->Color(0, 150, 0));
  _neopixel->show();
}

void Neopixel::noticedBlue() {
  _mode = Mode::STATIC;
  _neopixel->setPixelColor(0, _neopixel->Color(0, 0, 150));
  _neopixel->show();
}

void Neopixel::noticedRed() {
  _mode = Mode::STATIC;
  _neopixel->setPixelColor(0, _neopixel->Color(255, 0, 0));
  _neopixel->show();
}

void Neopixel::noticedWhite() {
  _mode = Mode::STATIC;
  _neopixel->setPixelColor(0, _neopixel->Color(255, 255, 255));
  _neopixel->show();
}

void Neopixel::noticedTime(bool permissionTime) {
  if (permissionTime) {
    _mode = Mode::STATIC;
    _neopixel->setPixelColor(0, _neopixel->Color(255, 0, 0));
    _neopixel->show();
  }
}

void Neopixel::setHSV(uint16_t hue, uint8_t sat, uint8_t val) {
  _mode = Mode::STATIC;
  _neopixel->setPixelColor(0, _neopixel->ColorHSV(hue, sat, val));
  _neopixel->show();
}

void Neopixel::show() { _neopixel->show(); }

void Neopixel::setBatteryStatus(float voltage) {
  _mode = Mode::STATIC;
  if (voltage >= 13.1) {
    _neopixel->setPixelColor(0, _neopixel->Color(0, 255, 0));
  } else if (voltage >= 11.5) {
    _neopixel->setPixelColor(0, _neopixel->Color(255, 165, 0));
  } else {
    _neopixel->setPixelColor(0, _neopixel->Color(255, 0, 0));
  }
  _neopixel->show();
}

void Neopixel::setMode(Mode mode) {
  _mode = mode;
}

void Neopixel::setBlink(uint32_t color1, uint32_t color2, uint32_t intervalMs) {
  _mode = Mode::BLINK;
  _blinkColor1 = color1;
  _blinkColor2 = color2;
  _blinkIntervalMs = intervalMs;
}

void Neopixel::setBlink(uint32_t color, uint32_t intervalMs) {
  setBlink(color, _neopixel->Color(0, 0, 0), intervalMs);
}

void Neopixel::setBreathing(uint16_t hue, uint8_t sat, uint32_t periodMs) {
  _mode = Mode::BREATHING;
  _breathHue = hue;
  _breathSat = sat;
  _breathPeriodMs = periodMs;
}

// 便利なアニメーション付きステータス表示の追加
void Neopixel::noticedGreenBlink(uint32_t intervalMs) {
  setBlink(_neopixel->Color(0, 150, 0), intervalMs);
}

void Neopixel::noticedGreenBreath(uint32_t periodMs) {
  setBreathing(21845, 255, periodMs); // Green
}

void Neopixel::noticedRedBlink(uint32_t intervalMs) {
  setBlink(_neopixel->Color(255, 0, 0), intervalMs);
}

void Neopixel::noticedRedBreath(uint32_t periodMs) {
  setBreathing(0, 255, periodMs); // Red
}

void Neopixel::noticedBlueBlink(uint32_t intervalMs) {
  setBlink(_neopixel->Color(0, 0, 150), intervalMs);
}

void Neopixel::noticedBlueBreath(uint32_t periodMs) {
  setBreathing(43690, 255, periodMs); // Blue
}

void Neopixel::update() {
  uint32_t now = millis();

  switch (_mode) {
    case Mode::STATIC:
      // 静的モードの場合は状態を変更しない
      break;

    case Mode::BLINK: {
      bool state = (now / _blinkIntervalMs) % 2;
      _neopixel->setPixelColor(0, state ? _blinkColor1 : _blinkColor2);
      _neopixel->show();
      break;
    }

    case Mode::BREATHING: {
      // 三角波またはサイン波でフェード（ホタル）制御を行う
      float phase = (float)(now % _breathPeriodMs) / _breathPeriodMs;
      float brightness = (sin(phase * 2.0 * PI - PI / 2.0) + 1.0) / 2.0;
      
      // 眩しすぎないよう最大輝度値を180に調整
      uint8_t val = (uint8_t)(brightness * 180);
      _neopixel->setPixelColor(0, _neopixel->ColorHSV(_breathHue, _breathSat, val));
      _neopixel->show();
      break;
    }
  }
}
