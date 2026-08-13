#include "Input.hpp"
#include "Lib_24LC256.hpp"
#include "Output.hpp"
#include "PowerMonitor.hpp"
#include "SemiAutoControl.hpp"
#include "TM1637.hpp"
#include "Thermistor.hpp"
#include <Arduino.h>
#include <DFPlayer_Mini_Mp3.h>
#include <MsgPacketizer.h>
#include <TaskManager.h>

namespace power
{
  // キルスイッチ入力 (CTL_KILL): 電源を落とすためのボタン
  Input killButton(PIN_PJ1, false);
  // 電源供給用ロードスイッチ (CTL_POWER): 各部への電源供給を制御
  Output loadSwitch(PIN_PF5);
  // 電源ランプ (LED_POWER): システム電源がONであることを示す
  Output powerLamp(PIN_PG5);
  // 低電圧警告ランプ (LED_LOW_VOLTAGE): バッテリー電圧が低下した際に点灯
  Output lowVoltageLamp(PIN_PK7);

  // 入力系統の電力・電流モニタ (I2Cアドレス: 0x40)
  PowerMonitor input(0x40);
  // 12Vバス系統の電力・電流モニタ (I2Cアドレス: 0x41)
  PowerMonitor bus12(0x41);
  // 温度監視用サーミスタ (10kΩ基準): 内部レギュレーターなどの発熱を監視
  Thermistor thermal(PIN_PF4, 10000.0); // VREF_THERMISTOR

  // 電源や温度の測定値を定期的にチェックするタスク
  void measureTask();
} // namespace power

namespace control
{
  // --- 操作卓（LaunchController3.0）の物理スイッチとLED設定 ---

  // 安全装置（セーフティ）スイッチと連動LED (ONにしないと他の操作ができない)
  SemiAutoControl safetyArmed(PIN_PC2, false, PIN_PH7); // CTL_SAFETY, LED_SAFTEY
  // シーケンス開始スイッチと連動LED (自動充填・点火シーケンスのトリガー)
  SemiAutoControl sequenceStart(PIN_PC3, false, PIN_PG3); // CTL_SEQUENCE_START, LED_SEQUENCE
  // エマージェンシーストップ（緊急停止）スイッチと連動LED
  SemiAutoControl emergencyStop(PIN_PC4, true, PIN_PG4); // CTL_EMERGENCY_STOP, LED_EMERGENCY

  // 点火シーケンスに進むための3人同時押し用確認スイッチ
  Input confirm1(PIN_PC7, false); // CTL_CONFIRM_1
  Input confirm2(PIN_PC6, false); // CTL_CONFIRM_2
  Input confirm3(PIN_PC5, false); // CTL_CONFIRM_3

  // 各種電磁弁などの手動操作スイッチと連動LED
  SemiAutoControl shift(PIN_PD4, false, PIN_PH5);   // CTL_SHIFT, LED_SHIFT
  SemiAutoControl fill(PIN_PD5, false, PIN_PB0);    // CTL_FILL, LED_FILL
  SemiAutoControl dump(PIN_PG1, false, PIN_PB5);    // CTL_DUMP ,LED_DUMP
  SemiAutoControl oxygen(PIN_PC1, false, PIN_PL4);  // CTL_O2, LED_O2
  SemiAutoControl igniter(PIN_PD7, false, PIN_PH4); // CTL_IGNITOR, LED_IGNITOR
  SemiAutoControl open(PIN_PD6, false, PIN_PH6);    // CTL_OPEN, LED_OPEN
  SemiAutoControl close(PIN_PG0, false, PIN_PB4);   // CTL_CLOSE, LED_CLOSE
  SemiAutoControl purge(PIN_PC0, false, PIN_PB6);   // CTL_PURGE, LED_PURGE

  // サテライト(機体)側から返ってきた実際の電磁弁の状態を表示するフィードバック用LED
  Output shiftFB(PIN_PE3);   // LED_FB_SHIFT
  Output fillFB(PIN_PE5);    // LED_FB_FILL
  Output dumpFB(PIN_PE7);    // LED_FB_DUMP
  Output oxygenFB(PIN_PH3);  // LED_FB_O2
  Output igniterFB(PIN_PE2); // LED_FB_IGNITOR
  Output openFB(PIN_PE4);    // LED_FB_OPEN
  Output closeFB(PIN_PE6);   // LED_FB_CLOSE
  Output purgeFB(PIN_PH2);   // LED_FB_PURGE

  // 制御タスクが正常に動作しているかを示すランプ
  Output statusLamp(PIN_PK4); // LED_TASK

  // 手動スイッチ入力に対する処理を行うタスク
  void handleManualTask();

  // TaskManagerで使用するタスク名（文字列定数）
  const String FILL_START = "fill-start";
  const String FILL_STOP = "fill-stop";
  const String OXYGEN_START = "oxygen-start";
  const String OXYGEN_STOP = "oxygen-stop";
  const String IGNITER_START = "igniter-start";
  const String IGNITER_STOP = "igniter-stop";
  const String OPEN_START = "open-start";
  const String PLAY_MUSIC = "play-music";
  const String PURGE_START = "purge-start";
  const String PURGE_STOP = "purge-stop";

  // 各種制御状態をセットする関数群
  void setChristmasTreeStart(); // 起動時のLED全点灯テスト開始
  void setChristmasTreeStop();  // LED全点灯テスト終了
  void setEmergencyStop();      // エマスト時の電磁弁状態セット
  void setPeacefulStop();       // 通常停止（シーケンス中断）時の電磁弁状態セット
  void setFillStart();
  void setFillStop();
  void setOxygenStart();
  void setOxygenStop();
  void setIgniterStart();
  void setIgniterStop();
  void setOpenStart();
  void setPurgeStart();
  void setPurgeStop();
} // namespace control

namespace sequence
{
  // 各種シーケンスを実行する関数
  void emergencyStop(); // 緊急停止シーケンス
  void peacefulStop();  // 通常停止（シーケンス中断）
  void fill();          // 充填シーケンス
  void ignition();      // 点火シーケンス

  // シーケンス開始スイッチが押された回数をカウント
  uint32_t sequenceStartRiseCount = 0;

  // 現在どのシーケンスが実行中かを示すフラグ
  bool emergencyStopSequenceIsActive = false; // エマスト中
  bool fillSequenceIsActive = false;          // 充填シーケンス中
  bool ignitionSequenceIsActive = false;      // 点火シーケンス中
  bool canConfirm = false;                    // 点火の最終確認（3人同時押し）が可能かどうか
} // namespace sequence

namespace n2o
{
  TM1637 tm1637(PIN_PK0, PIN_PK1); // 7SEG_CLK, 7SEG_DIO

  // --- キャリブレーション設定 ---
  const float SENSOR_FS_MPa = 10.0f; // センサーの定格圧力
  const float CALIB_SLOPE_A = 0.5777; // 傾き
  const float CALIB_INTERCEPT_B = -CALIB_SLOPE_A * 4.0f; // 切片
} // namespace n2o

namespace error
{
  // エラー状態を示すランプ
  Output statusLamp(PIN_PK6); // ERR
} // namespace error

namespace solenoid
{
  constexpr uint8_t FAULT_CONFIRM_COUNT = 5;

  uint8_t fillFaultCount   = 0;
  uint8_t dumpFaultCount   = 0;
  uint8_t oxygenFaultCount = 0;
  uint8_t purgeFaultCount  = 0;
  uint8_t openFaultCount   = 0;
  uint8_t closeFaultCount  = 0;
} // namespace solenoid

namespace communication
{
  // 通信パケットの種類を定義 (3.0 で Raspberry Pi 4 用のパケットを追加)
  enum class Packet : uint8_t
  {
    CONTROL_SYNC = 0,              // 操作卓(Launch)からの制御コマンド同期（電磁弁開閉など）
    FEEDBACK_SYNC = 1,             // サテライトコントローラーからのフィードバック（電磁弁の実際の状態など）
    PRESSURE_SYNC = 2,             // 算出された圧力値(MPa)の同期
    COM_CHECK_L_TO_S = 3,          // 操作卓から機体への生存確認（ハートビート）
    COM_CHECK_S_TO_L = 4,          // 機体から操作卓への生存確認（ハートビート）
    SENSOR_CONFIG_SYNC = 5,        // センサの基本設定（フルスケールなど）の機体への同期
    SENSOR_DUMMY_CURRENT_SYNC = 6, // シミュレーション用のダミー電流値の同期
    SENSOR_CALIB_COEFF_SYNC = 7,   // 校正係数(a, b)の同期用
    SENSOR_ZERO_CALIB_REQ = 8,     // 機体に対するゼロ点校正実行の要求
    SENSOR_CURRENT_SYNC = 9,       // 機体から送信される生の電流値(mA)の同期
    LIMIT_SWITCH_SYNC = 10,        // SatelliteNode のリミットスイッチ状態同期
    COM_CHECK_L_TO_N = 11,         // ランチ → ノード 生存確認
    COM_CHECK_N_TO_L = 12,         // ノード → ランチ 生存確認
    COM_CHECK_L_TO_RN = 13,        // 親機 ➔ ロケットノード 生存確認
    COM_CHECK_RN_TO_L = 14,        // ロケットノード ➔ 親機 生存確認
    ROCKET_NODE_STATE_SYNC = 15,   // 親機 ➔ 状態同期

    // --- Raspberry Pi 4 無線通信用パケット (3.0 新規追加) ---
    RASPI_COMMAND = 0x20,            // (32) Raspberry Pi 4 からの遠隔制御コマンド
    RASPI_HEARTBEAT_L_TO_R = 0x21,   // (33) Launch3.0 → Raspberry Pi 4 生存確認
    RASPI_HEARTBEAT_R_TO_L = 0x22,   // (34) Raspberry Pi 4 → Launch3.0 生存確認
    RASPI_TELEMETRY = 0x23,          // (35) テレメトリ一括送信
    RASPI_WIRELESS_STATUS = 0x24,    // (36) 無線リンク状態報告
  };

  // RS485の送信許可ピン (HIGHで送信有効)
  Output sendEnableControl(PIN_PA2); // CTL_RS485_DERE
  // 通信アクセスランプ
  Output accessLamp(PIN_PA4); // LED_RS485_ACCESS

  // 機体側から最後に通信を受信した時刻
  unsigned long preReceivedTime = 0;
  unsigned long preReceivedTime_Node = 0;
  unsigned long preReceivedTime_RocketNode = 0;
  bool isRocketNodeDisconnected = false;
  const long timeout = 5000;

  // リミットスイッチ状態
  uint8_t limitSwitchState = 0;

  inline bool isCh0Pressed() { return (limitSwitchState >> 0) & 0x01; }
  inline bool isCh1Pressed() { return (limitSwitchState >> 1) & 0x01; }
  inline bool isCh2Pressed() { return (limitSwitchState >> 2) & 0x01; }
  inline bool isCh3Pressed() { return (limitSwitchState >> 3) & 0x01; }
  inline bool isCh4Pressed() { return (limitSwitchState >> 4) & 0x01; }
  inline bool isCh5Pressed() { return (limitSwitchState >> 5) & 0x01; }

  constexpr uint8_t SAFETY_GATE_MASK = (1 << 5); // ch5のみを監視
  inline bool isSafetyGateMet() { return (limitSwitchState & SAFETY_GATE_MASK) == SAFETY_GATE_MASK; }

  // RS485の送信を有効/無効化する関数
  void enableOutput();
  void disableOutput();

  // パケット送信関数
  void pollingTask();
  void sendSensorConfigSync();
  void sendSensorDummyCurrent(float current_mA);
  void sendSensorCalibCoeff(float a, float b);
  void sendSensorZeroCalibReq();
  void sendTelemetryForPython();

  // パケット受信時のコールバック関数
  void onFeedbackSyncReceived(uint8_t state);
  void onPressureSyncReceived(float pressure);
  void onCurrentSyncReceived(float current_mA);
  void onComCheckReceived();
  void onComCheckNodeReceived();
  void onComCheckRocketNodeReceived();
  void onComCheckFailed();
  void onLimitSwitchSyncReceived(uint8_t state);

  // 通信状態が正常であることを示すランプ
  Output statusLamp(PIN_PK5); // LED_COM
} // namespace communication

namespace raspi_wireless
{
  // --- Raspberry Pi 4 無線通信管理パラメータ ---
  unsigned long lastHeartbeatTime = 0;
  const unsigned long WIRELESS_TIMEOUT_MS = 3000; // 3.0秒タイムアウト
  bool isWirelessConnected = false;
  bool remoteArmingState = false;
  float latestPressure_MPa = 0.0f;

  // コマンド種別定義
  enum class RemoteCmd : uint8_t
  {
    EMERGENCY_STOP = 1,
    PEACEFUL_STOP  = 2,
    FILL_START     = 3,
    IGNITION_START = 4,
    ARM_SAFETY     = 5,
    VALVE_CONTROL  = 6,
    ZERO_CALIB     = 7,
  };

  // 無線生存監視タスク
  void checkWirelessTask();
  // RasPi 4 へテレメトリを送信するタスク
  void sendWirelessTelemetryTask();
  // コールバック関数
  void onRaspiHeartbeatReceived();
  void onRaspiCommandReceived(uint8_t cmdType, uint8_t param);
} // namespace raspi_wireless

namespace simulation
{
  void updateTask() { communication::sendSensorDummyCurrent(0.0f); }
} // namespace simulation

namespace storage
{
  Lib_24LC256 eeprom(0x50);
  void readWriteTest();
} // namespace storage

void setup()
{
  power::loadSwitch.on();
  power::powerLamp.on();

  // FT232RL (USB - Raspberry Pi 4 / PC との通信用シリアル)
  Serial.begin(115200);

  // LTC485 (RS485 - サテライトコントローラーとの通信用シリアル1)
  Serial1.begin(115200);

  // DFPlayer Mini (音声再生用シリアル2)
  Serial2.begin(9600);
  mp3_set_serial(Serial2);
  mp3_stop();
  mp3_set_volume(30);

  communication::preReceivedTime = millis();
  communication::preReceivedTime_Node = millis();
  communication::preReceivedTime_RocketNode = millis();
  raspi_wireless::lastHeartbeatTime = millis();

  // TM1637 & Wire
  n2o::tm1637.initialize();
  Wire.begin();
  power::input.begin();
  power::bus12.begin();

  // ================= タスクの登録 (TaskManager) =================
  Tasks.add(&power::measureTask)->startFps(10);
  Tasks.add(&control::handleManualTask)->startFps(9);
  Tasks.add(&communication::pollingTask)->startFps(5);
  Tasks.add(&communication::onComCheckFailed)->startFps(2);
  // (Python GUI 用シリアル送信機能通信 Tasks.add(&communication::sendTelemetryForPython) は無効化)
  Tasks.add(&simulation::updateTask)->startFps(2);

  // --- Raspberry Pi 4 無線タスクの追加 (3.0 新規: Serial 使用) ---
  Tasks.add(&raspi_wireless::checkWirelessTask)->startFps(2);          // 2Hz で無線タイムアウト監視
  Tasks.add(&raspi_wireless::sendWirelessTelemetryTask)->startFps(10); // 10Hz で無線テレメトリ送信

  communication::sendSensorConfigSync();
  communication::sendSensorCalibCoeff(n2o::CALIB_SLOPE_A, n2o::CALIB_INTERCEPT_B);

  // ================= RS485 受信コールバックの登録 (Serial1) =================
  MsgPacketizer::subscribe(
      Serial1, static_cast<uint8_t>(communication::Packet::FEEDBACK_SYNC),
      &communication::onFeedbackSyncReceived);
  MsgPacketizer::subscribe(
      Serial1, static_cast<uint8_t>(communication::Packet::PRESSURE_SYNC),
      &communication::onPressureSyncReceived);
  MsgPacketizer::subscribe(
      Serial1, static_cast<uint8_t>(communication::Packet::SENSOR_CURRENT_SYNC),
      &communication::onCurrentSyncReceived);
  MsgPacketizer::subscribe(
      Serial1, static_cast<uint8_t>(communication::Packet::COM_CHECK_S_TO_L),
      &communication::onComCheckReceived);
  MsgPacketizer::subscribe(
      Serial1, static_cast<uint8_t>(communication::Packet::COM_CHECK_N_TO_L),
      &communication::onComCheckNodeReceived);
  MsgPacketizer::subscribe(
      Serial1, static_cast<uint8_t>(communication::Packet::COM_CHECK_RN_TO_L),
      &communication::onComCheckRocketNodeReceived);
  MsgPacketizer::subscribe(
      Serial1, static_cast<uint8_t>(communication::Packet::LIMIT_SWITCH_SYNC),
      &communication::onLimitSwitchSyncReceived);

  // ================= Raspberry Pi 4 無線受信コールバックの登録 (Serial) =================
  MsgPacketizer::subscribe(
      Serial, static_cast<uint8_t>(communication::Packet::RASPI_HEARTBEAT_R_TO_L),
      &raspi_wireless::onRaspiHeartbeatReceived);
  MsgPacketizer::subscribe(
      Serial, static_cast<uint8_t>(communication::Packet::RASPI_COMMAND),
      &raspi_wireless::onRaspiCommandReceived);

  // ================= シーケンス関係のタスク登録 =================
  Tasks.add(control::FILL_START, &control::setFillStart);
  Tasks.add(control::FILL_STOP, &control::setFillStop);
  Tasks.add(control::OXYGEN_START, &control::setOxygenStart);
  Tasks.add(control::OXYGEN_STOP, &control::setOxygenStop);
  Tasks.add(control::IGNITER_START, &control::setIgniterStart);
  Tasks.add(control::IGNITER_STOP, &control::setIgniterStop);
  Tasks.add(control::OPEN_START, &control::setOpenStart);
  Tasks.add(control::PURGE_START, &control::setPurgeStart);
  Tasks.add(control::PURGE_STOP, &control::setPurgeStop);
  Tasks.add(control::PLAY_MUSIC, [] { mp3_play(9); });

  control::setChristmasTreeStart();
  Tasks.add(&control::setChristmasTreeStop)->startOnceAfterSec(3.0);

  Tasks.add([] {
    Serial.println(">>> Requesting Remote Zero-Point Calibration...");
    communication::sendSensorZeroCalibReq();
  })->startOnceAfterSec(5.0);

  mp3_play(11);
}

void loop()
{
  // Serial1 (RS485) および Serial3 (Raspberry Pi 4 無線) のパケット解析
  MsgPacketizer::parse();

  Tasks.update();

  communication::accessLamp.update();
  control::statusLamp.update();
}

/// @brief RS485の送信モードを有効にする（DEピンをHIGH）
void communication::enableOutput()
{
  communication::sendEnableControl.on();
  communication::accessLamp.pulse(10);
}

/// @brief RS485の送信モードを無効にする（DEピンをLOW）
void communication::disableOutput() { communication::sendEnableControl.off(); }

/// @brief 入力電圧、システム電流、温度を監視
void power::measureTask()
{
  bool isLowVoltage = power::input.getVoltage_V() < 11.5;
  bool isOverloadedInput = power::input.getAmpere_A() > 3.0;
  bool isOverloadedBus = power::bus12.getAmpere_A() > 3.0;
  bool isOverheated = power::thermal.getTemperature_degC() > 100.0;

  power::lowVoltageLamp.set(isLowVoltage);

  bool isSequenceStartMistake = control::sequenceStart.isManualRaised() &&
                                !control::dump.isManualRaised() &&
                                !sequence::emergencyStopSequenceIsActive;

  if (isOverloadedInput || isOverloadedBus || isOverheated)
  {
    error::statusLamp.on();
  }

  if (isSequenceStartMistake)
  {
    error::statusLamp.on();
  }
  else if (control::dump.isManualRaised() || sequence::emergencyStopSequenceIsActive)
  {
    error::statusLamp.off();
  }
}

/// @brief ポーリングタスク
void communication::pollingTask()
{
  static uint8_t pollTarget = 0;

  if (pollTarget == 0)
  {
    uint8_t state =
        (control::shift.isRaised() << 0) | (control::fill.isRaised() << 1) |
        (control::dump.isRaised() << 2) | (control::oxygen.isRaised() << 3) |
        (control::igniter.isRaised() << 4) | (control::open.isRaised() << 5) |
        (control::close.isRaised() << 6) | (control::purge.isRaised() << 7);

    communication::enableOutput();
    MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::CONTROL_SYNC), state);
    MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::COM_CHECK_L_TO_S));
    Serial1.flush();
    communication::disableOutput();
  }
  else if (pollTarget == 1)
  {
    communication::enableOutput();
    MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::COM_CHECK_L_TO_N));
    Serial1.flush();
    communication::disableOutput();
  }
  else if (pollTarget == 2)
  {
    uint8_t syncState = 0;
    if (control::igniter.isRaised()) syncState |= (1 << 0);
    if (control::open.isRaised()) syncState |= (1 << 1);

    communication::enableOutput();
    MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::ROCKET_NODE_STATE_SYNC), syncState);
    MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::COM_CHECK_L_TO_RN));
    Serial1.flush();
    communication::disableOutput();
  }

  pollTarget = (pollTarget + 1) % 3;
}

void communication::sendSensorConfigSync()
{
  communication::enableOutput();
  MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::SENSOR_CONFIG_SYNC), 10.0f);
  Serial1.flush();
  communication::disableOutput();
}

void communication::sendSensorDummyCurrent(float current_mA)
{
  communication::enableOutput();
  MsgPacketizer::send(
      Serial1,
      static_cast<uint8_t>(communication::Packet::SENSOR_DUMMY_CURRENT_SYNC),
      current_mA);
  Serial1.flush();
  communication::disableOutput();
}

void communication::sendSensorCalibCoeff(float a, float b)
{
  communication::enableOutput();
  MsgPacketizer::send(
      Serial1,
      static_cast<uint8_t>(communication::Packet::SENSOR_CALIB_COEFF_SYNC), a, b);
  Serial1.flush();
  communication::disableOutput();
}

void communication::sendSensorZeroCalibReq()
{
  communication::enableOutput();
  MsgPacketizer::send(
      Serial1,
      static_cast<uint8_t>(communication::Packet::SENSOR_ZERO_CALIB_REQ));
  Serial1.flush();
  communication::disableOutput();
}

void communication::onLimitSwitchSyncReceived(uint8_t state)
{
#define DEBOUNCE_LIMIT_SWITCH
#ifdef DEBOUNCE_LIMIT_SWITCH
  static uint8_t lastRawState = 0;
  static uint8_t stableState = 0;
  static uint8_t confirmCount = 0;
  constexpr uint8_t REQUIRED_CONFIRMS = 2;

  if (state == lastRawState)
  {
    confirmCount++;
    if (confirmCount >= REQUIRED_CONFIRMS)
    {
      stableState = state;
    }
  }
  else
  {
    lastRawState = state;
    confirmCount = 1;
  }
  communication::limitSwitchState = stableState;
#else
  communication::limitSwitchState = state;
#endif
}

void communication::onFeedbackSyncReceived(uint8_t state)
{
  control::shiftFB.set(state & (1 << 0));
  control::fillFB.set(state & (1 << 1));
  control::dumpFB.set(state & (1 << 2));
  control::oxygenFB.set(state & (1 << 3));
  control::igniterFB.set(state & (1 << 4));
  control::openFB.set(state & (1 << 5));
  control::closeFB.set(state & (1 << 6));
  control::purgeFB.set(state & (1 << 7));

  if (control::safetyArmed.isManualRaised())
  {
    auto checkFault = [](bool cmdOn, bool fbOn,
                         uint8_t &count, const char *name) -> bool
    {
      bool mismatch = (cmdOn != fbOn);
      if (mismatch)
      {
        if (count < solenoid::FAULT_CONFIRM_COUNT)
          count++;

        if (count >= solenoid::FAULT_CONFIRM_COUNT)
        {
          Serial.print("[SOLENOID FAULT] ");
          Serial.print(name);
          Serial.println(" mismatch confirmed");
          return true;
        }
      }
      else
      {
        count = 0;
      }
      return false;
    };

    bool hasFault = false;
    hasFault |= checkFault(control::fill.isRaised(),   (state & (1 << 1)) != 0, solenoid::fillFaultCount,   "FILL");
    hasFault |= checkFault(control::dump.isRaised(),   (state & (1 << 2)) != 0, solenoid::dumpFaultCount,   "DUMP");
    hasFault |= checkFault(control::oxygen.isRaised(), (state & (1 << 3)) != 0, solenoid::oxygenFaultCount, "OXYGEN");
    hasFault |= checkFault(control::purge.isRaised(),  (state & (1 << 7)) != 0, solenoid::purgeFaultCount,  "PURGE");
    hasFault |= checkFault(control::open.isRaised(),   (state & (1 << 5)) != 0, solenoid::openFaultCount,   "OPEN");
    hasFault |= checkFault(control::close.isRaised(),  (state & (1 << 6)) != 0, solenoid::closeFaultCount,  "CLOSE");

    if (hasFault)
    {
      error::statusLamp.on();
    }
    else
    {
      error::statusLamp.off();
    }
  }
  else
  {
    solenoid::fillFaultCount   = 0;
    solenoid::dumpFaultCount   = 0;
    solenoid::oxygenFaultCount = 0;
    solenoid::purgeFaultCount  = 0;
    solenoid::openFaultCount   = 0;
    solenoid::closeFaultCount  = 0;
  }
}

void communication::onPressureSyncReceived(float pressure)
{
  n2o::tm1637.displayNumber(pressure);
  raspi_wireless::latestPressure_MPa = pressure;
}

void communication::onCurrentSyncReceived(float current_mA)
{
  Serial.print(">VESIM10 Current: ");
  Serial.println(current_mA);
}

void communication::onComCheckReceived()
{
  communication::preReceivedTime = millis();

  if (millis() - communication::preReceivedTime <= communication::timeout &&
      millis() - communication::preReceivedTime_Node <= communication::timeout)
  {
    communication::statusLamp.on();
    error::statusLamp.off();
  }
}

void communication::onComCheckNodeReceived()
{
  communication::preReceivedTime_Node = millis();

  if (millis() - communication::preReceivedTime <= communication::timeout &&
      millis() - communication::preReceivedTime_Node <= communication::timeout)
  {
    communication::statusLamp.on();
    error::statusLamp.off();
  }
}

void communication::onComCheckRocketNodeReceived()
{
  communication::preReceivedTime_RocketNode = millis();
  if (communication::isRocketNodeDisconnected) {
      communication::isRocketNodeDisconnected = false;
      Serial.println("[INFO] RocketNode Connected");
  }
}

void communication::onComCheckFailed()
{
  bool controllerTimeout = (millis() - communication::preReceivedTime > communication::timeout);
  bool nodeTimeout = (millis() - communication::preReceivedTime_Node > communication::timeout);
  bool rocketNodeTimeout = (millis() - communication::preReceivedTime_RocketNode > communication::timeout);

  if (controllerTimeout || nodeTimeout)
  {
    communication::statusLamp.off();
    error::statusLamp.on();

    if (sequence::fillSequenceIsActive || sequence::ignitionSequenceIsActive)
    {
      sequence::peacefulStop();
    }

    control::fillFB.off();
    control::dumpFB.off();
    control::oxygenFB.off();
    control::igniterFB.off();
    control::openFB.off();
    control::closeFB.off();
    control::purgeFB.off();
  }

  if (rocketNodeTimeout && !communication::isRocketNodeDisconnected)
  {
    communication::isRocketNodeDisconnected = true;
  }
}

void communication::sendTelemetryForPython()
{
  uint8_t cmd_state =
      (control::shift.isRaised() << 0) | (control::fill.isRaised() << 1) |
      (control::dump.isRaised() << 2) | (control::oxygen.isRaised() << 3) |
      (control::igniter.isRaised() << 4) | (control::open.isRaised() << 5) |
      (control::close.isRaised() << 6) | (control::purge.isRaised() << 7);

  uint8_t fb_state =
      (control::shiftFB.isHigh() << 0) | (control::fillFB.isHigh() << 1) |
      (control::dumpFB.isHigh() << 2) | (control::oxygenFB.isHigh() << 3) |
      (control::igniterFB.isHigh() << 4) | (control::openFB.isHigh() << 5) |
      (control::closeFB.isHigh() << 6) | (control::purgeFB.isHigh() << 7);

  Serial.print("V_DATA:");
  Serial.print(cmd_state);
  Serial.print(",");
  Serial.println(fb_state);
}

// =========================================================================
// Raspberry Pi 4 無線遠隔制御実装 (新規 3.0)
// =========================================================================

/// @brief Raspberry Pi 4 からのハートビート受信ハンドラ
void raspi_wireless::onRaspiHeartbeatReceived()
{
  raspi_wireless::lastHeartbeatTime = millis();
  if (!raspi_wireless::isWirelessConnected)
  {
    raspi_wireless::isWirelessConnected = true;
    Serial.println("[RASPI WIRELESS] Link Established / Restored.");
  }
}

/// @brief Raspberry Pi 4 からの遠隔制御コマンド受信ハンドラ
void raspi_wireless::onRaspiCommandReceived(uint8_t cmdType, uint8_t param)
{
  raspi_wireless::lastHeartbeatTime = millis(); // コマンド受信時も生存時刻更新

  // 【必須条件】無線接続がタイムアウトしている場合はリモートコマンドを拒否
  if (!raspi_wireless::isWirelessConnected)
  {
    Serial.println("[RASPI WIRELESS REJECT] Remote command rejected: Wireless link not connected!");
    error::statusLamp.on();
    return;
  }

  // 物理セーフティスイッチがONになっていない場合はリモート動作をブロック
  bool isArmed = control::safetyArmed.isManualRaised() || raspi_wireless::remoteArmingState;

  RemoteCmd cmd = static_cast<RemoteCmd>(cmdType);
  switch (cmd)
  {
  case RemoteCmd::EMERGENCY_STOP:
    Serial.println("[RASPI CMD] Remote Emergency Stop triggered!");
    sequence::emergencyStop();
    break;

  case RemoteCmd::PEACEFUL_STOP:
    Serial.println("[RASPI CMD] Remote Peaceful Stop triggered!");
    sequence::peacefulStop();
    break;

  case RemoteCmd::FILL_START:
    if (!isArmed)
    {
      Serial.println("[RASPI CMD REJECT] Remote Fill blocked: Safety not ARMED!");
      error::statusLamp.on();
      return;
    }
    Serial.println("[RASPI CMD] Remote Fill Sequence Start requested!");
    sequence::fill();
    break;

  case RemoteCmd::IGNITION_START:
    if (!isArmed)
    {
      Serial.println("[RASPI CMD REJECT] Remote Ignition blocked: Safety not ARMED!");
      error::statusLamp.on();
      return;
    }
    Serial.println("[RASPI CMD] Remote Ignition Sequence Start requested!");
    sequence::ignition();
    break;

  case RemoteCmd::ARM_SAFETY:
    raspi_wireless::remoteArmingState = (param != 0);
    Serial.print("[RASPI CMD] Remote Arming set to: ");
    Serial.println(raspi_wireless::remoteArmingState ? "ARMED" : "DISARMED");
    break;

  case RemoteCmd::VALVE_CONTROL:
    if (!isArmed)
    {
      Serial.println("[RASPI CMD REJECT] Remote Valve control blocked: Safety not ARMED!");
      error::statusLamp.on();
      return;
    }
    // param: bit0=shift, bit1=fill, bit2=dump, bit3=o2, bit4=igniter, bit5=open, bit6=close, bit7=purge
    if (param & (1 << 1)) control::fill.setAutomaticOn(); else control::fill.setAutomaticOff();
    if (param & (1 << 2)) control::dump.setAutomaticOn(); else control::dump.setAutomaticOff();
    if (param & (1 << 3)) control::oxygen.setAutomaticOn(); else control::oxygen.setAutomaticOff();
    if (param & (1 << 4)) control::igniter.setAutomaticOn(); else control::igniter.setAutomaticOff();
    if (param & (1 << 5)) control::open.setAutomaticOn(); else control::open.setAutomaticOff();
    if (param & (1 << 6)) control::close.setAutomaticOn(); else control::close.setAutomaticOff();
    if (param & (1 << 7)) control::purge.setAutomaticOn(); else control::purge.setAutomaticOff();
    Serial.print("[RASPI CMD] Remote Valve Control applied: 0x");
    Serial.println(param, HEX);
    break;

  case RemoteCmd::ZERO_CALIB:
    Serial.println("[RASPI CMD] Remote Zero Calibration requested!");
    communication::sendSensorZeroCalibReq();
    break;

  default:
    Serial.print("[RASPI CMD WARN] Unknown Remote Command: ");
    Serial.println(cmdType);
    break;
  }
}

/// @brief 無線接続生存監視タスク（無線化必須条件のインターロック）
void raspi_wireless::checkWirelessTask()
{
  // 相互生存確認（Heartbeat L -> R）を送信
  MsgPacketizer::send(Serial, static_cast<uint8_t>(communication::Packet::RASPI_HEARTBEAT_L_TO_R));

  bool timeoutOccurred = (millis() - raspi_wireless::lastHeartbeatTime > raspi_wireless::WIRELESS_TIMEOUT_MS);

  if (timeoutOccurred)
  {
    if (raspi_wireless::isWirelessConnected)
    {
      raspi_wireless::isWirelessConnected = false;
      Serial.println("[RASPI WIRELESS CRITICAL] Wireless link LOST! Interlock activated.");
      error::statusLamp.on();

      // ★【必須条件アルゴリズム】自動シーケンス進行中かつ無線途絶時は直ちに安全停止を発動
      if (sequence::fillSequenceIsActive || sequence::ignitionSequenceIsActive)
      {
        Serial.println("[RASPI WIRELESS FAIL-SAFE] Automatic sequence active during wireless drop -> Triggering Emergency Stop!");
        sequence::emergencyStop();
      }
    }
  }

  // 無線状態のステータス同期パケット送信
  uint8_t wirelessState = (raspi_wireless::isWirelessConnected ? 1 : 0);
  MsgPacketizer::send(Serial, static_cast<uint8_t>(communication::Packet::RASPI_WIRELESS_STATUS), wirelessState);
}

/// @brief Raspberry Pi 4 へ無線テレメトリを一括送信するタスク (10Hz)
void raspi_wireless::sendWirelessTelemetryTask()
{
  uint8_t cmd_state =
      (control::shift.isRaised() << 0) | (control::fill.isRaised() << 1) |
      (control::dump.isRaised() << 2) | (control::oxygen.isRaised() << 3) |
      (control::igniter.isRaised() << 4) | (control::open.isRaised() << 5) |
      (control::close.isRaised() << 6) | (control::purge.isRaised() << 7);

  uint8_t fb_state =
      (control::shiftFB.isHigh() << 0) | (control::fillFB.isHigh() << 1) |
      (control::dumpFB.isHigh() << 2) | (control::oxygenFB.isHigh() << 3) |
      (control::igniterFB.isHigh() << 4) | (control::openFB.isHigh() << 5) |
      (control::closeFB.isHigh() << 6) | (control::purgeFB.isHigh() << 7);

  uint8_t sequence_flag =
      (sequence::emergencyStopSequenceIsActive << 0) |
      (sequence::fillSequenceIsActive << 1) |
      (sequence::ignitionSequenceIsActive << 2) |
      (sequence::canConfirm << 3) |
      (raspi_wireless::isWirelessConnected << 4);

  // パケット送信: RASPI_TELEMETRY
  // cmd_state (1 byte), fb_state (1 byte), sequence_flag (1 byte), pressure_MPa (float), limitSwitchState (1 byte)
  MsgPacketizer::send(
      Serial,
      static_cast<uint8_t>(communication::Packet::RASPI_TELEMETRY),
      cmd_state, fb_state, sequence_flag, raspi_wireless::latestPressure_MPa, communication::limitSwitchState);
}

void control::handleManualTask()
{
  control::statusLamp.pulse(50);

  if (power::killButton.isHigh())
  {
    mp3_play(12);
    power::powerLamp.off(); delay(500); power::powerLamp.on(); delay(500);
    power::powerLamp.off(); delay(500); power::powerLamp.on(); delay(500);
    power::powerLamp.off(); delay(500); power::powerLamp.on(); delay(500);
    power::powerLamp.off();
    power::loadSwitch.off();
  }

  control::safetyArmed.setManual();
  if (!control::safetyArmed.isManualRaised() && !raspi_wireless::remoteArmingState)
  {
    if (sequence::emergencyStopSequenceIsActive ||
        sequence::fillSequenceIsActive || sequence::ignitionSequenceIsActive)
    {
      sequence::peacefulStop();
    }
    return;
  }

  if (sequence::fillSequenceIsActive || sequence::ignitionSequenceIsActive)
  {
    if (!communication::isSafetyGateMet())
    {
      Serial.println("[GATE] Limit switch released during sequence! Stopping sequence.");
      sequence::peacefulStop();
      error::statusLamp.on();
    }
  }

  control::emergencyStop.setManual();
  if (control::emergencyStop.isManualRaised())
  {
    sequence::emergencyStop();
  }

  control::sequenceStart.setManual();

  if (control::sequenceStart.isManualRaised() &&
      !control::dump.isManualRaised() &&
      !sequence::emergencyStopSequenceIsActive)
  {
    mp3_play(13);
    return;
  }

  if (control::sequenceStart.isManualRaised())
  {
    if (sequence::sequenceStartRiseCount == 0)
    {
      if (!(sequence::emergencyStopSequenceIsActive ||
            sequence::fillSequenceIsActive ||
            sequence::ignitionSequenceIsActive))
      {
        sequence::fill();
      }
      else if (sequence::canConfirm)
      {
        sequence::ignition();
      }
      else
      {
        sequence::peacefulStop();
      }
    }
    sequence::sequenceStartRiseCount++;
  }
  else
  {
    sequence::sequenceStartRiseCount = 0;
  }

  if (control::confirm1.isHigh() && control::confirm2.isHigh() && control::confirm3.isHigh())
  {
    sequence::ignition();
  }

  control::shift.setManual();
  control::fill.setManual();
  control::dump.setManual();
  control::oxygen.setManual();
  control::igniter.setManual();
  control::open.setManual();
  control::close.setManual();
  control::purge.setManual();
}

void sequence::emergencyStop()
{
  if (sequence::emergencyStopSequenceIsActive)
    return;

  sequence::emergencyStopSequenceIsActive = true;
  sequence::fillSequenceIsActive = false;
  sequence::ignitionSequenceIsActive = false;
  sequence::canConfirm = false;

  control::emergencyStop.setAutomaticOn();
  mp3_play(3);

  control::sequenceStart.setAutomaticOff();

  Tasks[control::PLAY_MUSIC]->stop();
  Tasks[control::FILL_START]->stop();
  Tasks[control::OXYGEN_START]->stop();
  Tasks[control::IGNITER_START]->stop();
  Tasks[control::FILL_STOP]->stop();
  Tasks[control::OPEN_START]->stop();
  Tasks[control::OXYGEN_STOP]->stop();
  Tasks[control::IGNITER_STOP]->stop();

  control::setEmergencyStop();
}

void sequence::peacefulStop()
{
  control::sequenceStart.setAutomaticOff();

  sequence::emergencyStopSequenceIsActive = false;
  sequence::fillSequenceIsActive = false;
  sequence::ignitionSequenceIsActive = false;
  sequence::canConfirm = false;

  control::sequenceStart.setAutomaticOff();
  control::emergencyStop.setAutomaticOff();

  Tasks[control::PLAY_MUSIC]->stop();
  Tasks[control::FILL_START]->stop();
  Tasks[control::OXYGEN_START]->stop();
  Tasks[control::IGNITER_START]->stop();
  Tasks[control::PURGE_START]->stop();
  Tasks[control::FILL_STOP]->stop();
  Tasks[control::OPEN_START]->stop();
  Tasks[control::OXYGEN_STOP]->stop();
  Tasks[control::IGNITER_STOP]->stop();
  Tasks[control::PURGE_STOP]->stop();

  mp3_stop();
  control::setPeacefulStop();
}

void sequence::fill()
{
  if (sequence::fillSequenceIsActive)
    return;

  if (sequence::emergencyStopSequenceIsActive)
    return;

  if (control::confirm1.isHigh() || control::confirm2.isHigh() || control::confirm3.isHigh())
  {
    sequence::peacefulStop();
    error::statusLamp.on();
    return;
  }

  if (!communication::statusLamp.isHigh())
  {
    sequence::peacefulStop();
    return;
  }

  if (!communication::isSafetyGateMet())
  {
    Serial.println("[GATE] sequence::fill() blocked: limit switch safety gate not met.");
    error::statusLamp.on();
    sequence::peacefulStop();
    mp3_play(13);
    return;
  }

  sequence::fillSequenceIsActive = true;
  sequence::canConfirm = false;

  control::sequenceStart.setAutomaticOn();
  mp3_play(10);

  Tasks[control::PLAY_MUSIC]->startOnceAfterSec(15.0);
  Tasks[control::FILL_START]->startOnceAfterSec(24.0);
}

void sequence::ignition()
{
  if (sequence::ignitionSequenceIsActive)
    return;

  if (!communication::statusLamp.isHigh())
  {
    sequence::peacefulStop();
    return;
  }

  if (sequence::emergencyStopSequenceIsActive)
    return;

  if (!control::fill.isAutomaticRaised())
    return;

  if (control::fill.isManualRaised())
    return;

  if (!communication::isSafetyGateMet())
  {
    Serial.println("[GATE] sequence::ignition() blocked: limit switch safety gate not met.");
    error::statusLamp.on();
    sequence::peacefulStop();
    mp3_play(13);
    return;
  }

  sequence::ignitionSequenceIsActive = true;
  sequence::canConfirm = false;

  control::sequenceStart.setAutomaticOn();
  mp3_play(4);

  Tasks[control::OXYGEN_START]->startOnceAfterMsec(50);
  Tasks[control::IGNITER_START]->startOnceAfterSec(1.0);
  Tasks[control::FILL_STOP]->startOnceAfterSec(10.0);
  Tasks[control::OPEN_START]->startOnceAfterSec(10.0);
  Tasks[control::OXYGEN_STOP]->startOnceAfterSec(10.5);
  Tasks[control::IGNITER_STOP]->startOnceAfterSec(10.5);
  Tasks[control::PURGE_START]->startOnceAfterSec(30.5);
  Tasks[control::PURGE_STOP]->startOnceAfterSec(35.5);
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
  control::sequenceStart.setTestOn();
  control::emergencyStop.setTestOn();
  control::shift.setTestOn();
  control::fill.setTestOn();
  control::dump.setTestOn();
  control::oxygen.setTestOn();
  control::igniter.setTestOn();
  control::open.setTestOn();
  control::close.setTestOn();
  control::purge.setTestOn();
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
  control::sequenceStart.setTestOff();
  control::emergencyStop.setTestOff();
  control::shift.setTestOff();
  control::fill.setTestOff();
  control::dump.setTestOff();
  control::oxygen.setTestOff();
  control::igniter.setTestOff();
  control::open.setTestOff();
  control::close.setTestOff();
  control::purge.setTestOff();
  control::shiftFB.setTestOff();
  control::fillFB.setTestOff();
  control::dumpFB.setTestOff();
  control::oxygenFB.setTestOff();
  control::igniterFB.setTestOff();
  control::openFB.setTestOff();
  control::closeFB.setTestOff();
  control::purgeFB.setTestOff();
}

void control::setEmergencyStop()
{
  control::fill.setAutomaticOff();
  control::oxygen.setAutomaticOff();
  control::igniter.setAutomaticOff();
  control::open.setAutomaticOff();
  control::close.setAutomaticOn();
  control::dump.setAutomaticOff();
  control::dump.setManualOff();
  control::purge.setAutomaticOn();
}

void control::setPeacefulStop()
{
  control::fill.setAutomaticOff();
  control::oxygen.setAutomaticOff();
  control::igniter.setAutomaticOff();
  control::open.setAutomaticOff();
  control::close.setAutomaticOff();
  control::dump.setAutomaticOff();
  control::purge.setAutomaticOff();
}

void control::setFillStart()
{
  control::fill.setAutomaticOn();
  sequence::canConfirm = true;
}

void control::setFillStop() { control::fill.setAutomaticOff(); }
void control::setOxygenStart() { control::oxygen.setAutomaticOn(); }
void control::setOxygenStop() { control::oxygen.setAutomaticOff(); }
void control::setIgniterStart() { control::igniter.setAutomaticOn(); }
void control::setIgniterStop() { control::igniter.setAutomaticOff(); }
void control::setOpenStart() { control::open.setAutomaticOn(); }
void control::setPurgeStart() { control::purge.setAutomaticOn(); }
void control::setPurgeStop() { control::purge.setAutomaticOff(); }
