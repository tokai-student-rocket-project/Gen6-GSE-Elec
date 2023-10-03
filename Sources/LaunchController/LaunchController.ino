#include <Arduino.h>
#include "Control.hpp"
#include "AccessLED.hpp"
#include "Button.hpp"


namespace control {
  Control* power = new Control(A5);
}

namespace LED {
  AccessLED* task = new AccessLED(A12);
}

namespace button {
  Button* kill = new Button(14, false);
}


void setup() {
  control::power->turnOn();
}


void loop() {
  if (button::kill->isPushed()) {
    // 終了処理
    control::power->turnOff();
  }

  // HACK TaskManagerを追加していないのでloop内で処理
  LED::task->blink();
  delay(100);
}
