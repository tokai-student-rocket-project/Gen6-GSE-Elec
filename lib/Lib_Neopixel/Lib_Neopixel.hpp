#pragma once
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

class Neopixel {
public:
  enum class Mode {
    STATIC,
    BLINK,
    BREATHING
  };

  Neopixel(uint8_t pin);
  void init(uint8_t power);
  void off();

  // 既存のメソッド
  void noticedPink();
  void noticedGreen();
  void noticedRed();
  void noticedBlue();
  void noticedWhite();
  void noticedTime(bool permissionTime);
  void setHSV(uint16_t hue, uint8_t sat, uint8_t val);
  void show();
  void setBatteryStatus(float voltage);

  // 高度な制御用の新メソッド
  void setMode(Mode mode);
  void setBlink(uint32_t color1, uint32_t color2, uint32_t intervalMs);
  void setBlink(uint32_t color, uint32_t intervalMs);
  void setBreathing(uint16_t hue, uint8_t sat, uint32_t periodMs);
  
  // 便利なアニメーション付きステータス表示
  void noticedGreenBlink(uint32_t intervalMs = 500);
  void noticedGreenBreath(uint32_t periodMs = 2000);
  void noticedRedBlink(uint32_t intervalMs = 500);
  void noticedRedBreath(uint32_t periodMs = 2000);
  void noticedBlueBlink(uint32_t intervalMs = 500);
  void noticedBlueBreath(uint32_t periodMs = 2000);
  
  // TaskManagerから周期的に呼び出す更新処理 (例: 50Hzで実行)
  void update();

private:
  Adafruit_NeoPixel *_neopixel;
  uint8_t _power;

  Mode _mode = Mode::STATIC;
  
  // 点滅用の状態管理
  uint32_t _blinkColor1 = 0;
  uint32_t _blinkColor2 = 0;
  uint32_t _blinkIntervalMs = 500;
  
  // 呼吸・ホタル用の状態管理
  uint16_t _breathHue = 0;
  uint8_t _breathSat = 255;
  uint32_t _breathPeriodMs = 2000;
};