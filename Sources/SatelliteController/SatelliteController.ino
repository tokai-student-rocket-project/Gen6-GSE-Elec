#include <Arduino.h>
#include <TaskManager.h>
#include <MsgPacketizer.h>
#include "Input.hpp"
#include "Output.hpp"


namespace power {
  Input killButton(PIN_PJ1);
  Output loadSwitch(PIN_PF5);
} // namespace power

namespace umbilical {
  Input igniterSwitch(PIN_PD7);
  Input openSwitch(PIN_PD6);
  Input closeSwitch(PIN_PG0);

  Output flightMode(PIN_PH3);
  Output valveMode(PIN_PH2);
} // namespace umbilical


void setup() {
  power::loadSwitch.on();

  // FT232RL
  Serial.begin(115200);

  Tasks.add(&handleManualTask)->startFps(20);
}


void loop() {
  Tasks.update();
}


void handleManualTask() {
  if (power::killButton.isHigh()) {
    // 終了処理
    power::loadSwitch.off();
  }

  umbilical::flightMode.set(umbilical::igniterSwitch.isHigh());
  umbilical::valveMode.set(umbilical::openSwitch.isHigh() && !umbilical::closeSwitch.isHigh());
}
