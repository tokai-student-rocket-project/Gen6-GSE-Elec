#include "Input.hpp"
#include "Output.hpp"
#include "PowerMonitor.hpp"
#include "SemiAutoControl.hpp"
#include "SolenoidMonitor.hpp"
#include "TM1637.hpp"
#include "Thermistor.hpp"
#include "VESIM10.hpp"
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
} // namespace error

namespace solenoid
{
  SolenoidMonitor monitor(PIN_PC4);
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
  TM1637 tm1637(PIN_PK0, PIN_PK1);
  VESIM10 vesim10(PIN_PK2, 240.0, 10.0);

  // --- ゼロ点校正の安全設定 ---
  // 大気圧下において許容される生電流の範囲 (mA)
  // この範囲外の場合は、圧力が残っているかセンサ異常として校正をブロックします。
  const float CALIB_SAFE_CURRENT_MIN = 3.8;
  const float CALIB_SAFE_CURRENT_MAX = 4.2;
  // ---------------------------

  float pressure_MPa = 0.0;

  void measureTask();
  void samplingTask();
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
    SATELLITE_VOLTAGE_SYNC = 13,

    RASPI_SATELLITE_TELEMETRY = 0x33, // (51) Raspberry Pi 4 へ機体詳細テレメトリ送信
    RASPI_WIRELESS_STATUS = 0x34,     // (52) 機体無線ステータス
  };

  Output sendEnableControl(PIN_PA2);
  Output accessLamp(PIN_PA4); // RS485

  uint8_t syncState;
  unsigned long preReceivedTime = 0;
  const long timeout = 5000;

  void enableOutput();
  void disableOutput();

  void sendReplyToLaunch();

  void onControlSyncReceived(uint8_t state);
  void onComCheckReceived();
  void onComCheckFailed();
  void onSensorConfigReceived(float fullScale_MPa);
  void onSensorDummyCurrentReceived(float dummyCurrent_mA);
  void onSensorCalibCoeffReceived(float a, float b);
  void onSensorZeroCalibReqReceived();

  Output statusLamp(PIN_PK5); // COM
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

/// @brief 送信を有効にする
void communication::enableOutput()
{
  communication::sendEnableControl.on();
  delayMicroseconds(50);
  communication::accessLamp.pulse(50);
}

/// @brief 送信を無効にする
void communication::disableOutput()
{
  delayMicroseconds(500);
  communication::sendEnableControl.off();
}

void power::measureTask()
{
  bool isLowVoltage =
      power::input.getVoltage_V() < 10.5; // 電磁弁の許容電流に設定
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
  case SolenoidMonitor::Status::OPEN_FAILURE:
    control::fillFB.toggle();
    break;
  case SolenoidMonitor::Status::CLOSE_FAILURE:
    control::fillFB.off();
    break;
  case SolenoidMonitor::Status::OFF:
    control::fillFB.off();
    break;
  case SolenoidMonitor::Status::ON:
    control::fillFB.on();
    break;
  }

  switch (dumpStatus)
  {
  case SolenoidMonitor::Status::OPEN_FAILURE:
    control::dumpFB.toggle();
    break;
  case SolenoidMonitor::Status::CLOSE_FAILURE:
    control::dumpFB.off();
    break;
  case SolenoidMonitor::Status::OFF:
    control::dumpFB.off();
    break;
  case SolenoidMonitor::Status::ON:
    control::dumpFB.on();
    break;
  }

  switch (oxygenStatus)
  {
  case SolenoidMonitor::Status::OPEN_FAILURE:
    control::oxygenFB.toggle();
    break;
  case SolenoidMonitor::Status::CLOSE_FAILURE:
    control::oxygenFB.off();
    break;
  case SolenoidMonitor::Status::OFF:
    control::oxygenFB.off();
    break;
  case SolenoidMonitor::Status::ON:
    control::oxygenFB.on();
    break;
  }

  switch (purgeStatus)
  {
  case SolenoidMonitor::Status::OPEN_FAILURE:
    control::purgeFB.toggle();
    break;
  case SolenoidMonitor::Status::CLOSE_FAILURE:
    control::purgeFB.off();
    break;
  case SolenoidMonitor::Status::OFF:
    control::purgeFB.off();
    break;
  case SolenoidMonitor::Status::ON:
    control::purgeFB.on();
    break;
  }
}

void solenoid::checkSolenoid(SolenoidMonitor::Solenoid solenoid,
                             const char *name)
{
  // 電圧値の取得
  uint16_t voltage = monitor.getVoltage_mV(solenoid);

  // 状態の取得
  SolenoidMonitor::Status status = monitor.getStatus(solenoid);

  // 状態の文字列化
  const char *statusStr;
  switch (status)
  {
  case SolenoidMonitor::Status::ON:
    statusStr = "ON";
    break;
  case SolenoidMonitor::Status::OFF:
    statusStr = "OFF";
    break;
  case SolenoidMonitor::Status::OPEN_FAILURE:
    statusStr = "OPEN FAILURE";
    break;
  case SolenoidMonitor::Status::CLOSE_FAILURE:
    statusStr = "CLOSE FAILURE";
    break;
  }
  // 結果の出力
  // Serial.print(name);
  // Serial.print(": Voltage=");
  // Serial.print(voltage);
  // Serial.print("mV, Status=");
  // Serial.println(statusStr);
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
  float satVolts = power::input.getVoltage_V();
  float pressure = n2o::pressure_MPa;
  float current = n2o::vesim10.getCurrent_mA();

  communication::enableOutput();
  Serial.println("[SATELLITE] TX Reply -> Launch");
  MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::FEEDBACK_SYNC), state);
  MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::SATELLITE_VOLTAGE_SYNC), satVolts);
  MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::PRESSURE_SYNC), pressure);
  MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::SENSOR_CURRENT_SYNC), current);
  MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::COM_CHECK_S_TO_L));
  Serial1.flush();
  communication::disableOutput();
}

void communication::onControlSyncReceived(uint8_t state)
{
  bool isArmed = control::safetyArmed.isManualRaised();
  Serial.print("[SATELLITE] RX SUCCESS! CONTROL_SYNC state = 0x");
  Serial.println(state, HEX);

  communication::syncState = state;
  communication::preReceivedTime = millis(); // ★Heartbeat: 受信時刻を更新
  communication::statusLamp.on();            // ★受信成功でCOMランプ点灯
  error::statusLamp.off();                   // ★受信成功でERRランプ消灯
  communication::accessLamp.pulse(50);       // ★RS485アクセスLEDを点滅

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

  communication::sendReplyToLaunch();
}

void communication::onComCheckReceived()
{
  communication::statusLamp.on();
  error::statusLamp.off();
  communication::preReceivedTime = millis();
  // Serial.print("preComReceivedTime: ");
  // Serial.println(communication::preReceivedTime);
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

/**
 * @brief センサ校正係数の受信ハンドラ
 * 実測に基づく傾き(a)と切片(b)を直接設定します。
 */
void communication::onSensorCalibCoeffReceived(float a, float b)
{
  n2o::vesim10.setCalibration(a, b);
}

/**
 * @brief ゼロ点校正実行リクエストの受信ハンドラ
 * 現在の圧力（大気圧下を想定）を 0MPa としてオフセットを再計算します。
 */
void communication::onSensorZeroCalibReqReceived()
{
  // 安全チェック: 生の電流値が大気圧相当（約4mA）であることを確認
  float current = n2o::vesim10.getCurrent_mA();

  if (current >= n2o::CALIB_SAFE_CURRENT_MIN &&
      current <= n2o::CALIB_SAFE_CURRENT_MAX)
  {
    Serial.println(">>> Zero-Point Calibration: Started.");
    n2o::vesim10.calibrateBlocking(10);
    Serial.println(">>> Zero-Point Calibration: Successfully completed.");
  }
  else
  {
    Serial.print(">>> CALIBRATION BLOCKED: Sensor current out of safe range (");
    Serial.print(current);
    Serial.println(" mA). Pressure may be present.");
  }
}

void communication::onComCheckFailed()
{
  if (millis() - communication::preReceivedTime > communication::timeout)
  {
    communication::statusLamp.off();
    error::statusLamp.on();
    Serial.println("[SATELLITE TIMEOUT] No RS485 packets received from Launch (>5s)!");

    // 通信失敗時は安全のため全出力を強制OFF
    control::dump.set(communication::syncState & 0);
    control::fill.set(communication::syncState & 0);
    control::oxygen.set(communication::syncState & 0);
    control::igniter.set(communication::syncState & 0);
    control::open.set(communication::syncState & 0);
    control::close.set(communication::syncState & 0);
    control::purge.set(communication::syncState & 0);
  }
}

void control::handleManualTask()
{
  control::statusLamp.pulse(50);

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
  communication::disableOutput();
  communication::preReceivedTime = millis();
  raspi_wireless::lastHeartbeatTime = millis();

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
  // n2o::vesim10.calibrateBlocking(10);

  Tasks.add(&power::measureTask)->startFps(10);
  Tasks.add(&solenoid::measureTask)->startFps(10);
  Tasks.add(&n2o::samplingTask)->startFps(100); // 100Hzでサンプリング
  Tasks.add(&n2o::measureTask)->startFps(2);    // 亜酸化窒素の圧力を計測
  Tasks.add(&control::handleManualTask)->startFps(10);
  Tasks.add(&communication::onComCheckFailed)->startFps(2);
  Tasks.add(&raspi_wireless::checkWirelessTask)->startFps(2);
  // Tasks.add(&raspi_wireless::sendWirelessTelemetryTask)->startFps(10); // ★デバッグ中テキスト表示のため一時無効化

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
      Serial1,
      static_cast<uint8_t>(communication::Packet::SENSOR_DUMMY_CURRENT_SYNC),
      &communication::onSensorDummyCurrentReceived);
  MsgPacketizer::subscribe(
      Serial1,
      static_cast<uint8_t>(communication::Packet::SENSOR_CALIB_COEFF_SYNC),
      &communication::onSensorCalibCoeffReceived);
  MsgPacketizer::subscribe(
      Serial1,
      static_cast<uint8_t>(communication::Packet::SENSOR_ZERO_CALIB_REQ),
      &communication::onSensorZeroCalibReqReceived);

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

  communication::accessLamp.update(); // RS485
  control::statusLamp.update();       // Task
}

// =========================================================================
// Raspberry Pi 4 無線通信管理
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

  // パケット送信: RASPI_SATELLITE_TELEMETRY (send_arr を使って unpack しやすくする)
  MsgPacketizer::send_arr(
      Serial,
      static_cast<uint8_t>(communication::Packet::RASPI_SATELLITE_TELEMETRY),
      static_cast<uint8_t>(communication::Packet::RASPI_SATELLITE_TELEMETRY),
      fb_state, n2o::pressure_MPa, n2o::vesim10.getCurrent_mA(), power::input.getVoltage_V(), power::input.getAmpere_A());
}
