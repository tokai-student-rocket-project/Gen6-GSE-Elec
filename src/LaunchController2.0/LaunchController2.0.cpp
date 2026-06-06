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
  // --- 操作卓（LaunchController）の物理スイッチとLED設定 ---

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
  // 高圧ガス試験で求めた値をここに記入してください
  const float SENSOR_FS_MPa = 10.0f; // センサーの定格圧力
  const float CALIB_SLOPE_A =
      0.5777; // 傾き (理論値は FS/16) //default -> SENSOR_FS_MPa / 16.0f
  // 2026/02/21 手動手押しポンプと精製水を用いて圧力を発生させた．
  const float CALIB_INTERCEPT_B = -CALIB_SLOPE_A * 4.0f; // 切片 (理論値は -a*4)
  // ----------------------------------------
} // namespace n2o

namespace error
{
  // エラー状態を示すランプ
  Output statusLamp(PIN_PK6); // ERR
} // namespace error

namespace solenoid
{
  // -------------------------------------------------------
  // チャタリング防止用デバウンス設定
  // -------------------------------------------------------
  // コマンド ≠ FB（サテライトコントローラー側の故障検知）をこの回数連続で検出した場合にエラー確定とする
  // onFeedbackSyncReceived は 5Hz で呈送されるため、5回 = 1秒間連続不一致で確定
  constexpr uint8_t FAULT_CONFIRM_COUNT = 5;

  // 各弁の「コマンド/FB不一致連続検出カウンタ」
  // 「弁をONに指示したのにFBがOFF」 or 「OFFに指示したのにFBがON」を連続検出した回数
  // 一致した場合はリセット
  uint8_t fillFaultCount   = 0;
  uint8_t dumpFaultCount   = 0;
  uint8_t oxygenFaultCount = 0;
  uint8_t purgeFaultCount  = 0;
  uint8_t openFaultCount   = 0;
  uint8_t closeFaultCount  = 0;
} // namespace solenoid

namespace communication
{
  // 通信パケットの種類を定義
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
    LIMIT_SWITCH_SYNC = 10,        // SatelliteNode のリミットスイッチ状態同期 (bit5=ch5)
    COM_CHECK_L_TO_N = 11,         // ランチ → ノード 生存確認
    COM_CHECK_N_TO_L = 12,         // ノード → ランチ 生存確認
  };

  // RS485の送信許可ピン (HIGHで送信有効)
  Output sendEnableControl(PIN_PA2); // CTL_RS485_DERE
  // 通信アクセスランプ
  Output accessLamp(PIN_PA4); // LED_RS485_ACCESS

  // 機体側から最後に通信を受信した時刻 (タイムアウト判定用)
  unsigned long preReceivedTime = 0;
  // ノード側から最後に通信を受信した時刻 (タイムアウト判定用)
  unsigned long preReceivedTime_Node = 0;
  // 通信タイムアウト時間 (ミリ秒)
  const long timeout = 5000;

  // SatelliteController 経由で転送された SatelliteNode のリミットスイッチ状態
  // bit0=ch0, bit1=ch1, ..., bit5=ch5
  uint8_t limitSwitchState = 0;

  /// @brief リミットスイッチが押されているか判定するヘルパー
  inline bool isCh0Pressed() { return (limitSwitchState >> 0) & 0x01; }
  inline bool isCh1Pressed() { return (limitSwitchState >> 1) & 0x01; }
  inline bool isCh2Pressed() { return (limitSwitchState >> 2) & 0x01; }
  inline bool isCh3Pressed() { return (limitSwitchState >> 3) & 0x01; }
  inline bool isCh4Pressed() { return (limitSwitchState >> 4) & 0x01; }
  inline bool isCh5Pressed() { return (limitSwitchState >> 5) & 0x01; }

  // RS485の送信を有効/無効化する関数
  void enableOutput();
  void disableOutput();

  // パケット送信関数
  void sendControlSync();
  void sendComCheck();
  void sendComCheckNode(); // ノード向け生存確認送信
  void sendSensorConfigSync();
  void sendSensorDummyCurrent(float current_mA);
  void sendSensorCalibCoeff(float a, float b);
  void sendSensorZeroCalibReq();
  void sendTelemetryForPython(); // PythonのGUI用にデータをシリアル出力する関数

  // パケット受信時のコールバック関数
  void onFeedbackSyncReceived(uint8_t state);
  void onPressureSyncReceived(float pressure);
  void onCurrentSyncReceived(float current_mA);
  void onComCheckReceived();
  void onComCheckNodeReceived(); // ノード向け生存確認受信
  void onComCheckFailed(); // タイムアウト時のフェールセーフ処理
  // SatelliteController 経由でリミットスイッチ状態を受信した際のコールバック
  void onLimitSwitchSyncReceived(uint8_t state);

  // 通信状態が正常であることを示すランプ
  Output statusLamp(PIN_PK5); // LED_COM
} // namespace communication

namespace simulation
{
  // シミュレーションタスク: 現在はダミー電流(0.0)を送り続ける処理
  void updateTask() { communication::sendSensorDummyCurrent(0.0f); }
} // namespace simulation

namespace storage
{
  // EEPROMの制御用インスタンス (I2Cアドレス: 0x50)
  Lib_24LC256 eeprom(0x50);
  // EEPROMの読み書きテスト用関数
  void readWriteTest();
} // namespace storage

void setup()
{
  // pinMode(PIN_PB7, OUTPUT);    // PIN_PB7 マイコンの頂点についてるLED
  // digitalWrite(PIN_PB7, HIGH); // PIN_PB7 マイコンの頂点についてるLED

  // EEPROMテスト用 (現在は無効化)
  // storage::eeprom.begin();

  // システムの電源をONにする
  power::loadSwitch.on();
  // 電源ランプを点灯する
  power::powerLamp.on();

  // FT232RL (USB - PCとの通信用シリアル)
  Serial.begin(115200);

  // LTC485 (RS485 - サテライトコントローラーとの通信用シリアル1)
  Serial1.begin(115200);
  // 初期状態でタイムアウト判定にならないよう、過去の時刻をセットしておく
  communication::preReceivedTime = millis();
  communication::preReceivedTime_Node = millis();

  // DFPlayer Mini (音声再生用シリアル2)
  Serial2.begin(9600);
  mp3_set_serial(Serial2);
  mp3_stop();
  mp3_set_volume(30);

  // TM1637 (7セグメントLEDディスプレイの初期化)
  n2o::tm1637.initialize();

  // INA219 (電力・電流モニタの初期化)
  Wire.begin();
  power::input.begin();
  power::bus12.begin();

  // ================= タスクの登録 (TaskManager) =================

  // Tasks.add(&storage::readWriteTest)->startFps(1);                 // 1Hzで実行
  // 各種センサー(電圧、電流、温度など)の計測タスク
  Tasks.add(&power::measureTask)->startFps(10); // 10Hzで実行
  // 操作卓の物理スイッチの状態を読み取り処理するタスク
  Tasks.add(&control::handleManualTask)->startFps(10); // 10Hzで実行
  // 機体側へ操作卓のスイッチ状態（コマンド）を送信するタスク
  Tasks.add(&communication::sendControlSync)->startFps(20); // 20Hzで実行
  // 生存確認(ハートビート)を定期的に送信するタスク
  Tasks.add(&communication::sendComCheck)->startFps(2); // 2Hzで実行
  Tasks.add(&communication::sendComCheckNode)->startFps(2); // 2Hzで実行
  // 通信がタイムアウトしていないか監視するタスク
  Tasks.add(&communication::onComCheckFailed)->startFps(2); // 2Hzで実行
  // PythonのGUI(ビジュアライザ)向けにシリアル通信でデータを送るタスク
  Tasks.add(&communication::sendTelemetryForPython)->startFps(10); // 10Hzで実行
  // シミュレーション用のタスク（ダミー電流送信など）
  Tasks.add(&simulation::updateTask)->startFps(2); // 2Hzで実行

  // 起動時に機体側へセンサの基本設定（フルスケール値など）を送信
  communication::sendSensorConfigSync();
  // 起動時に実測校正係数（傾き、切片）を機体に同期
  communication::sendSensorCalibCoeff(n2o::CALIB_SLOPE_A,
                                      n2o::CALIB_INTERCEPT_B);

  // ================= RS485 受信コールバックの登録 =================
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
  // SatelliteController が転送してくるリミットスイッチパケットを受信するコールバックを登録
  MsgPacketizer::subscribe(
      Serial1, static_cast<uint8_t>(communication::Packet::LIMIT_SWITCH_SYNC),
      &communication::onLimitSwitchSyncReceived);

  // ================= シーケンス関係のタスク登録 =================
  // ※ここでは登録のみ行い、後から startOnceAfterSec() などで呼び出す
  Tasks.add(control::FILL_START, &control::setFillStart);
  Tasks.add(control::FILL_STOP, &control::setFillStop);
  Tasks.add(control::OXYGEN_START, &control::setOxygenStart);
  Tasks.add(control::OXYGEN_STOP, &control::setOxygenStop);
  Tasks.add(control::IGNITER_START, &control::setIgniterStart);
  Tasks.add(control::IGNITER_STOP, &control::setIgniterStop);
  Tasks.add(control::OPEN_START, &control::setOpenStart);
  Tasks.add(control::PURGE_START, &control::setPurgeStart);
  Tasks.add(control::PURGE_STOP, &control::setPurgeStop);
  Tasks.add(control::PLAY_MUSIC, []
            { mp3_play(9); }); // 音声9番を再生

  // 起動時のLED全点灯（クリスマスツリーテスト）を開始
  control::setChristmasTreeStart();
  // 3秒後に全点灯テストを終了
  Tasks.add(&control::setChristmasTreeStop)->startOnceAfterSec(3.0);

  // 起動5秒後に、その場の環境（大気圧）に合わせてサテライトコントローラーにゼロ点校正をリクエストする
  Tasks.add([]
            {
        Serial.println(">>> Requesting Remote Zero-Point Calibration...");
        communication::sendSensorZeroCalibReq(); })
      ->startOnceAfterSec(5.0);

  // 起動完了の合図として音声11番を再生
  mp3_play(11);
}

void loop()
{
  // 受信したパケットを解析し、subscribeで登録したコールバックを呼び出す
  MsgPacketizer::parse();

  // 登録されたタスクのスケジュール管理と実行を行う
  Tasks.update();

  // ランプ(LED)の点滅(pulse/blink)状態を更新する
  communication::accessLamp.update();
  control::statusLamp.update();
}

/// @brief 外部EEPROMの読み書きテスト関数
void storage::readWriteTest()
{
  Serial.println("== EEPROM TEST ==");

  uint16_t testAddress = 0x0100; // テスト書き込み先アドレス
  const char *myText = "Hello, TSRP";
  uint16_t dataLength = strlen(myText) + 1; // ヌル文字を含む長さ
  Serial.print("Writing text: ");
  Serial.println(myText);

  // EEPROMへ書き込み
  storage::eeprom.writeBuffer(testAddress, (const uint8_t *)myText, dataLength);

  char readData[100]; // 読み出し用バッファ

  // EEPROMから読み出し
  storage::eeprom.readBuffer(testAddress, (uint8_t *)readData, dataLength);

  Serial.print("Read text: ");
  Serial.println(readData);

  // 書き込んだデータと読み出したデータが一致するか確認
  if (strcmp(myText, readData) == 0)
  {
    Serial.println("== Success ==");
  }
  else
  {
    Serial.println("== Error... ==");
  }
}

/// @brief RS485の送信モードを有効にする（DEピンをHIGH）
void communication::enableOutput()
{
  communication::sendEnableControl.on();
  communication::accessLamp.pulse(10); // 送信時にアクセスランプを一瞬(10ms)点灯
}

/// @brief RS485の送信モードを無効にする（DEピンをLOW、受信モードに戻す）
void communication::disableOutput() { communication::sendEnableControl.off(); }

/// @brief 入力電圧、システム電流、温度を監視し、異常があればエラーを出すタスク
void power::measureTask()
{
  // 各種ステータスの取得と閾値チェック
  bool isLowVoltage = power::input.getVoltage_V() < 11.5;           // 入力電圧が11.5V未満か
  bool isOverloadedInput = power::input.getAmpere_A() > 3.0;        // 全体消費電流が3.0Aを超えているか
  bool isOverloadedBus = power::bus12.getAmpere_A() > 3.0;          // 12V系の消費電流が3.0Aを超えているか
  bool isOverheated = power::thermal.getTemperature_degC() > 100.0; // 基板温度が100℃を超えているか

  // 低電圧ランプの点灯/消灯
  power::lowVoltageLamp.set(isLowVoltage);

  // 【誤操作チェック】
  // DUMP弁を閉じていない状態でシーケンス開始スイッチを押す、というミスをしたか判定
  bool isSequenceStartMistake = control::sequenceStart.isManualRaised() &&
                                !control::dump.isManualRaised() &&
                                !sequence::emergencyStopSequenceIsActive;

  // Teleplot用 (現在はコメントアウト)
  // Serial.print(">Tempelature_degC:");
  // Serial.println(power::thermal.getTemperature_degC()); // 温度モニター
  // Serial.print(">inputVoltage_V:");
  // Serial.println(power::input.getVoltage_V());
  // Serial.print(">inputAmpere_A:");
  // Serial.println(power::input.getAmpere_A());
  // Serial.print(">bus12Voltage_V:");
  // Serial.println(power::bus12.getVoltage_V());

  // ハードウェア的な異常（過電流、過熱）があればエラーランプ点灯
  if (isOverloadedInput || isOverloadedBus || isOverheated)
  {
    // HACK エラー
    error::statusLamp.on();
  }

  // 操作ミスがあればエラーランプ点灯
  if (isSequenceStartMistake)
  {
    error::statusLamp.on();
  }
  // DUMP弁が手動で開けられた（またはエマスト中）場合は、エラー状態を解除（正常系）
  else if (control::dump.isManualRaised() || sequence::emergencyStopSequenceIsActive)
  {
    error::statusLamp.off();
  }
}

/// @brief 現在の操作卓のスイッチ状態（電磁弁の開閉指令）をサテライトコントローラーに送信するタスク
void communication::sendControlSync()
{
  // ★重要：通信失敗（タイムアウト）している間は、送信処理自体をスキップする
  // (現在はコメントアウトされており、常に送信を試みるようになっている)
  // if (millis() - communication::preReceivedTime > communication::timeout) {
  //   return;
  // }

  // 各スイッチの状態（isRaised）を1ビットずつシフトして、1つのバイト(8ビット)にまとめる
  uint8_t state =
      (control::shift.isRaised() << 0) | (control::fill.isRaised() << 1) |
      (control::dump.isRaised() << 2) | (control::oxygen.isRaised() << 3) |
      (control::igniter.isRaised() << 4) | (control::open.isRaised() << 5) |
      (control::close.isRaised() << 6) | (control::purge.isRaised() << 7);

  // Serial.println(state, BIN); // ビット列を表示

  communication::enableOutput(); // 送信モードON

  // パケット送信
  MsgPacketizer::send(Serial1,
                      static_cast<uint8_t>(communication::Packet::CONTROL_SYNC),
                      state);

  Serial1.flush();                // 全てのデータが配線に送り出されるのを待つ
  communication::disableOutput(); // 受信モードに戻す
}

/// @brief センサの基本設定（フルスケール圧など）を機体側に同期する
void communication::sendSensorConfigSync()
{
  communication::enableOutput();
  MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::SENSOR_CONFIG_SYNC), 10.0f); // Default 10.0 MPa
  Serial1.flush();
  communication::disableOutput();
}

/// @brief シミュレーション用のダミー電流値を機体側に送信する
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

/**
 * @brief センサ校正係数の送信（同期）
 * @param a 傾き (Pressure = a * Current + b)
 * @param b 切片
 */
void communication::sendSensorCalibCoeff(float a, float b)
{
  communication::enableOutput();
  MsgPacketizer::send(
      Serial1,
      static_cast<uint8_t>(communication::Packet::SENSOR_CALIB_COEFF_SYNC), a,
      b);
  Serial1.flush();
  communication::disableOutput();
}

/**
 * @brief ゼロ点校正（大気圧環境でのキャリブレーション）の実行リクエストをサテライトコントローラに送信
 */
void communication::sendSensorZeroCalibReq()
{
  communication::enableOutput();
  MsgPacketizer::send(
      Serial1,
      static_cast<uint8_t>(communication::Packet::SENSOR_ZERO_CALIB_REQ));
  Serial1.flush();
  communication::disableOutput();
}

/// @brief 機体へ通信生存確認（ハートビート）を送信するタスク
void communication::sendComCheck()
{
  communication::enableOutput();
  MsgPacketizer::send(
      Serial1, static_cast<uint8_t>(communication::Packet::COM_CHECK_L_TO_S));
  Serial1.flush();
  communication::disableOutput();
}

/// @brief ノードへ通信生存確認（ハートビート）を送信するタスク
void communication::sendComCheckNode()
{
  communication::enableOutput();
  MsgPacketizer::send(
      Serial1, static_cast<uint8_t>(communication::Packet::COM_CHECK_L_TO_N));
  Serial1.flush();
  communication::disableOutput();
}

/// @brief SatelliteController から転送されたリミットスイッチ状態を受信するコールバック
/// SatelliteNode (ch0〜ch5) → SatelliteController → (LIMIT_SWITCH_SYNCパケット) → LaunchController
/// 内部変数 limitSwitchState に保存し、isCh0Pressed() 〜 isCh5Pressed() で参照する。
void communication::onLimitSwitchSyncReceived(uint8_t state)
{
  communication::limitSwitchState = state;

  // teleplot / デバッグ用: リミットスイッチ各chの状態をシリアルモニターで確認できるようにする
  Serial.print(">limitSwitch_ch0:");
  Serial.println(communication::isCh0Pressed() ? 1 : 0);
  Serial.print(">limitSwitch_ch1:");
  Serial.println(communication::isCh1Pressed() ? 1 : 0);
  Serial.print(">limitSwitch_ch2:");
  Serial.println(communication::isCh2Pressed() ? 1 : 0);
  Serial.print(">limitSwitch_ch3:");
  Serial.println(communication::isCh3Pressed() ? 1 : 0);
  Serial.print(">limitSwitch_ch4:");
  Serial.println(communication::isCh4Pressed() ? 1 : 0);
  Serial.print(">limitSwitch_ch5:");
  Serial.println(communication::isCh5Pressed() ? 1 : 0);
}

/// @brief 機体から実際の電磁弁状態（フィードバック）を受信した際のコールバック
void communication::onFeedbackSyncReceived(uint8_t state)
{
  // 受信したビット列を展開し、各フィードバック用LEDに状態を反映させる
  control::shiftFB.set(state & (1 << 0));
  control::fillFB.set(state & (1 << 1));
  control::dumpFB.set(state & (1 << 2));
  control::oxygenFB.set(state & (1 << 3));
  control::igniterFB.set(state & (1 << 4));
  control::openFB.set(state & (1 << 5));
  control::closeFB.set(state & (1 << 6));
  control::purgeFB.set(state & (1 << 7));

  // コマンド vs FB の不一致による故障デバウンスチェック
  // 「弁をONに指示したのにFBがOFF」 or 「OFFに指示したのにFBがON」を連続検出する
  // ※ Armed状態のときのみチェックする（未展開時は常にFB=OFFなので誤検知防止）
  if (control::safetyArmed.isManualRaised())
  {
    // 各弁 of コマンド状態（isRaised）とFB状態を比較するラムダ
    // mismatchが継続したらカウントアップし、一定回数に達したら故障確定（trueを返す）
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
          Serial.print(": CMD=");
          Serial.print(cmdOn ? "ON" : "OFF");
          Serial.print(" FB=");
          Serial.print(fbOn ? "ON" : "OFF");
          Serial.print(" (mismatch confirmed ");
          Serial.print(solenoid::FAULT_CONFIRM_COUNT);
          Serial.println(" times)");
          return true;
        }
      }
      else
      {
        // 一致したのでカウンタリセット
        count = 0;
      }
      return false;
    };

    bool hasFault = false;
    hasFault |= checkFault(control::fill.isRaised(),   (state & (1 << 1)) != 0,
                           solenoid::fillFaultCount,   "FILL");
    hasFault |= checkFault(control::dump.isRaised(),   (state & (1 << 2)) != 0,
                           solenoid::dumpFaultCount,   "DUMP");
    hasFault |= checkFault(control::oxygen.isRaised(), (state & (1 << 3)) != 0,
                           solenoid::oxygenFaultCount, "OXYGEN");
    hasFault |= checkFault(control::purge.isRaised(),  (state & (1 << 7)) != 0,
                           solenoid::purgeFaultCount,  "PURGE");
    hasFault |= checkFault(control::open.isRaised(),   (state & (1 << 5)) != 0,
                           solenoid::openFaultCount,   "OPEN");
    hasFault |= checkFault(control::close.isRaised(),  (state & (1 << 6)) != 0,
                           solenoid::closeFaultCount,  "CLOSE");

    if (hasFault)
    {
      error::statusLamp.on();
    }
    else
    {
      // 故障判定がすべてクリアされた（またはデバウンス回数未満）場合はエラーランプを消灯
      error::statusLamp.off();
    }
  }
  else
  {
    // Armed解除中はカウンタをリセット
    solenoid::fillFaultCount   = 0;
    solenoid::dumpFaultCount   = 0;
    solenoid::oxygenFaultCount = 0;
    solenoid::purgeFaultCount  = 0;
    solenoid::openFaultCount   = 0;
    solenoid::closeFaultCount  = 0;
  }

  // communication::statusLamp.blink();
}

/// @brief 機体から算出された圧力値を受信した際のコールバック
void communication::onPressureSyncReceived(float pressure)
{
  // 7セグメントディスプレイに圧力を表示
  n2o::tm1637.displayNumber(pressure);

  // communication::statusLamp.blink();
}

/**
 * @brief 機体から受信した生の電流値(mA)をシリアル出力
 */
void communication::onCurrentSyncReceived(float current_mA)
{
  Serial.print(">VESIM10 Current: ");
  Serial.println(current_mA);
}

/// @brief 機体から通信生存確認（ハートビート）を受信した際のコールバック
void communication::onComCheckReceived()
{
  // 通信受信時刻を更新
  communication::preReceivedTime = millis();

  // 両方がタイムアウト以内の場合のみ通信正常ランプを点灯
  if (millis() - communication::preReceivedTime <= communication::timeout &&
      millis() - communication::preReceivedTime_Node <= communication::timeout)
  {
    communication::statusLamp.on();
    error::statusLamp.off();
  }
}

/// @brief ノードから通信生存確認（ハートビート）を受信した際のコールバック
void communication::onComCheckNodeReceived()
{
  // 通信受信時刻を更新
  communication::preReceivedTime_Node = millis();

  // 両方がタイムアウト以内の場合のみ通信正常ランプを点灯
  if (millis() - communication::preReceivedTime <= communication::timeout &&
      millis() - communication::preReceivedTime_Node <= communication::timeout)
  {
    communication::statusLamp.on();
    error::statusLamp.off();
  }
}

/// @brief 機体またはノードとの通信が途絶した（タイムアウト）場合に実行されるフェールセーフタスク
void communication::onComCheckFailed()
{
  bool controllerTimeout = (millis() - communication::preReceivedTime > communication::timeout);
  bool nodeTimeout = (millis() - communication::preReceivedTime_Node > communication::timeout);

  if (controllerTimeout || nodeTimeout)
  {
    static unsigned long lastLogTime = 0;
    if (millis() - lastLogTime > 2000) // 2秒おきに出力
    {
      if (controllerTimeout && nodeTimeout) {
        Serial.println("[COM ERROR] Both SatelliteController and SatelliteNode timed out!");
      } else if (controllerTimeout) {
        Serial.println("[COM ERROR] SatelliteController timed out!");
      } else {
        Serial.println("[COM ERROR] SatelliteNode timed out!");
      }
      lastLogTime = millis();
    }

    communication::statusLamp.off();
    error::statusLamp.on();

    // ★重要：シーケンス実行中に通信が切れたら、強制的にシーケンスを止める
    if (sequence::fillSequenceIsActive || sequence::ignitionSequenceIsActive)
    {
      sequence::peacefulStop();
    }

    // 実際の電磁弁の状態が分からないため、操作卓側のフィードバックLEDも安全のため消灯させる
    control::fillFB.off();
    control::dumpFB.off();
    control::oxygenFB.off();
    control::igniterFB.off();
    control::openFB.off();
    control::closeFB.off();
    control::purgeFB.off();
  }
}

/**
 * @brief Python GUI (Visualizer) 用に現在の状態をSerial出力
 * フォーマット: V_DATA:<CMD_BITS>,<FB_BITS>
 */
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

/// @brief 操作卓の物理スイッチ・ボタン入力を読み取り、各種制御を行うメインタスク
void control::handleManualTask()
{
  // タスクが稼働中であることを示すランプを点滅させる
  control::statusLamp.pulse(50);

  // ================= キルスイッチ処理 =================
  if (power::killButton.isHigh())
  {
    mp3_play(12); // 電源を落とすときの音声(アラート)を再生

    // システム電源を落とす前に、電源ランプを数回点滅させる
    // 【注意】ここで delay(500) を使用しているため、この間 TaskManager や RS485 通信は完全に停止します
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
    power::powerLamp.on();
    delay(500);
    power::powerLamp.off();
    delay(500);

    // 最後にロードスイッチをオフにして物理的に電源供給を遮断する
    power::loadSwitch.off();
  }

  // ================= セーフティースイッチ処理 =================
  // セーフティースイッチの状態を読み取り
  control::safetyArmed.setManual();
  // Armed(ON)でなければ、これ以降の手動操作・シーケンス処理を行わずに関数を抜ける
  if (!control::safetyArmed.isManualRaised())
  {
    // もしシーケンスが進行中だった場合は、安全のため穏便にシーケンスを停止させる
    if (sequence::emergencyStopSequenceIsActive ||
        sequence::fillSequenceIsActive || sequence::ignitionSequenceIsActive)
    {
      sequence::peacefulStop();
    }

    return;
  }

  // ================= エマージェンシーストップ（緊急停止）処理 =================
  control::emergencyStop.setManual();
  if (control::emergencyStop.isManualRaised())
  {
    // エマストボタンが押されたら、直ちにエマストシーケンス（安全な状態への移行）を発動
    sequence::emergencyStop();
  }

  // ================= 充填シーケンス処理 =================
  control::sequenceStart.setManual();

  // DUMPがCLOSEされていない場合、シーケンス開始ボタンを押してもシーケンスを開始させないための安全ロック
  if (control::sequenceStart.isManualRaised() &&
      !control::dump.isManualRaised() &&
      !sequence::emergencyStopSequenceIsActive)
  {
    mp3_play(13); // エラー音を再生
    return;
  }

  // シーケンス開始スイッチが押された場合の処理
  if (control::sequenceStart.isManualRaised())
  {
    // スイッチが「新しく押された瞬間」だけ処理を進める（長押しによる連打を防止）
    if (sequence::sequenceStartRiseCount == 0)
    {
      // どのシーケンスも実行されていなければ、充填シーケンスを開始
      if (!(sequence::emergencyStopSequenceIsActive ||
            sequence::fillSequenceIsActive ||
            sequence::ignitionSequenceIsActive))
      {
        sequence::fill();
      }
      // すでに充填シーケンスが完了し、点火確認状態であれば、点火シーケンスを開始
      else if (sequence::canConfirm)
      {
        sequence::ignition();
      }
      // それ以外（進行中のシーケンスを中断したい場合など）はシーケンスを止める
      else
      {
        sequence::peacefulStop();
      }
    }

    sequence::sequenceStartRiseCount++;
  }
  else
  {
    // スイッチが離されたらカウントをリセット
    sequence::sequenceStartRiseCount = 0;
  }

  // ================= 点火シーケンス処理（3人同時押し） =================
  // confirm1, confirm2, confirm3 がすべて同時に押されたら点火シーケンスを開始する
  if (control::confirm1.isHigh() && control::confirm2.isHigh() && control::confirm3.isHigh())
  {
    sequence::ignition();
  }

  // ================= 手動操作処理 =================
  // 各電磁弁の手動トグルスイッチの状態を読み取り、出力設定に反映させる
  // (シーケンスによってAutomaticに制御されていない場合のみ有効)
  control::shift.setManual();
  control::fill.setManual();
  control::dump.setManual();
  control::oxygen.setManual();
  control::igniter.setManual();
  control::open.setManual();
  control::close.setManual();
  control::purge.setManual();
}

/// @brief 緊急停止シーケンスを発動し、各電磁弁を安全な状態へ強制移行する
void sequence::emergencyStop()
{
  // 重複実行防止
  if (sequence::emergencyStopSequenceIsActive)
    return;

  // フラグの更新
  sequence::emergencyStopSequenceIsActive = true;
  sequence::fillSequenceIsActive = false;
  sequence::ignitionSequenceIsActive = false;
  sequence::canConfirm = false;

  control::emergencyStop.setAutomaticOn(); // エマストランプ点灯
  mp3_play(3);                             // 0102_emergencyStop.mp3

  control::sequenceStart.setAutomaticOff();

  // 進行中のTaskManagerタスク（音楽再生や時限発火のバルブ操作）をすべて強制停止する
  Tasks[control::PLAY_MUSIC]->stop();
  Tasks[control::FILL_START]->stop();
  Tasks[control::OXYGEN_START]->stop();
  Tasks[control::IGNITER_START]->stop();
  Tasks[control::FILL_STOP]->stop();
  Tasks[control::OPEN_START]->stop();
  Tasks[control::OXYGEN_STOP]->stop();
  Tasks[control::IGNITER_STOP]->stop();

  // 電磁弁を安全状態(DUMP開など)にセット
  control::setEmergencyStop();
}

/// @brief 通常のシーケンス中断（穏便ストップ）処理。実行中のシーケンスをキャンセルする
void sequence::peacefulStop()
{
  control::sequenceStart.setAutomaticOff();

  // すべてのシーケンスフラグを解除
  sequence::emergencyStopSequenceIsActive = false;
  sequence::fillSequenceIsActive = false;
  sequence::ignitionSequenceIsActive = false;
  sequence::canConfirm = false;

  control::sequenceStart.setAutomaticOff();
  control::emergencyStop.setAutomaticOff();

  // 進行中のTaskManagerタスクをすべて停止する
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

  mp3_stop();                 // 音楽・音声を停止
  control::setPeacefulStop(); // 各操作の自動制御状態を解除
}

/// @brief 自動充填シーケンスを開始する
void sequence::fill()
{
  // 重複実行防止
  if (sequence::fillSequenceIsActive)
    return;

  // エマスト中は充填シーケンスを始めない
  if (sequence::emergencyStopSequenceIsActive)
    return;

  // シーケンス開始時点で充填確認(confirm1〜3)が既に押されていたらエラーを吐いて中断
  if (control::confirm1.isHigh() || control::confirm2.isHigh() || control::confirm3.isHigh())
  {
    // HACK エラー
    sequence::peacefulStop();
    error::statusLamp.on();
    return;
  }

  // 通信状態が正常でない（通信切断中）場合はシーケンスに進めない
  if (!communication::statusLamp.isHigh())
  {
    sequence::peacefulStop();
    return;
  }

  // ★ ch5 リミットスイッチによる安全ゲート
  // SatelliteNode の ch5 が押されていない場合は充填シーケンスを開始しない
  // if (!communication::isCh5Pressed())
  // {
  //   Serial.println("[GATE] sequence::fill() blocked: ch5 limit switch not pressed.");
  //   error::statusLamp.on(); // ch5 未押下をエラーランプで通知
  //   return;
  // }

  // フラグ更新
  sequence::fillSequenceIsActive = true;
  sequence::canConfirm = false;

  control::sequenceStart.setAutomaticOn(); // シーケンスランプ点灯
  mp3_play(10);                            // 音声再生

  // 15秒後に音楽再生タスクをスケジュール
  Tasks[control::PLAY_MUSIC]->startOnceAfterSec(15.0);
  // 24秒後に実際の充填（FILL開）タスクをスケジュール
  Tasks[control::FILL_START]->startOnceAfterSec(24.0);
}

/// @brief 自動点火シーケンスを開始する
void sequence::ignition()
{
  // 重複実行防止
  if (sequence::ignitionSequenceIsActive)
    return;

  // 通信失敗時は点火シーケンスに進めない
  if (!communication::statusLamp.isHigh())
  {
    sequence::peacefulStop();
    return;
  }

  // エマスト中は点火シーケンスを始めない
  if (sequence::emergencyStopSequenceIsActive)
    return;

  // 充填シーケンス（FILL_START）が走っていない（自動的にFILLが上がっていない）場合は始めない
  if (!control::fill.isAutomaticRaised())
    return;

  // 手動でFILLをONにしている最中は点火シーケンスを始めない
  if (control::fill.isManualRaised())
    return;

  // ★ ch5 リミットスイッチによる安全ゲート
  // SatelliteNode の ch5 が押されていない場合は点火シーケンスに進めない
  // if (!communication::isCh5Pressed())
  // {
  //   Serial.println("[GATE] sequence::ignition() blocked: ch5 limit switch not pressed.");
  //   error::statusLamp.on(); // ch5 未押下をエラーランプで通知
  //   return;
  // }

  // フラグ更新
  sequence::ignitionSequenceIsActive = true;
  sequence::canConfirm = false;

  control::sequenceStart.setAutomaticOn();
  mp3_play(4); // 0104_ignitionSequenceStart

  // Tasks[control::OXYGEN_START]->startOnceAfterSec(
  //     4.5); // 「充填が確認されました．点火します．5秒前...」←これが4.5秒くらいかかる

  Tasks[control::OXYGEN_START]->startOnceAfterMsec(50); // THR-E820L で点火

  // 点火器の通電開始 (6秒後)
  // Tasks[control::IGNITER_START]->startOnceAfterSec(6.0);
  Tasks[control::IGNITER_START]->startOnceAfterSec(1.0); // THR-E820L で点火

  // 充填(FILL)バルブを閉じる (10秒後: カウントダウン終了付近)
  Tasks[control::FILL_STOP]->startOnceAfterSec(10.0);
  // 主流路(OPEN)バルブを開く (10秒後)
  Tasks[control::OPEN_START]->startOnceAfterSec(10.0);

  // 酸素(O2)バルブと点火器(IGNITER)をオフにする (10.5秒後)
  Tasks[control::OXYGEN_STOP]->startOnceAfterSec(10.5);
  Tasks[control::IGNITER_STOP]->startOnceAfterSec(10.5);

  // 燃焼終了後、パージ(PURGE)バルブを開けて配管内をパージする (20.5秒後)
  Tasks[control::PURGE_START]->startOnceAfterSec(30.5);
  // パージを終了する (25.5秒後)
  Tasks[control::PURGE_STOP]->startOnceAfterSec(35.5);
}

/// @brief 起動時にすべてのLEDやフィードバックランプをテスト点灯する
void control::setChristmasTreeStart()
{
  n2o::tm1637.displayNumber(8.8); // 7セグに8.8を表示
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

/// @brief テスト点灯（クリスマスツリー）を終了する
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

/// @brief エマスト（緊急停止）時に設定する各バルブ・制御状態
void control::setEmergencyStop()
{
  control::fill.setAutomaticOff();
  control::oxygen.setAutomaticOff();
  control::igniter.setAutomaticOff();
  control::open.setAutomaticOff();
  // 万一のためCLOSEをONにして主流路を閉じる
  control::close.setAutomaticOn();

  // DUMPを開放(CLOSE)してタンク内の圧力を抜く
  // control::dump.setAutomaticOn();
  control::dump.setAutomaticOff(); // ※シークエンス開始前にDUMPをCLOSEするための処理
  control::dump.setManualOff();

  // パージをONにして消化する
  control::purge.setAutomaticOn();
}

/// @brief 通常のシーケンス中断時に各自動制御フラグをリセットする
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

/// @brief 各シーケンス動作でTaskManagerから呼び出されるヘルパー関数群
void control::setFillStart()
{
  control::fill.setAutomaticOn();
  // FILLが始まったら、点火前の最終確認が可能になる
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
