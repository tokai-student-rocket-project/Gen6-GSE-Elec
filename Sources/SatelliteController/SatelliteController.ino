#include <Arduino.h>
#include <TaskManager.h>
#include <MsgPacketizer.h>
#include "Input.hpp"
#include "Output.hpp"
#include "SemiAutoControl.hpp"
#include "AutoControl.hpp"
#include "SolenoidMonitor.hpp"


namespace power {
  Input killButton(PIN_PJ1, false);
  Output loadSwitch(PIN_PF5);
  Output lowVoltageLamp(PIN_PK7);
} // namespace power

namespace control {
  SemiAutoControl safetyArmed(PIN_PC2, true, PIN_PH7);

  AutoControl shift(PIN_PD4);
  AutoControl fill(PIN_PD5);
  AutoControl dump(PIN_PG1);
  // AutoControl oxygen(PIN_PD5);
  AutoControl igniter(PIN_PD7);
  AutoControl open(PIN_PD6);
  AutoControl close(PIN_PG0);
  SemiAutoControl purge(PIN_PC0, true, PIN_PB6);

  void handleManualTask();

  void setChristmasTreeStart();
  void setChristmasTreeStop();
} // namespace control

namespace task {
  Output accessLamp(PIN_PK4);
} // namespace task

namespace error {
  // HACK LEDだけでなく処理もする
  Output statusLamp(PIN_PK6);
} // namespace caution

namespace solenoid {
  SolenoidMonitor monitor(PIN_PC4);

} // namespace solenoid

namespace umbilical {
  Output flightMode(PIN_PH3);
  Output valveMode(PIN_PH2);
} // namespace umbilical

namespace launchController {
  enum class Packet : uint8_t {
    CONTROL_SYNC
  };

  Output sendEnableControl(PIN_PA2);
  Output accessLamp(PIN_PA4);

  void enableOutput();
  void disableOutput();

  void onControlReceived(uint8_t state);

  Output comLamp(PIN_PK5);
} // namespace launchController


void setup() {
  power::loadSwitch.on();

  // RS485の送信が終わったら割り込みを発生させる
  UCSR1B |= (1 << TXCIE0);

  // FT232RL (USB)
  Serial.begin(115200);

  // LTC485 (RS485)
  Serial1.begin(115200);

  // MCP3208 (ADC)
  SPI.begin();
  solenoid::monitor.setDividerResistance(5600, 3300);


  Tasks.add(&control::handleManualTask)->startFps(50);

  MsgPacketizer::subscribe(Serial1, static_cast<uint8_t>(launchController::Packet::CONTROL_SYNC), &launchController::onControlReceived);

  control::setChristmasTreeStart();
  Tasks.add(&control::setChristmasTreeStop)->startOnceAfterSec(3.0);
}


void loop() {
  MsgPacketizer::parse();
  Tasks.update();
}


/// @brief RS485の送信が終わったら送信を無効にするイベントハンドラ
ISR(USART1_TX_vect) {
  launchController::disableOutput();
}


/// @brief 送信を有効にする
void launchController::enableOutput() {
  launchController::sendEnableControl.on();
  launchController::accessLamp.on();
}


/// @brief 送信を無効にする
void launchController::disableOutput() {
  launchController::sendEnableControl.off();
  launchController::accessLamp.off();
}


void launchController::onControlReceived(uint8_t state) {
  control::shift.setAutomatic(state & (1 << 0) && control::safetyArmed.isManualRaised());
  control::fill.setAutomatic(state & (1 << 1) && control::safetyArmed.isManualRaised());
  control::dump.setAutomatic(state & (1 << 2) && control::safetyArmed.isManualRaised());
  // control::oxygen.setAutomatic(state & (1 << 3) && control::safetyArmed.isManualRaised());
  control::igniter.setAutomatic(state & (1 << 4) && control::safetyArmed.isManualRaised());
  control::open.setAutomatic(state & (1 << 5) && control::safetyArmed.isManualRaised());
  control::close.setAutomatic(state & (1 << 6) && control::safetyArmed.isManualRaised());
  control::purge.setAutomatic(state & (1 << 7) && control::safetyArmed.isManualRaised());
}


void control::handleManualTask() {
  task::accessLamp.blink();

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
  umbilical::flightMode.set(control::igniter.isRaised());
  umbilical::valveMode.set(control::open.isRaised() && !control::close.isRaised());
}


void control::setChristmasTreeStart() {
  error::statusLamp.setTestOn();
  power::lowVoltageLamp.setTestOn();
  task::accessLamp.setTestOn();
  launchController::accessLamp.setTestOn();
  launchController::comLamp.setTestOn();
  control::safetyArmed.setTestOn();
  control::shift.setTestOn();
  control::fill.setTestOn();
  control::dump.setTestOn();
  // control::oxygen.setTestOn();
  control::igniter.setTestOn();
  control::open.setTestOn();
  control::close.setTestOn();
  control::purge.setTestOn();
}


void control::setChristmasTreeStop() {
  error::statusLamp.setTestOff();
  power::lowVoltageLamp.setTestOff();
  task::accessLamp.setTestOff();
  launchController::accessLamp.setTestOff();
  launchController::comLamp.setTestOff();
  control::safetyArmed.setTestOff();
  control::shift.setTestOff();
  control::fill.setTestOff();
  control::dump.setTestOff();
  // control::oxygen.setTestOff();
  control::igniter.setTestOff();
  control::open.setTestOff();
  control::close.setTestOff();
  control::purge.setTestOff();
}
