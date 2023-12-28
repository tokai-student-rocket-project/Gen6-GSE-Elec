#include <Arduino.h>
#include <TaskManager.h>
#include <MsgPacketizer.h>
#include "Input.hpp"
#include "Output.hpp"
#include "SemiAutoControl.hpp"
#include "SolenoidMonitor.hpp"
#include "PowerMonitor.hpp"
#include "Thermistor.hpp"


namespace power {
  Input killButton(PIN_PJ1, false);
  Output loadSwitch(PIN_PF5);
  Output lowVoltageLamp(PIN_PK7);

  PowerMonitor input(0x40);
  PowerMonitor bus12(0x41);
  Thermistor thermal(PIN_PF4, 10000.0);

  void measureTask();
} // namespace power

namespace control {
  SemiAutoControl safetyArmed(PIN_PC2, true, PIN_PH7);

  Output shift(PIN_PD4);
  Output fill(PIN_PD5);
  Output dump(PIN_PG1);
  Output oxygen(PIN_PL6);
  Output igniter(PIN_PD7);
  Output open(PIN_PD6);
  Output close(PIN_PG0);
  SemiAutoControl purge(PIN_PC0, true, PIN_PB6);

  Output statusLamp(PIN_PK4);
  void handleManualTask();

  void setChristmasTreeStart();
  void setChristmasTreeStop();
} // namespace control

namespace error {
  // HACK LEDだけでなく処理もする
  Output statusLamp(PIN_PK6);
} // namespace caution

namespace solenoid {
  SolenoidMonitor monitor(PIN_PC4);

  void measureTask();
} // namespace solenoid

namespace umbilical {
  Output flightMode(PIN_PH3);
  Output valveMode(PIN_PH2);
} // namespace umbilical

namespace communication {
  enum class Packet : uint8_t {
    CONTROL_SYNC,
    COM_CHECK_L_TO_S,
    COM_CHECK_S_TO_L
  };

  Output sendEnableControl(PIN_PA2);
  Output accessLamp(PIN_PA4);

  void enableOutput();
  void disableOutput();

  void sendComCheck();
  void onControlSyncReceived(uint8_t state);
  void onComCheckReceived();

  Output statusLamp(PIN_PK5);
} // namespace communication


void setup() {
  power::loadSwitch.on();


  // FT232RL (USB)
  Serial.begin(115200);

  // LTC485 (RS485)
  Serial1.begin(115200);

  // MCP3208 (ADC)
  SPI.begin();
  solenoid::monitor.setDividerResistance(5600, 3300);

  // INA219 (Power)
  Wire.begin();
  power::input.begin();
  power::bus12.begin();


  Tasks.add(&power::measureTask)->startFps(10);
  Tasks.add(&solenoid::measureTask)->startFps(10);
  Tasks.add(&control::handleManualTask)->startFps(50);


  Tasks.add(&communication::sendComCheck)->startFps(2);
  MsgPacketizer::subscribe(Serial1, static_cast<uint8_t>(communication::Packet::CONTROL_SYNC), &communication::onControlSyncReceived);
  MsgPacketizer::subscribe(Serial1, static_cast<uint8_t>(communication::Packet::COM_CHECK_L_TO_S), &communication::onComCheckReceived);


  control::setChristmasTreeStart();
  Tasks.add(&control::setChristmasTreeStop)->startOnceAfterSec(3.0);
}


void loop() {
  MsgPacketizer::parse();
  Tasks.update();
}


/// @brief 送信を有効にする
void communication::enableOutput() {
  communication::sendEnableControl.on();
  communication::accessLamp.on();
}


/// @brief 送信を無効にする
void communication::disableOutput() {
  communication::sendEnableControl.off();
  communication::accessLamp.off();
}


void power::measureTask() {
  bool isLowVoltage = power::input.getVoltage_V() < 17.0;
  bool isOverloadedInput = power::input.getAmpere_A() > 3.0;
  bool isOverloadedBus = power::bus12.getAmpere_A() > 3.0;
  bool isOverheated = power::thermal.getTemperature_degC() > 100.0;

  power::lowVoltageLamp.set(isLowVoltage);

  if (isOverloadedInput || isOverloadedBus || isOverheated) {
    // HACK エラー
    error::statusLamp.on();
  }
}


void solenoid::measureTask() {
}


void communication::sendComCheck() {
  communication::enableOutput();
  MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::COM_CHECK_S_TO_L));
  Serial1.flush();
  communication::disableOutput();
}


void communication::onControlSyncReceived(uint8_t state) {
  control::shift.set(state & (1 << 0) && control::safetyArmed.isManualRaised());
  control::fill.set(state & (1 << 1) && control::safetyArmed.isManualRaised());
  control::dump.set(state & (1 << 2) && control::safetyArmed.isManualRaised());
  control::oxygen.set(state & (1 << 3) && control::safetyArmed.isManualRaised());
  control::igniter.set(state & (1 << 4) && control::safetyArmed.isManualRaised());
  control::open.set(state & (1 << 5) && control::safetyArmed.isManualRaised());
  control::close.set(state & (1 << 6) && control::safetyArmed.isManualRaised());
  control::purge.setAutomatic(state & (1 << 7) && control::safetyArmed.isManualRaised());
}


void communication::onComCheckReceived() {
  communication::statusLamp.blink();
}


void control::handleManualTask() {
  control::statusLamp.blink();

  if (power::killButton.isHigh()) {
    // 終了処理
    power::loadSwitch.off();
  }

  // セーフティー
  // Armedでなければこの時点で終わり
  control::safetyArmed.setManual();
  if (!control::safetyArmed.isManualRaised()) {
    return;
  }


  // 手動制御
  control::purge.setManual();

  // アンビリカル
  umbilical::flightMode.set(control::igniter.isHigh());
  umbilical::valveMode.set(control::open.isHigh() && !control::close.isHigh());
}


void control::setChristmasTreeStart() {
  error::statusLamp.setTestOn();
  power::lowVoltageLamp.setTestOn();
  control::statusLamp.setTestOn();
  communication::accessLamp.setTestOn();
  communication::statusLamp.setTestOn();
  control::safetyArmed.setTestOn();
  control::shift.setTestOn();
  control::fill.setTestOn();
  control::dump.setTestOn();
  control::oxygen.setTestOn();
  control::igniter.setTestOn();
  control::open.setTestOn();
  control::close.setTestOn();
  control::purge.setTestOn();
}


void control::setChristmasTreeStop() {
  error::statusLamp.setTestOff();
  power::lowVoltageLamp.setTestOff();
  control::statusLamp.setTestOff();
  communication::accessLamp.setTestOff();
  communication::statusLamp.setTestOff();
  control::safetyArmed.setTestOff();
  control::shift.setTestOff();
  control::fill.setTestOff();
  control::dump.setTestOff();
  control::oxygen.setTestOff();
  control::igniter.setTestOff();
  control::open.setTestOff();
  control::close.setTestOff();
  control::purge.setTestOff();
}
