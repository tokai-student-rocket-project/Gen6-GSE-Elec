#include <Arduino.h>
#include <DFPlayer_Mini_Mp3.h>
#include "Control.hpp"
#include "AccessLED.hpp"
#include "Button.hpp"


namespace control {
  Control power(A5);
}

namespace LED {
  AccessLED task(A12);
}

namespace button {
  Button kill(14, false);
}


void setup() {
  control::power.turnOn();

  // FT232RL
  Serial.begin(115200);
  // DFPlayer
  Serial2.begin(9600);

  // HACK 動作確認用
  // mp3_set_serial(Serial2);
  // mp3_set_debug_serial(Serial);
  // mp3_set_volume(30);
  // mp3_play(1);
}


void loop() {
  if (button::kill.isPushed()) {
    // 終了処理
    control::power.turnOff();
  }

  // HACK TaskManagerを追加していないのでloop内で処理
  LED::task.blink();
  delay(100);
}
