#include <Arduino.h>
#include <TaskManager.h>
#include <MsgPacketizer.h>
#include "Input.hpp"
#include "Output.hpp"
#include "SolenoidMonitor.hpp"


Output armed(PIN_PH7);

Output shift(PIN_PD4);
Output fill(PIN_PD5);
Output dump(PIN_PG1);
// Output oxygen(PIN_PD5);
Output igniter(PIN_PD7);
Output open(PIN_PD6);
Output close(PIN_PG0);
Output purge(PIN_PB6);

Output sendEnableControl(PIN_PA2);

enum class Id : uint8_t {
  CONTROL
};

SolenoidMonitor solenoidMonitor(PIN_PC4);

namespace power {
  Input killButton(PIN_PJ1);
  Output loadSwitch(PIN_PF5);
} // namespace power

namespace umbilical {
  Output flightMode(PIN_PH3);
  Output valveMode(PIN_PH2);
} // namespace umbilical


void setup() {
  power::loadSwitch.on();

  // FT232RL (USB)
  Serial.begin(115200);

  // LTC485 (RS485)
  Serial1.begin(115200);

  // MCP3208 (ADC)
  SPI.begin();
  solenoidMonitor.setDividerResistance(5600, 3300);

  Tasks.add(&handleManualTask)->startFps(20);

  armed.on();

  MsgPacketizer::subscribe(Serial1, static_cast<uint8_t>(Id::CONTROL), &onControlReceived);
}


void loop() {
  MsgPacketizer::parse();
  Tasks.update();
}


void onControlReceived(uint8_t state) {
  shift.set(state & (1 << 0));
  fill.set(state & (1 << 1));
  dump.set(state & (1 << 2));
  // oxygen.set(state & (1 << 3));
  igniter.set(state & (1 << 4));
  open.set(state & (1 << 5));
  close.set(state & (1 << 6));
  purge.set(state & (1 << 7));
}


void handleManualTask() {
  if (power::killButton.isHigh()) {
    // 終了処理
    power::loadSwitch.off();
  }

  umbilical::flightMode.set(igniter.isHigh());
  umbilical::valveMode.set(open.isHigh() && !close.isHigh());
}
