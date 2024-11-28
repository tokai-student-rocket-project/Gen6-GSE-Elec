#include <Arduino.h>
#include <TaskManager.h>
#include <MsgPacketizer.h>
#include <TM1637.hpp>
#include <VESIM10.hpp>
#include <Input.hpp>
#include <Output.hpp>
#include <SemiAutoControl.hpp>
#include <SolenoidMonitor.hpp>
#include <PowerMonitor.hpp>
#include <Thermistor.hpp>

namespace power
{
  Input killButton(PIN_PJ1, false);
  Output loadSwitch(PIN_PF5);
  Output powerLamp(PIN_PG5);
  Output lowVoltageLamp(PIN_PK7);

  PowerMonitor input(0x40);
  PowerMonitor bus12(0x41);
  Thermistor thermal(PIN_PF4, 10000.0);

  void measureTask();
} // namespace power

namespace control
{
  SemiAutoControl safetyArmed(PIN_PC2, true, PIN_PH7);

  Output shift(PIN_PD4);
  Output fill(PIN_PD5);
  Output dump(PIN_PG1);
  Output oxygen(PIN_PC3);
  Output igniter(PIN_PD7);
  Output open(PIN_PD6);
  Output close(PIN_PG0);
  // SemiAutoControl purge(PIN_PC0, false, PIN_PG3);
  SemiAutoControl purgeSwitch(PIN_PG4, true, PIN_PG3);
  Output purge(PIN_PC0);

  // Output purge(PIN_PB6);
  // SemiAutoControl check(PIN_PC0, true, PIN_PB6);

  Output shiftFB(PIN_PH5);
  Output fillFB(PIN_PB0);
  Output dumpFB(PIN_PB5);
  Output oxygenFB(PIN_PL4);
  Output igniterFB(PIN_PH4);
  Output openFB(PIN_PH6);
  Output closeFB(PIN_PB4);
  Output purgeFB(PIN_PH1);
  Output statusLamp(PIN_PK4);

  void handleManualTask();
  void setChristmasTreeStart();
  void setChristmasTreeStop();
} // namespace control

namespace error
{
  // HACK LEDだけでなく処理もする
  Output statusLamp(PIN_PK6);
} // namespace caution

namespace solenoid
{
  SolenoidMonitor monitor(PIN_PC4);
  void measureTask();
} // namespace solenoid

namespace n2o
{
  TM1637 tm1637(PIN_PK0, PIN_PK1);
  VESIM10 vesim10(PIN_PK2, 240.0, 10.0);

  float pressure_MPa = 0.0;

  void measureTask();
} // namespace n2o

namespace umbilical
{
  Output flightMode(PIN_PH3);
  Output valveMode(PIN_PH2);
} // namespace umbilical

namespace communication
{
  enum class Packet : uint8_t
  {
    CONTROL_SYNC,
    FEEDBACK_SYNC,
    PRESSURE_SYNC,
    COM_CHECK_L_TO_S,
    COM_CHECK_S_TO_L,
  };

  Output sendEnableControl(PIN_PA2);
  Output accessLamp(PIN_PA4);

  void enableOutput();
  void disableOutput();

  void sendFeedbackSync();
  void sendPressureSync();
  void sendComCheck();
  void onControlSyncReceived(uint8_t state);
  void onComCheckReceived();

  Output statusLamp(PIN_PK5);
} // namespace communication

/// @brief 送信を有効にする
void communication::enableOutput()
{
  communication::sendEnableControl.on();
  communication::accessLamp.on();
}

/// @brief 送信を無効にする
void communication::disableOutput()
{
  communication::sendEnableControl.off();
  communication::accessLamp.off();
}

void power::measureTask()
{
  bool isLowVoltage = power::input.getVoltage_V() < 12.0;
  bool isOverloadedInput = power::input.getAmpere_A() > 3.0;
  bool isOverloadedBus = power::bus12.getAmpere_A() > 3.0;
  bool isOverheated = power::thermal.getTemperature_degC() > 100.0;

  power::lowVoltageLamp.set(isLowVoltage);

  if (isOverloadedInput || isOverloadedBus || isOverheated)
  {
    // HACK エラー
    error::statusLamp.on();
  }
}

void solenoid::measureTask()
{
  // 仮の振る舞い
  bool isArmed = control::safetyArmed.isManualRaised();

  // 正常
  control::shiftFB.set(control::shift.isHigh() && isArmed);
  control::fillFB.set(control::fill.isHigh() && isArmed);
  control::dumpFB.set(control::dump.isHigh() && isArmed);
  control::oxygenFB.set(control::oxygen.isHigh() && isArmed);
  control::igniterFB.set(control::igniter.isHigh() && isArmed);
  control::openFB.set(control::open.isHigh() && isArmed);
  control::closeFB.set(control::close.isHigh() && isArmed);
  control::purgeFB.set(control::purge.isHigh() && isArmed);

  // 故障
  // control::shiftFB.set(control::shift.isHigh() && isArmed ? !control::shiftFB.isHigh() : LOW);
  // control::fillFB.set(control::fill.isHigh() && isArmed ? !control::fillFB.isHigh() : LOW);
  // control::dumpFB.set(control::dump.isHigh() && isArmed ? !control::dumpFB.isHigh() : LOW);
  // control::oxygenFB.set(control::oxygen.isHigh() && isArmed ? !control::oxygenFB.isHigh() : LOW);
  // control::igniterFB.set(control::igniter.isHigh() && isArmed ? !control::igniterFB.isHigh() : LOW);
  // control::openFB.set(control::open.isHigh() && isArmed ? !control::openFB.isHigh() : LOW);
  // control::closeFB.set(control::close.isHigh() && isArmed ? !control::closeFB.isHigh() : LOW);
  // control::purgeFB.set(control::purge.isHigh() && isArmed ? !control::purgeFB.isHigh() : LOW);
}

void n2o::measureTask()
{
  float current_mA = n2o::vesim10.getCurrent_mA();

  if (current_mA < 0.1)
  {
    n2o::tm1637.clearDisplay();
  }
  else
  {
    n2o::pressure_MPa = n2o::vesim10.getPressure_MPa();
    n2o::tm1637.displayNumber(abs(n2o::pressure_MPa));
  }
}

void communication::sendFeedbackSync()
{
  uint8_t state = (control::shiftFB.isHigh() << 0) | (control::fillFB.isHigh() << 1) | (control::dumpFB.isHigh() << 2) | (control::oxygenFB.isHigh() << 3) | (control::igniterFB.isHigh() << 4) | (control::openFB.isHigh() << 5) | (control::closeFB.isHigh() << 6) | (control::purgeFB.isHigh() << 7);

  communication::enableOutput();
  MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::FEEDBACK_SYNC), state);
  Serial1.flush();
  communication::disableOutput();
}

void communication::sendPressureSync()
{
  communication::enableOutput();
  MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::PRESSURE_SYNC), n2o::pressure_MPa);
  Serial1.flush();
  communication::disableOutput();
}

void communication::sendComCheck()
{
  communication::enableOutput();
  MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::COM_CHECK_S_TO_L));
  Serial1.flush();
  communication::disableOutput();
}

void communication::onControlSyncReceived(uint8_t state)
{
  bool isArmed = control::safetyArmed.isManualRaised();

  // control::shift.set(state & (1 << 0) && isArmed);
  control::fill.set(state & (1 << 1) && isArmed);
  control::dump.set(state & (1 << 2) && isArmed);
  control::oxygen.set(state & (1 << 3) && isArmed);
  control::igniter.set(state & (1 << 4) && isArmed);
  control::open.set(state & (1 << 5) && isArmed);
  control::close.set(state & (1 << 6) && isArmed);
  control::purge.set(state & (1 << 7) && isArmed);

  if ((control::dump.isHigh()) && (control::close.isHigh()))
  {
    control::dump.off();
  }

  communication::statusLamp.blink();
}

void communication::onComCheckReceived()
{
  communication::statusLamp.blink();
}

void control::handleManualTask()
{
  control::statusLamp.blink();

  if (power::killButton.isHigh())
  {
    power::powerLamp.off();
    delay(500);
    power::powerLamp.on();
    delay(500);
    power::powerLamp.off();
    delay(500);
    power::powerLamp.on();
    delay(500);
    power::powerLamp.off();
    delay(500);
    power::loadSwitch.off();
  }

  // セーフティー
  // Armedでなければこの時点で終わり
  control::safetyArmed.setManual();
  if (!control::safetyArmed.isManualRaised())
  {
    return;
  }

  // 手動制御
  control::purgeSwitch.setManual();
  if (control::purgeSwitch.isManualRaised())
  {
    control::purge.on();
  }
  // Serial.println(control::purgeSwitch.isRaised());
  // control::purgeSwitch.setAutomatic(control::purge.isHigh());

  // アンビリカル
  umbilical::flightMode.set(control::igniter.isHigh());
  umbilical::valveMode.set(control::open.isHigh() && !control::close.isHigh());
}

void control::setChristmasTreeStart()
{
  n2o::tm1637.displayNumber(8.8);
  error::statusLamp.setTestOn();
  power::lowVoltageLamp.setTestOn();
  control::statusLamp.setTestOn();
  communication::accessLamp.setTestOn();
  communication::statusLamp.setTestOn();
  control::safetyArmed.setTestOn();
  control::shiftFB.setTestOn();
  control::fillFB.setTestOn();
  control::dumpFB.setTestOn();
  control::oxygenFB.setTestOn();
  control::igniterFB.setTestOn();
  control::openFB.setTestOn();
  control::closeFB.setTestOn();
  control::purgeFB.setTestOn();
  control::purgeSwitch.setTestOn();
}

void control::setChristmasTreeStop()
{
  n2o::tm1637.clearDisplay();
  error::statusLamp.setTestOff();
  power::lowVoltageLamp.setTestOff();
  control::statusLamp.setTestOff();
  communication::accessLamp.setTestOff();
  communication::statusLamp.setTestOff();
  control::safetyArmed.setTestOff();
  control::shiftFB.setTestOff();
  control::fillFB.setTestOff();
  control::dumpFB.setTestOff();
  control::oxygenFB.setTestOff();
  control::igniterFB.setTestOff();
  control::openFB.setTestOff();
  control::closeFB.setTestOff();
  control::purgeFB.setTestOff();
  control::purgeSwitch.setTestOff();
}

void setup()
{
  power::loadSwitch.on();
  power::powerLamp.on();

  // FT232RL (USB)
  Serial.begin(115200);

  // LTC485 (RS485)
  Serial1.begin(115200);

  // MCP3208 (ADC)
  SPI.begin();
  solenoid::monitor.setDividerResistance(5600, 3300);

  // TM1637 (7SEG)
  n2o::tm1637.initialize();

  // INA219 (Power)
  Wire.begin();
  power::input.begin();
  power::bus12.begin();

  // VESIM10 (N2O Pressure Sensor)
  n2o::vesim10.calibrateBlocking(10);

  Tasks.add(&power::measureTask)->startFps(10);
  Tasks.add(&solenoid::measureTask)->startFps(10);
  Tasks.add(&n2o::measureTask)->startFps(2);
  Tasks.add(&control::handleManualTask)->startFps(50);

  Tasks.add(&communication::sendFeedbackSync)->startFps(10);
  Tasks.add(&communication::sendPressureSync)->startFps(2);
  Tasks.add(&communication::sendComCheck)->startFps(2);
  MsgPacketizer::subscribe(Serial1, static_cast<uint8_t>(communication::Packet::CONTROL_SYNC), &communication::onControlSyncReceived);
  MsgPacketizer::subscribe(Serial1, static_cast<uint8_t>(communication::Packet::COM_CHECK_L_TO_S), &communication::onComCheckReceived);

  control::setChristmasTreeStart();
  Tasks.add(&control::setChristmasTreeStop)->startOnceAfterSec(3.0);
}

void loop()
{
  MsgPacketizer::parse();
  Tasks.update();
}
