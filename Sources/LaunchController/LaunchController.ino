#include <Arduino.h>
#include "Control.hpp"
#include "AccessLED.hpp"
#include "Button.hpp"
#include "AudioPlayer.hpp"


namespace control {
  Control power(A5);
}

namespace LED {
  AccessLED task(A12);
}

namespace button {
  Button kill(14, false);
}

namespace device {
  // AudioPlayer audioPlayer;
}


void setup() {
  control::power.turnOn();
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
