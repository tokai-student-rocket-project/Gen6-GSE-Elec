#include "Input.hpp"
#include "Output.hpp"
#include "PowerMonitor.hpp"
#include "SemiAutoControl.hpp"
#include "SolenoidMonitor_2.0.hpp"
#include "TM1637.hpp"
#include "Thermistor.hpp"
#include "VESIM10_2.0.hpp"
#include <Arduino.h>
#include <MsgPacketizer.h>
#include <TaskManager.h>

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
  SemiAutoControl safetyArmed(PIN_PH7, true, PIN_PG3); // CTL_SAFETY, LED_SAFTEY

  Output shift(PIN_PD4);   // CTL_SHIFT
  Output fill(PIN_PD7);    // CTL_FILL
  Output dump(PIN_PD6);    // CTL_DUMP
  Output oxygen(PIN_PG0);  // CTL_O2
  Output igniter(PIN_PD5); // CTL_IGNITOR
  Output open(PIN_PG1);    // CTL_OPEN
  Output close(PIN_PC0);   // CTL_CLOSE
  Output purge(PIN_PC2);   // CTL_PURGE

  SemiAutoControl check(PIN_PC1, false, PIN_PB6); // CTL_CHECK, LED_CHECK

  Output shiftFB(PIN_PH5);   // LED_FB_SHIFT
  Output fillFB(PIN_PB0);    // LED_FB_FILL
  Output dumpFB(PIN_PB5);    // LED_FB_DUMP
  Output oxygenFB(PIN_PH0);  // LED_FB_O2
  Output igniterFB(PIN_PH4); // LED_FB_IGNITOR
  Output openFB(PIN_PH6);    // LED_FB_OPEN
  Output closeFB(PIN_PB4);   // LED_FB_CLOSE
  Output purgeFB(PIN_PH1);   // LED_FB_PURGE

  Output statusLamp(PIN_PK4); // LED_TASK

  void handleManualTask();
  void setChristmasTreeStart();
  void setChristmasTreeStop();
} // namespace control

namespace error
{
  Output statusLamp(PIN_PK6); // LED_ERROR
} // namespace error

namespace solenoid
{
  SolenoidMonitor monitor(PIN_PC5); // CS_ADC
  constexpr uint8_t FAULT_CONFIRM_COUNT = 5;

  struct Debouncer
  {
    SolenoidMonitor::Status lastRawStatus = SolenoidMonitor::Status::OFF;
    SolenoidMonitor::Status confirmedStatus = SolenoidMonitor::Status::OFF;
    uint8_t count = 0;

    SolenoidMonitor::Status update(SolenoidMonitor::Status newStatus)
    {
      if (newStatus == lastRawStatus)
      {
        count++;
      }
      else
      {
        lastRawStatus = newStatus;
        count = 1;
      }

      if (count >= FAULT_CONFIRM_COUNT)
      {
        confirmedStatus = newStatus;
      }
      return confirmedStatus;
    }

    void reset()
    {
      lastRawStatus = SolenoidMonitor::Status::OFF;
      confirmedStatus = SolenoidMonitor::Status::OFF;
      count = 0;
    }
  };

  Debouncer fillDebouncer;
  Debouncer dumpDebouncer;
  Debouncer oxygenDebouncer;
  Debouncer purgeDebouncer;

  void checkSolenoid(SolenoidMonitor::Solenoid solenoid, const char *name);
  void measureTask();
} // namespace solenoid

namespace n2o
{
  TM1637 tm1637(PIN_PK0, PIN_PK1); // 7SEG_CLK, 7SEG_DIO
  Lib_ADS1115 ads1115;
  VESIM10 vesim10(&ads1115, 0, 100.0, 10.0);

  const float CALIB_SAFE_CURRENT_MIN = 3.8;
  const float CALIB_SAFE_CURRENT_MAX = 4.2;

  float pressure_MPa = 0.0;

  void measureTask();
  void samplingTask();
} // namespace n2o

namespace umbilical
{
  Output flightMode(PIN_PL7);
  Output valveMode(PIN_PL6);
} // namespace umbilical

namespace communication
{
  enum class Packet : uint8_t
  {
    CONTROL_SYNC = 0,
    FEEDBACK_SYNC = 1,
    PRESSURE_SYNC = 2,
    COM_CHECK_L_TO_S = 3,
    COM_CHECK_S_TO_L = 4,
    SENSOR_CONFIG_SYNC = 5,
    SENSOR_DUMMY_CURRENT_SYNC = 6,
    SENSOR_CALIB_COEFF_SYNC = 7,
    SENSOR_ZERO_CALIB_REQ = 8,
    SENSOR_CURRENT_SYNC = 9,
    LIMIT_SWITCH_SYNC = 10,
    COM_CHECK_L_TO_N = 11,
    COM_CHECK_N_TO_L = 12,

    // --- Raspberry Pi 4 無線通信用パケット (3.0 新規追加) ---
    RASPI_SATELLITE_TELEMETRY = 0x33, // (51) Raspberry Pi 4 へ機体詳細テレメトリ送信
    RASPI_WIRELESS_STATUS     = 0x34, // (52) 機体無線ステータス
  };

  Output sendEnableControl(PIN_PC3); // CTL_RS485_DERE
  Output accessLamp(PIN_PC4);       // LED_RS485_ACCESS

  uint8_t syncState;
  unsigned long preReceivedTime = 0;
  const long timeout = 5000;

  void enableOutput();
  void disableOutput();

  void sendFeedbackSync();
  void sendPressureSync();
  void sendCurrentSync();
  void sendReplyToLaunch();

  void onControlSyncReceived(uint8_t state);
  void onComCheckReceived();
  void onComCheckFailed();
  void onSensorConfigReceived(float fullScale_MPa);
  void onSensorDummyCurrentReceived(float dummyCurrent_mA);
  void onSensorCalibCoeffReceived(float a, float b);
  void onSensorZeroCalibReqReceived();

  Output statusLamp(PIN_PK5); // LED_COM
} // namespace communication

namespace raspi_wireless
{
  unsigned long lastHeartbeatTime = 0;
  const unsigned long WIRELESS_TIMEOUT_MS = 3000;
  bool isWirelessConnected = false;

  void checkWirelessTask();
  void sendWirelessTelemetryTask();
  void onHeartbeatReceived();
} // namespace raspi_wireless

void setup()
{
  power::loadSwitch.on();
  power::powerLamp.on();

  // FT232RL (USB - Raspberry Pi 4 / PC との通信用シリアル)
  Serial.begin(115200);

  // LTC485 (RS485 - ランチコントローラーとの通信用シリアル1)
  Serial1.begin(115200);

  communication::preReceivedTime = millis();
  raspi_wireless::lastHeartbeatTime = millis();

  n2o::tm1637.initialize();

  Wire.begin();
  power::input.begin();
  power::bus12.begin();

  if (!n2o::ads1115.begin())
  {
    Serial.println("Failed to initialize ADS1115.");
    error::statusLamp.on();
  }
  else
  {
    n2o::vesim10.begin();
  }

  // ================= タスクの登録 (TaskManager) =================
  Tasks.add(&power::measureTask)->startFps(10);
  Tasks.add(&solenoid::measureTask)->startFps(5);
  Tasks.add(&n2o::measureTask)->startFps(5);
  Tasks.add(&n2o::samplingTask)->startFps(100);
  Tasks.add(&control::handleManualTask)->startFps(9);
  Tasks.add(&communication::onComCheckFailed)->startFps(2);

  // --- Raspberry Pi 4 無線タスクの追加 (3.0 新規) ---
  Tasks.add(&raspi_wireless::checkWirelessTask)->startFps(2);          // 2Hz 無線生存監視
  Tasks.add(&raspi_wireless::sendWirelessTelemetryTask)->startFps(10); // 10Hz 無線テレメトリ送信

  // ================= RS485 受信コールバックの登録 (Serial1) =================
  MsgPacketizer::subscribe(
      Serial1, static_cast<uint8_t>(communication::Packet::CONTROL_SYNC),
      &communication::onControlSyncReceived);
  MsgPacketizer::subscribe(
      Serial1, static_cast<uint8_t>(communication::Packet::COM_CHECK_L_TO_S),
      &communication::onComCheckReceived);
  MsgPacketizer::subscribe(
      Serial1, static_cast<uint8_t>(communication::Packet::SENSOR_CONFIG_SYNC),
      &communication::onSensorConfigReceived);
  MsgPacketizer::subscribe(
      Serial1, static_cast<uint8_t>(communication::Packet::SENSOR_DUMMY_CURRENT_SYNC),
      &communication::onSensorDummyCurrentReceived);
  MsgPacketizer::subscribe(
      Serial1, static_cast<uint8_t>(communication::Packet::SENSOR_CALIB_COEFF_SYNC),
      &communication::onSensorCalibCoeffReceived);
  MsgPacketizer::subscribe(
      Serial1, static_cast<uint8_t>(communication::Packet::SENSOR_ZERO_CALIB_REQ),
      &communication::onSensorZeroCalibReqReceived);

  // ================= Raspberry Pi 4 受信コールバックの登録 (Serial) =================
  MsgPacketizer::subscribe(
      Serial, static_cast<uint8_t>(communication::Packet::RASPI_WIRELESS_STATUS),
      &raspi_wireless::onHeartbeatReceived);

  control::setChristmasTreeStart();
  Tasks.add(&control::setChristmasTreeStop)->startOnceAfterSec(3.0);
}

void loop()
{
  MsgPacketizer::parse();
  Tasks.update();

  communication::accessLamp.update();
  control::statusLamp.update();
}

void communication::enableOutput()
{
  communication::sendEnableControl.on();
  communication::accessLamp.pulse(50);
}

void communication::disableOutput() { communication::sendEnableControl.off(); }

void power::measureTask()
{
  bool isLowVoltage = power::input.getVoltage_V() < 10.5;
  bool isOverloadedInput = power::input.getAmpere_A() > 5.0;
  bool isOverloadedBus = power::bus12.getAmpere_A() > 3.0;
  bool isOverheated = power::thermal.getTemperature_degC() > 100.0;

  power::lowVoltageLamp.set(isLowVoltage);

  if (isOverloadedInput || isOverloadedBus || isOverheated)
  {
    error::statusLamp.on();
  }
}

void solenoid::measureTask()
{
  checkSolenoid(SolenoidMonitor::Solenoid::FILL, "FILL");
  checkSolenoid(SolenoidMonitor::Solenoid::DUMP, "DUMP");
  checkSolenoid(SolenoidMonitor::Solenoid::OXYGEN, "OXYGEN");
  checkSolenoid(SolenoidMonitor::Solenoid::PURGE, "PURGE");

  SolenoidMonitor::Status rawFillStatus = monitor.getStatus(SolenoidMonitor::Solenoid::FILL);
  SolenoidMonitor::Status rawDumpStatus = monitor.getStatus(SolenoidMonitor::Solenoid::DUMP);
  SolenoidMonitor::Status rawOxygenStatus = monitor.getStatus(SolenoidMonitor::Solenoid::OXYGEN);
  SolenoidMonitor::Status rawPurgeStatus = monitor.getStatus(SolenoidMonitor::Solenoid::PURGE);

  if (!control::safetyArmed.isManualRaised())
  {
    fillDebouncer.reset();
    dumpDebouncer.reset();
    oxygenDebouncer.reset();
    purgeDebouncer.reset();

    control::fillFB.off();
    control::dumpFB.off();
    control::oxygenFB.off();
    control::purgeFB.off();
    return;
  }

  SolenoidMonitor::Status fillStatus = fillDebouncer.update(rawFillStatus);
  SolenoidMonitor::Status dumpStatus = dumpDebouncer.update(rawDumpStatus);
  SolenoidMonitor::Status oxygenStatus = oxygenDebouncer.update(rawOxygenStatus);
  SolenoidMonitor::Status purgeStatus = purgeDebouncer.update(rawPurgeStatus);

  control::openFB.set(control::open.isHigh());
  control::closeFB.set(control::close.isHigh());
  control::igniterFB.set(control::igniter.isHigh());

  switch (fillStatus)
  {
  case SolenoidMonitor::Status::OPEN_FAILURE: control::fillFB.toggle(); break;
  case SolenoidMonitor::Status::CLOSE_FAILURE: control::fillFB.off(); break;
  case SolenoidMonitor::Status::OFF: control::fillFB.off(); break;
  case SolenoidMonitor::Status::ON: control::fillFB.on(); break;
  }

  switch (dumpStatus)
  {
  case SolenoidMonitor::Status::OPEN_FAILURE: control::dumpFB.toggle(); break;
  case SolenoidMonitor::Status::CLOSE_FAILURE: control::dumpFB.off(); break;
  case SolenoidMonitor::Status::OFF: control::dumpFB.off(); break;
  case SolenoidMonitor::Status::ON: control::dumpFB.on(); break;
  }

  switch (oxygenStatus)
  {
  case SolenoidMonitor::Status::OPEN_FAILURE: control::oxygenFB.toggle(); break;
  case SolenoidMonitor::Status::CLOSE_FAILURE: control::oxygenFB.off(); break;
  case SolenoidMonitor::Status::OFF: control::oxygenFB.off(); break;
  case SolenoidMonitor::Status::ON: control::oxygenFB.on(); break;
  }

  switch (purgeStatus)
  {
  case SolenoidMonitor::Status::OPEN_FAILURE: control::purgeFB.toggle(); break;
  case SolenoidMonitor::Status::CLOSE_FAILURE: control::purgeFB.off(); break;
  case SolenoidMonitor::Status::OFF: control::purgeFB.off(); break;
  case SolenoidMonitor::Status::ON: control::purgeFB.on(); break;
  }
}

void solenoid::checkSolenoid(SolenoidMonitor::Solenoid solenoid, const char *name)
{
  uint16_t voltage = monitor.getVoltage_mV(solenoid);
  SolenoidMonitor::Status status = monitor.getStatus(solenoid);
  const char *statusStr;
  switch (status)
  {
  case SolenoidMonitor::Status::ON: statusStr = "ON"; break;
  case SolenoidMonitor::Status::OFF: statusStr = "OFF"; break;
  case SolenoidMonitor::Status::OPEN_FAILURE: statusStr = "OPEN FAILURE"; break;
  case SolenoidMonitor::Status::CLOSE_FAILURE: statusStr = "CLOSE FAILURE"; break;
  }
  Serial.print(name);
  Serial.print(": Voltage=");
  Serial.print(voltage);
  Serial.print("mV, Status=");
  Serial.println(statusStr);
}

void n2o::measureTask()
{
  float current_mA = n2o::vesim10.read();
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

void n2o::samplingTask() { n2o::vesim10.sample(); }

void communication::sendReplyToLaunch()
{
  uint8_t state =
      (control::shiftFB.isHigh() << 0) | (control::fillFB.isHigh() << 1) |
      (control::dumpFB.isHigh() << 2) | (control::oxygenFB.isHigh() << 3) |
      (control::igniterFB.isHigh() << 4) | (control::openFB.isHigh() << 5) |
      (control::closeFB.isHigh() << 6) | (control::purgeFB.isHigh() << 7);

  communication::enableOutput();
  MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::FEEDBACK_SYNC), state);
  MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::PRESSURE_SYNC), n2o::pressure_MPa);
  MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::SENSOR_CURRENT_SYNC), n2o::vesim10.getCurrent_mA());
  MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::COM_CHECK_S_TO_L));
  Serial1.flush();
  communication::disableOutput();
}

void communication::onControlSyncReceived(uint8_t state)
{
  bool isArmed = control::safetyArmed.isManualRaised();
  communication::syncState = state;
  communication::statusLamp.on();
  error::statusLamp.off();

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

  communication::sendReplyToLaunch();
}

void communication::onComCheckReceived()
{
  communication::statusLamp.on();
  error::statusLamp.off();
  communication::preReceivedTime = millis();
}

void communication::onComCheckFailed()
{
  if (millis() - communication::preReceivedTime > communication::timeout)
  {
    communication::statusLamp.off();
    error::statusLamp.on();
  }
}

void communication::onSensorConfigReceived(float fullScale_MPa)
{
  n2o::vesim10.setFullScale(fullScale_MPa);
}

void communication::onSensorDummyCurrentReceived(float dummyCurrent_mA)
{
  if (dummyCurrent_mA < 0.1)
  {
    n2o::vesim10.disableDummy();
  }
  else
  {
    n2o::vesim10.setDummyCurrent(dummyCurrent_mA);
  }
}

void communication::onSensorCalibCoeffReceived(float a, float b)
{
  n2o::vesim10.setCalibration(a, b);
}

void communication::onSensorZeroCalibReqReceived()
{
  float current_mA = n2o::vesim10.getCurrent_mA();
  if (current_mA >= n2o::CALIB_SAFE_CURRENT_MIN && current_mA <= n2o::CALIB_SAFE_CURRENT_MAX)
  {
    bool success = n2o::vesim10.calibrateZero();
    if (success)
    {
      Serial.println("[VESIM10] Zero-point calibration SUCCESSFUL.");
    }
    else
    {
      Serial.println("[VESIM10] Zero-point calibration FAILED.");
      error::statusLamp.on();
    }
  }
  else
  {
    Serial.println("[VESIM10] Zero-point calibration REJECTED: Current out of safe atmospheric range!");
    error::statusLamp.on();
  }
}

void control::handleManualTask()
{
  control::statusLamp.pulse(50);
  if (power::killButton.isHigh())
  {
    power::powerLamp.off(); delay(500); power::powerLamp.on(); delay(500);
    power::powerLamp.off(); delay(500); power::powerLamp.on(); delay(500);
    power::powerLamp.off(); delay(500); power::powerLamp.on(); delay(500);
    power::powerLamp.off();
    power::loadSwitch.off();
  }
  control::safetyArmed.setManual();
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
  control::check.setTestOn();
  control::shiftFB.setTestOn();
  control::fillFB.setTestOn();
  control::dumpFB.setTestOn();
  control::oxygenFB.setTestOn();
  control::igniterFB.setTestOn();
  control::openFB.setTestOn();
  control::closeFB.setTestOn();
  control::purgeFB.setTestOn();
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
  control::check.setTestOff();
  control::shiftFB.setTestOff();
  control::fillFB.setTestOff();
  control::dumpFB.setTestOff();
  control::oxygenFB.setTestOff();
  control::igniterFB.setTestOff();
  control::openFB.setTestOff();
  control::closeFB.setTestOff();
  control::purgeFB.setTestOff();
}

// =========================================================================
// Raspberry Pi 4 無線通信管理 (Satellite3.0 新規)
// =========================================================================

void raspi_wireless::onHeartbeatReceived()
{
  raspi_wireless::lastHeartbeatTime = millis();
  if (!raspi_wireless::isWirelessConnected)
  {
    raspi_wireless::isWirelessConnected = true;
    Serial.println("[RASPI SATELLITE WIRELESS] Link Established / Restored.");
  }
}

void raspi_wireless::checkWirelessTask()
{
  bool timeoutOccurred = (millis() - raspi_wireless::lastHeartbeatTime > raspi_wireless::WIRELESS_TIMEOUT_MS);
  if (timeoutOccurred && raspi_wireless::isWirelessConnected)
  {
    raspi_wireless::isWirelessConnected = false;
    Serial.println("[RASPI SATELLITE WIRELESS WARN] Wireless heartbeat timed out.");
  }
}

void raspi_wireless::sendWirelessTelemetryTask()
{
  uint8_t fb_state =
      (control::shiftFB.isHigh() << 0) | (control::fillFB.isHigh() << 1) |
      (control::dumpFB.isHigh() << 2) | (control::oxygenFB.isHigh() << 3) |
      (control::igniterFB.isHigh() << 4) | (control::openFB.isHigh() << 5) |
      (control::closeFB.isHigh() << 6) | (control::purgeFB.isHigh() << 7);

  // パケット送信: RASPI_SATELLITE_TELEMETRY
  MsgPacketizer::send(
      Serial,
      static_cast<uint8_t>(communication::Packet::RASPI_SATELLITE_TELEMETRY),
      fb_state, n2o::pressure_MPa, n2o::vesim10.getCurrent_mA(), power::input.getVoltage_V(), power::input.getAmpere_A());
}
