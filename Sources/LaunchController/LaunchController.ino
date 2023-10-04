#include <Arduino.h>
#include <TaskManager.h>
#include <DFPlayer_Mini_Mp3.h>
#include "Control.hpp"
#include "AccessLED.hpp"
#include "Button.hpp"


namespace control {
  Control power(PIN_PF5);
}

namespace indicator {
  AccessLED task(PIN_PK4);

  // LED
  Control emergencyStop(PIN_PG4);
  Control dump(PIN_PB5);
}

namespace button {
  Button kill(PIN_PJ1, false);
  Button emergencyStop(PIN_PC4, false);
}

namespace task {
  void handleManualControl();
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

  Tasks.add(&task::handleManualControl)->startFps(20);

  // Tasks.add<EmergencyStop>("EmergencyStop")
  //   ->sync<EmergencyStop>("Dump", [&](TaskRef<EmergencyStop> task) {
  //   task->dump();
  //     });
}


void loop() {
  Tasks.update();
}


void task::handleManualControl() {
  if (button::kill.isPushed()) {
    // 終了処理
    control::power.turnOff();
  }

  // HACK 仮エマスト
  if (button::emergencyStop.isPushed()) {
    indicator::emergencyStop.turnOn();
    indicator::dump.turnOn();
  }

  indicator::task.blink();
}