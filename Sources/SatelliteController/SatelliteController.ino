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

  MsgPacketizer::subscribe(Serial1, static_cast<uint8_t>(0xAA),
    [](bool fillIsRaised) {
      Serial.println(fillIsRaised);
      fill.set(fillIsRaised);
    }
  );
}


void loop() {
  MsgPacketizer::parse();
  Tasks.update();
}


void handleManualTask() {
  if (power::killButton.isHigh()) {
    // 終了処理
    power::loadSwitch.off();
  }

  // umbilical::flightMode.set(umbilical::igniterSwitch.isHigh());
  // umbilical::valveMode.set(umbilical::openSwitch.isHigh() && !umbilical::closeSwitch.isHigh());
}
