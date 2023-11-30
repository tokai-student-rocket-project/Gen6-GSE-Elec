#include <Arduino.h>
#include <TaskManager.h>
#include <MsgPacketizer.h>
#include "Input.hpp"
#include "Output.hpp"


namespace power {
  Input killButton(PIN_PJ1);
  Output loadSwitch(PIN_PF5);
} // namespace power


void setup() {
  power::loadSwitch.on();

  // FT232RL
  Serial.begin(115200);

  Tasks.add(&control::handleManualTask)->startFps(20);
}


void loop() {
  Tasks.update();
}


void control::handleManualTask() {
  if (power::killButton.isHigh()) {
    // 終了処理
    power::loadSwitch.off();
  }
}
