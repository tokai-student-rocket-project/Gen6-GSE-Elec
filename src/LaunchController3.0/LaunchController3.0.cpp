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

// =====================================================================
//  デモモード設定 (DEMO_MODE)
// ---------------------------------------------------------------------
//  下の #define のコメントを外してビルドすると「デモモード」になります。
//  （または PlatformIO 等のビルドフラグで -DDEMO_MODE を指定）
//
//  想定する構成:
//    ランチコントローラー ＋ サテライトコントローラー は接続した状態。
//    SatelliteNode（リミットスイッチ）と RocketNode は未接続。
//  この状態でも充填〜点火シーケンスを最後まで流せるようにします。
//
//  デモモードで変わるのは以下の「SatelliteNode 依存部分」だけです:
//    - リミットスイッチによる安全ゲート(isSafetyGateMet)を常に成立扱いにする
//  ※ 3.0 は SatelliteNode / RocketNode をポーリングしない (下記 NO_LIMIT_SWITCH_GATE 参照)。
//  ★ ランチ⇔サテライト間の通信タイムアウト・フェールセーフはそのまま有効です。
//    サテライトとの通信が切れれば、通常どおりシーケンスは強制停止されます。
//
//  ■ 安全に関する注意
//    - 現場で運用する際は、必ず DEMO_MODE を無効（コメントアウト）にして
//      ビルドしたファームウェアを使用してください。
//    - 起動時にシリアルへ警告バナーを表示します。
// =====================================================================
// #define DEMO_MODE

// =====================================================================
//  通信構成 (3.0)
// ---------------------------------------------------------------------
//  Launch - MOXA NPort - WiFi | WiFi - MOXA NPort - Satellite の1対1構成。
//  SatelliteNode / RocketNode は接続しないため、ポーリングは SatelliteController のみ。
//
//  -D NO_LIMIT_SWITCH_GATE (platformio.ini の env:Launch3 で指定)
//    SatelliteNode のリミットスイッチ (ch5) による安全ゲート isSafetyGateMet() を
//    常に成立扱いにする。Node を接続しないため、指定しないとシーケンスを開始できない。
//    Node を戻す場合はこのフラグを外し、pollingTask に Node のスロットを戻すこと。
// =====================================================================

// =====================================================================
//  RS485 通信デバッグ
// ---------------------------------------------------------------------
//  どちらも PlatformIO のビルドフラグで有効化する (現場運用では無効にすること)。
//    -D RS485_RX_LOG     … 受信ログ。Serial(USB)へパケット受信ごとの [RX] 行と、
//                          1秒ごとの [RX-STAT] (受信バイト数/パケット種別ごとの件数) を出力
//    -D RS485_ECHO_TEST  … エコーテスト。通常動作(ポーリング等)は停止し、Serial1 で
//                          受信したバイトをそのまま送り返し、16進で Serial(USB)に表示
//  ※ ログ行は Python GUI 向けのテレメトリ(V_DATA:...)と同じ Serial に出る。
// =====================================================================
#ifdef RS485_RX_LOG
#define RX_LOG(id) communication::rxLog(communication::id)
#else
#define RX_LOG(id) ((void)0)
#endif

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

  // キルスイッチ押下後、電源を落とすまでの待機中か (待機中は全弁OFFを送信する)
  bool isShuttingDown = false;
  unsigned long shutdownStartTime = 0;
  constexpr unsigned long SHUTDOWN_DELAY_MS = 3500; // 電源ランプを点滅させてから電源を落とすまでの時間
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

  // エラーの発生源。各処理は自分のビットだけを立て/下ろしし、ランプはその OR で点灯する
  // (以前は各処理が直接 on/off していたため、別の処理が点けたエラーを消してしまっていた)
  enum Source : uint8_t
  {
    POWER = 1 << 0,             // 過電流・過熱 (measureTask)
    SEQ_START_MISTAKE = 1 << 1, // DUMPを閉じずにシーケンス開始スイッチを押している (measureTask)
    SOLENOID = 1 << 2,          // 指令とFBの不一致 (onFeedbackSyncReceived)
    COM_TIMEOUT = 1 << 3,       // サテライトとの通信途絶 (onComCheckFailed)
    OPERATION = 1 << 4,         // 安全ゲート不成立などの操作エラー。Armed を OFF にするまで保持
  };
  uint8_t flags = 0;

  void set(uint8_t source, bool isActive) { flags = isActive ? (flags | source) : (flags & ~source); }
  void updateLamp() { statusLamp.set(flags != 0); }
} // namespace error

namespace solenoid
{
  // -------------------------------------------------------
  // チャタリング防止用デバウンス設定
  // -------------------------------------------------------
  // コマンド ≠ FB（サテライトコントローラー側の故障検知）をこの回数連続で検出した場合にエラー確定とする
  // FEEDBACK_SYNC は問い合わせ1回ごとに届く (POLL_SLOT_MS=150 〜 REPLY_TIMEOUT_MS=400 間隔) ため、
  // 5回 = 約0.75〜2秒間の連続不一致で確定
  constexpr uint8_t FAULT_CONFIRM_COUNT = 5;

  // 各弁の「コマンド/FB不一致連続検出カウンタ」
  // 「弁をONに指示したのにFBがOFF」 or 「OFFに指示したのにFBがON」を連続検出した回数
  // 一致した場合はリセット
  uint8_t fillFaultCount = 0;
  uint8_t dumpFaultCount = 0;
  uint8_t oxygenFaultCount = 0;
  uint8_t purgeFaultCount = 0;
  uint8_t openFaultCount = 0;
  uint8_t closeFaultCount = 0;
} // namespace solenoid

namespace communication
{
  // 通信パケットの種類を定義
  enum class Packet : uint8_t
  {
    CONTROL_SYNC = 0,              // 操作卓(Launch)からの制御コマンド同期 (state, seq)。生存確認も兼ねる
    FEEDBACK_SYNC = 1,             // サテライトコントローラーからのフィードバック（電磁弁の実際の状態など）
    PRESSURE_SYNC = 2,             // 算出された圧力値(MPa)の同期
    COM_CHECK_L_TO_S = 3,          // (未使用) 操作卓から機体への生存確認。CONTROL_SYNC に統合した
    COM_CHECK_S_TO_L = 4,          // 機体から操作卓への生存確認 (seq のエコー, 設定済みフラグ)
    SENSOR_CONFIG_SYNC = 5,        // センサの基本設定（フルスケールなど）の機体への同期
    SENSOR_DUMMY_CURRENT_SYNC = 6, // シミュレーション用のダミー電流値の同期
    SENSOR_CALIB_COEFF_SYNC = 7,   // 校正係数(a, b)の同期用
    SENSOR_ZERO_CALIB_REQ = 8,     // 機体に対するゼロ点校正実行の要求
    SENSOR_CURRENT_SYNC = 9,       // 機体から送信される生の電流値(mA)の同期
    LIMIT_SWITCH_SYNC = 10,        // SatelliteNode のリミットスイッチ状態同期 (bit5=ch5)
    COM_CHECK_L_TO_N = 11,         // (未使用: Node 非接続) ランチ → ノード 生存確認
    COM_CHECK_N_TO_L = 12,         // (未使用: Node 非接続) ノード → ランチ 生存確認
    COM_CHECK_L_TO_RN = 13,        // (未使用: Node 非接続) 親機 ➔ ロケットノード 生存確認
    COM_CHECK_RN_TO_L = 14,        // (未使用: Node 非接続) ロケットノード ➔ 親機 生存確認
    ROCKET_NODE_STATE_SYNC = 15,   // (未使用: Node 非接続) 親機 ➔ 状態同期（イグニッション/OPEN状態）
  };

  // RS485の送信許可ピン (HIGHで送信有効)
  Output sendEnableControl(PIN_PA2); // CTL_RS485_DERE
  // 通信アクセスランプ
  Output accessLamp(PIN_PA4); // LED_RS485_ACCESS

  // 機体側から最後に通信を受信した時刻 (タイムアウト判定用)
  unsigned long preReceivedTime = 0;

  // 通信タイムアウト時間 (ミリ秒)
  // MOXA NPort + WiFi 経由では遅延が揺らぐため、ポーリング (POLL_SLOT_MS 〜 REPLY_TIMEOUT_MS 間隔) の数回分を見込む。
  // 実測して調整すること
  const long timeout = 2000;

  // ---- MOXA NPort + WiFi 経由のポーリング設定 ----
  // 応答が届く時刻が読めないため、「問い合わせ → 応答 (またはタイムアウト) → 次の送信」の順を必ず守る。
  // DE/RE が共用 (CTL_RS485_DERE) なので、送信中に届いた応答は受信できない。
  // 問い合わせ間隔の最短値 (ミリ秒)
  constexpr unsigned long POLL_SLOT_MS = 150;
  // 応答が来ない場合に次の送信を始めるまでの最大待ち時間 (ミリ秒)
  constexpr unsigned long REPLY_TIMEOUT_MS = 400;
  // 送信前に「受信が途切れた」とみなす無通信時間 (マイクロ秒)。115200bps で約23バイト分
  constexpr unsigned long RX_IDLE_US = 2000;
  // 受信の途切れを待つ上限 (ミリ秒)
  constexpr unsigned long RX_IDLE_MAX_WAIT_MS = 20;

  // 直近に問い合わせを送った時刻
  unsigned long lastPollSentTime = 0;
  // 問い合わせに対する応答待ち中か
  bool isAwaitingReply = false;

  // CONTROL_SYNC の通し番号。サテライトが COM_CHECK_S_TO_L でそのまま返す
  uint8_t txSeq = 0;

  // 直近の COM_CHECK_S_TO_L が最新の txSeq への応答だったか (古い応答に続くフィードバックを捨てるため)
  bool isLastReplyValid = false;

  // 直近の往復時間 (ミリ秒)。WiFi 区間の遅延の目安
  unsigned long lastRoundTrip_ms = 0;

  // サテライトとの通信が確立しているか (途絶 → 回復の切り替わりで設定を送り直すため)
  bool isSatelliteLinkUp = false;

  // 起動後のゼロ点校正要求を出し終えたか (再接続のたびには出さない)
  bool hasRequestedZeroCalib = false;

  // 送信予約された単発コマンド。pollingTask で CONTROL_SYNC の前にまとめて送る
  bool hasPendingSensorConfig = false;
  bool hasPendingCalibCoeff = false;
  bool hasPendingZeroCalib = false;
  bool hasPendingDummyCurrent = false;
  float pendingCalibA = 0.0f;
  float pendingCalibB = 0.0f;
  float pendingDummyCurrent_mA = 0.0f;

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

  // 監視対象とするリミットスイッチのビットマスク (Bit5 = ch5)
  // 将来的に他の弁/リミットスイッチを増やす場合は、このマスクや判定ロジックを変更する
  constexpr uint8_t SAFETY_GATE_MASK = (1 << 5); // ch5のみを監視

  /// @brief 安全ゲート（必要なリミットスイッチがすべて押されているか）の判定
  /// @return true: 安全（すべて押されている）、false: 危険（どれかが押されていない）
  inline bool isSafetyGateMet()
  {
#if defined(DEMO_MODE) || defined(NO_LIMIT_SWITCH_GATE)
    // SatelliteNode を接続しない構成 (NO_LIMIT_SWITCH_GATE) またはデモモード:
    // リミットスイッチが無いため安全ゲートを成立扱いにする
    return true;
#else
    return (limitSwitchState & SAFETY_GATE_MASK) == SAFETY_GATE_MASK;
#endif
  }

  // RS485の送信を有効/無効化する関数
  void enableOutput();
  void disableOutput();

  // 受信が途切れるまで待つ (送信前に呼ぶ)
  void waitRxIdle();

  // パケット送信関数
  void pollingTask();
  uint8_t buildControlState(); // CONTROL_SYNC で送る8弁分の指令ビット
  void sendPendingCommands();  // 送信予約された単発コマンドを送る (送信有効中に呼ぶ)
  // 単発コマンドの送信予約 (実際の送信は pollingTask で行う)
  void requestSensorConfigSync();
  void requestSensorDummyCurrent(float current_mA);
  void requestSensorCalibCoeff(float a, float b);
  void requestSensorZeroCalib();
  void requestSatelliteResync(); // サテライトへ設定一式を送り直す
  void sendTelemetryForPython(); // PythonのGUI用にデータをシリアル出力する関数

  // パケット受信時のコールバック関数
  void onFeedbackSyncReceived(uint8_t state);
  void onPressureSyncReceived(float pressure);
  void onCurrentSyncReceived(float current_mA);
  void onComCheckReceived(uint8_t seq, bool isConfigured);
  void onComCheckFailed(); // タイムアウト時のフェールセーフ処理
  // SatelliteController 経由でリミットスイッチ状態を受信した際のコールバック
  void onLimitSwitchSyncReceived(uint8_t state);

#ifdef RS485_RX_LOG
  // 受信ログ用 (RS485_RX_LOG 有効時のみ)
  enum RxId : uint8_t
  {
    RX_FEEDBACK,
    RX_PRESSURE,
    RX_CURRENT,
    RX_COMCHECK_S,
    RX_LIMIT,
    RX_ID_COUNT
  };
  // Serial1 の生受信バイト数 (parse() 直前の available() の累計。概算値)
  uint32_t rxByteCount = 0;
  uint32_t rxPacketCount[RX_ID_COUNT] = {0};
  unsigned long rxLastPacketMs = 0;
  void rxLog(RxId id);
  void rxDebugTask(); // 1Hz で統計を出力
#endif

#ifdef RS485_ECHO_TEST
  // エコーテスト (RS485_ECHO_TEST 有効時のみ)
  void echoTest();
#endif

  // 通信状態が正常であることを示すランプ
  Output statusLamp(PIN_PK5); // LED_COM
} // namespace communication

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

#ifdef DEMO_MODE
  Serial.println();
  Serial.println("#########################################################");
  Serial.println("#   DEMO MODE ENABLED / デモモード有効                   #");
  Serial.println("#   SatelliteNode(リミットSW)/RocketNode 未接続でも      #");
  Serial.println("#   ランチ+サテライトのみでシーケンスを実行できます      #");
  Serial.println("#   ランチ<->サテライト間のフェールセーフは通常どおり有効 #");
  Serial.println("#   ※現場運用時は DEMO_MODE を無効化してビルドする事     #");
  Serial.println("#########################################################");
  Serial.println();
#endif
#ifdef NO_LIMIT_SWITCH_GATE
  Serial.println("[WARN] NO_LIMIT_SWITCH_GATE: limit switch safety gate is disabled (SatelliteNode not connected)");
#endif

#ifdef RS485_RX_LOG
  Serial.println("[DEBUG] RS485_RX_LOG enabled: [RX] / [RX-STAT] lines will be printed");
#endif
#ifdef RS485_ECHO_TEST
  Serial.println("[DEBUG] RS485_ECHO_TEST enabled: Serial1 echo only, polling/failsafe stopped");
#endif

  // LTC485 (RS485 - サテライトコントローラーとの通信用シリアル1)
  Serial1.begin(115200);
  // 初期状態でタイムアウト判定にならないよう、過去の時刻をセットしておく
  communication::preReceivedTime = millis();

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
  Tasks.add(&control::handleManualTask)->startFps(9); // 9Hzで実行
  // SatelliteController へポーリング要求を送信するタスク
  // 100Hz で呼び、送信してよいか (間隔経過・応答受信) は pollingTask の中で判定する
  Tasks.add(&communication::pollingTask)->startFps(100);
  // 通信がタイムアウトしていないか監視するタスク
  Tasks.add(&communication::onComCheckFailed)->startFps(2); // 2Hzで実行
  // PythonのGUI(ビジュアライザ)向けにシリアル通信でデータを送るタスク
  Tasks.add(&communication::sendTelemetryForPython)->startFps(10); // 10Hzで実行
#ifdef RS485_RX_LOG
  Tasks.add(&communication::rxDebugTask)->startFps(1); // 受信統計 1Hz
#endif

  // センサ設定・校正係数・ゼロ点校正は、サテライトとの通信が確立した時点で送る (onComCheckReceived)。
  // NPort の起動と WiFi の接続は Launch の起動より遅れるため、setup() で送っても届かない

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
  // SatelliteController が転送してくるリミットスイッチパケットを受信するコールバックを登録
  // (Node 非接続の現構成では届かない。Node を戻したときのために残している)
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

  // 起動完了の合図として音声11番を再生
  mp3_play(11);
}

void loop()
{
#ifdef RS485_ECHO_TEST
  // エコーテスト: 通常のパケット処理・タスク(ポーリング/フェールセーフ)は実行しない
  communication::echoTest();
  communication::accessLamp.update();
#else
#ifdef RS485_RX_LOG
  // parse() が受信バッファを消費する前に、溜まっているバイト数を加算する
  communication::rxByteCount += Serial1.available();
#endif

  // 受信したパケットを解析し、subscribeで登録したコールバックを呼び出す
  MsgPacketizer::parse();

  // 登録されたタスクのスケジュール管理と実行を行う
  Tasks.update();

  // ランプ(LED)の点滅(pulse/blink)状態を更新する
  communication::accessLamp.update();
  control::statusLamp.update();
  error::updateLamp();
#endif
}

#ifdef RS485_RX_LOG
/// @brief パケット受信ごとのログ出力と件数カウント
void communication::rxLog(RxId id)
{
  static const char *const names[RX_ID_COUNT] = {
      "FEEDBACK_SYNC", "PRESSURE_SYNC", "SENSOR_CURRENT_SYNC", "COM_CHECK_S_TO_L",
      "LIMIT_SWITCH_SYNC"};
  rxPacketCount[id]++;
  rxLastPacketMs = millis();
  Serial.print("[RX] ");
  Serial.print(names[id]);
  Serial.print(" t=");
  Serial.println(rxLastPacketMs);
}

/// @brief 1秒ごとに受信統計を出力する
void communication::rxDebugTask()
{
  static uint32_t prevBytes = 0;
  uint32_t delta = rxByteCount - prevBytes;
  prevBytes = rxByteCount;

  uint32_t totalPackets = 0;
  for (uint8_t i = 0; i < RX_ID_COUNT; i++)
    totalPackets += rxPacketCount[i];

  Serial.print("[RX-STAT] bytes=");
  Serial.print(rxByteCount);
  Serial.print(" (+");
  Serial.print(delta);
  Serial.print("/s) FB=");
  Serial.print(rxPacketCount[RX_FEEDBACK]);
  Serial.print(" PRS=");
  Serial.print(rxPacketCount[RX_PRESSURE]);
  Serial.print(" CUR=");
  Serial.print(rxPacketCount[RX_CURRENT]);
  Serial.print(" CC_S=");
  Serial.print(rxPacketCount[RX_COMCHECK_S]);
  Serial.print(" LIM=");
  Serial.print(rxPacketCount[RX_LIMIT]);
  Serial.print(" lastPkt=");
  if (totalPackets == 0)
  {
    Serial.println("none");
  }
  else
  {
    Serial.print(millis() - rxLastPacketMs);
    Serial.println("ms ago");
  }

  // 切り分けのヒント: バイトが全く来ない / バイトは来るがパケットにならない
  if (delta == 0)
  {
    Serial.println("[RX-STAT] no bytes received (check wiring / NPort serial settings / DE-RE)");
  }
  else if (totalPackets == 0 || millis() - rxLastPacketMs > 3000)
  {
    Serial.println("[RX-STAT] bytes arrive but no valid packet (check baud/format/packet index/framing)");
  }
}
#endif

#ifdef RS485_ECHO_TEST
/// @brief Serial1 で受信したバイトをそのまま送り返し、16進で Serial(USB) に表示する
/// 受信が途切れて約1ms経過したらまとめて送り返す (DEの切替を最小限にするため)
void communication::echoTest()
{
  static uint8_t buf[64];
  static uint8_t len = 0;
  static unsigned long lastRxUs = 0;
  static uint32_t totalBytes = 0;

  while (Serial1.available() && len < sizeof(buf))
  {
    buf[len++] = static_cast<uint8_t>(Serial1.read());
    lastRxUs = micros();
  }

  if (len > 0 && (len == sizeof(buf) || micros() - lastRxUs > 1000))
  {
    totalBytes += len;
    Serial.print("[ECHO] rx ");
    Serial.print(len);
    Serial.print("B (total ");
    Serial.print(totalBytes);
    Serial.print("):");
    for (uint8_t i = 0; i < len; i++)
    {
      Serial.print(' ');
      if (buf[i] < 0x10)
        Serial.print('0');
      Serial.print(buf[i], HEX);
    }
    Serial.println();

    communication::enableOutput();
    Serial1.write(buf, len);
    Serial1.flush();
    delayMicroseconds(200); // 最終バイトが線上に出切るまで DE を保持
    communication::disableOutput();
    len = 0;
  }
}
#endif

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

  // ハードウェア的な異常（過電流、過熱）があればエラー
  // HACK エラー
  error::set(error::POWER, isOverloadedInput || isOverloadedBus || isOverheated);

  // 操作ミスがあればエラー。DUMP弁を閉じる、スイッチを離す、またはエマスト中なら解除
  error::set(error::SEQ_START_MISTAKE, isSequenceStartMistake);
}

/// @brief Serial1 の受信が途切れるまで待つ (最大 RX_IDLE_MAX_WAIT_MS)
/// DE/RE 共用のため、送信を始めると受信中のバイトを取りこぼす。送信の直前に呼ぶ。
/// 待っている間に届いたバイトは受信バッファに残り、次の MsgPacketizer::parse() で処理される。
void communication::waitRxIdle()
{
  const unsigned long startTime = millis();
  int lastAvailable = Serial1.available();
  unsigned long lastRxTime_us = micros();

  while (millis() - startTime < communication::RX_IDLE_MAX_WAIT_MS)
  {
    const int available = Serial1.available();
    if (available != lastAvailable)
    {
      // 新しいバイトが届いたので、無通信時間の計測をやり直す
      lastAvailable = available;
      lastRxTime_us = micros();
    }
    else if (micros() - lastRxTime_us >= communication::RX_IDLE_US)
    {
      return;
    }
  }
}

/// @brief SatelliteController へ問い合わせ (送信予約された単発コマンド + CONTROL_SYNC) を送るポーリングタスク
/// 100Hz で呼ばれ、次の条件をすべて満たしたときだけ送信する。
///   - 前回の送信から POLL_SLOT_MS 以上経過している
///   - 前回の問い合わせへの応答を受信済み、または REPLY_TIMEOUT_MS を過ぎた
/// SatelliteNode / RocketNode は接続しないため問い合わせない
void communication::pollingTask()
{
  const unsigned long elapsed = millis() - communication::lastPollSentTime;
  if (elapsed < communication::POLL_SLOT_MS)
  {
    return;
  }
  if (communication::isAwaitingReply && elapsed < communication::REPLY_TIMEOUT_MS)
  {
    return;
  }

  // 遅れて届いた応答の受信中に送信を始めないよう、受信が途切れるまで待つ
  communication::waitRxIdle();

  const uint8_t state = communication::buildControlState();

  communication::txSeq++;
  // 新しい seq への応答を受け取るまで、届いたフィードバックは採用しない
  communication::isLastReplyValid = false;

  communication::enableOutput();
  communication::sendPendingCommands();
  // CONTROL_SYNC は必ずバーストの最後に送る。
  // サテライトはこれを受け取ると返信を始めるため、後ろにパケットがあると返信と衝突する
  MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::CONTROL_SYNC), state, communication::txSeq);
  Serial1.flush();
  communication::disableOutput();

  communication::lastPollSentTime = millis();
  communication::isAwaitingReply = true;
}

/// @brief CONTROL_SYNC で送る8弁分の指令ビットを作る (bit0:SHIFT ... bit7:PURGE)
/// キルスイッチ押下後の電源断待ち中は全弁OFFを返し、サテライトを電源断より先に安全状態にする
uint8_t communication::buildControlState()
{
  if (power::isShuttingDown)
  {
    return 0;
  }
  return (control::shift.isRaised() << 0) | (control::fill.isRaised() << 1) |
         (control::dump.isRaised() << 2) | (control::oxygen.isRaised() << 3) |
         (control::igniter.isRaised() << 4) | (control::open.isRaised() << 5) |
         (control::close.isRaised() << 6) | (control::purge.isRaised() << 7);
}

/// @brief 送信予約された単発コマンドを送る (pollingTask から、送信有効中に呼ぶ)
void communication::sendPendingCommands()
{
  if (communication::hasPendingSensorConfig)
  {
    MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::SENSOR_CONFIG_SYNC), 10.0f); // Default 10.0 MPa
    communication::hasPendingSensorConfig = false;
  }
  if (communication::hasPendingCalibCoeff)
  {
    MsgPacketizer::send(
        Serial1,
        static_cast<uint8_t>(communication::Packet::SENSOR_CALIB_COEFF_SYNC),
        communication::pendingCalibA, communication::pendingCalibB);
    communication::hasPendingCalibCoeff = false;
  }
  if (communication::hasPendingDummyCurrent)
  {
    MsgPacketizer::send(
        Serial1,
        static_cast<uint8_t>(communication::Packet::SENSOR_DUMMY_CURRENT_SYNC),
        communication::pendingDummyCurrent_mA);
    communication::hasPendingDummyCurrent = false;
  }
  if (communication::hasPendingZeroCalib)
  {
    Serial.println(">>> Requesting Remote Zero-Point Calibration...");
    MsgPacketizer::send(
        Serial1,
        static_cast<uint8_t>(communication::Packet::SENSOR_ZERO_CALIB_REQ));
    communication::hasPendingZeroCalib = false;
  }
}

/// @brief センサの基本設定（フルスケール圧など）の送信を予約する
void communication::requestSensorConfigSync() { communication::hasPendingSensorConfig = true; }

/// @brief シミュレーション用のダミー電流値の送信を予約する (0.0 でダミーモード解除)
void communication::requestSensorDummyCurrent(float current_mA)
{
  communication::pendingDummyCurrent_mA = current_mA;
  communication::hasPendingDummyCurrent = true;
}

/**
 * @brief センサ校正係数の送信を予約する
 * @param a 傾き (Pressure = a * Current + b)
 * @param b 切片
 */
void communication::requestSensorCalibCoeff(float a, float b)
{
  communication::pendingCalibA = a;
  communication::pendingCalibB = b;
  communication::hasPendingCalibCoeff = true;
}

/**
 * @brief ゼロ点校正（大気圧環境でのキャリブレーション）の実行リクエストの送信を予約する
 */
void communication::requestSensorZeroCalib() { communication::hasPendingZeroCalib = true; }

/// @brief サテライトへ設定一式 (フルスケール, 校正係数, ダミー電流解除) の送信を予約する
/// 通信の確立・回復時と、サテライトが未設定 (再起動直後) と応答してきたときに呼ぶ
void communication::requestSatelliteResync()
{
  communication::requestSensorConfigSync();
  communication::requestSensorCalibCoeff(n2o::CALIB_SLOPE_A, n2o::CALIB_INTERCEPT_B);
  communication::requestSensorDummyCurrent(0.0f);
}

// sendComCheck, sendComCheckNode was replaced by pollingTask

/// @brief SatelliteController から転送されたリミットスイッチ状態を受信するコールバック
/// SatelliteNode (ch0〜ch5) → SatelliteController → (LIMIT_SWITCH_SYNCパケット) → LaunchController
/// 内部変数 limitSwitchState に保存し、isCh0Pressed() 〜 isCh5Pressed() で参照する。
void communication::onLimitSwitchSyncReceived(uint8_t state)
{
  RX_LOG(RX_LIMIT);
  // ソフトウェア・デバウンス（チャタリング防止）処理
  // コメントアウトすることでデバウンスを無効化できます
#define DEBOUNCE_LIMIT_SWITCH

#ifdef DEBOUNCE_LIMIT_SWITCH
  static uint8_t lastRawState = 0;
  static uint8_t stableState = 0;
  static uint8_t confirmCount = 0;
  constexpr uint8_t REQUIRED_CONFIRMS = 2; // 10Hz周期で2回連続一致(約200ms)で状態確定

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
  RX_LOG(RX_FEEDBACK);
  // 最新の CONTROL_SYNC への応答でなければ (遅れて届いた古い応答など) 反映しない
  if (!communication::isLastReplyValid)
  {
    return;
  }
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

    // 実際に送っている指令 (電源断待ち中は全弁OFF) と比較する
    const uint8_t cmd = communication::buildControlState();
    bool hasFault = false;
    hasFault |= checkFault((cmd & (1 << 1)) != 0, (state & (1 << 1)) != 0,
                           solenoid::fillFaultCount, "FILL");
    hasFault |= checkFault((cmd & (1 << 2)) != 0, (state & (1 << 2)) != 0,
                           solenoid::dumpFaultCount, "DUMP");
    hasFault |= checkFault((cmd & (1 << 3)) != 0, (state & (1 << 3)) != 0,
                           solenoid::oxygenFaultCount, "OXYGEN");
    hasFault |= checkFault((cmd & (1 << 7)) != 0, (state & (1 << 7)) != 0,
                           solenoid::purgeFaultCount, "PURGE");
    hasFault |= checkFault((cmd & (1 << 5)) != 0, (state & (1 << 5)) != 0,
                           solenoid::openFaultCount, "OPEN");
    hasFault |= checkFault((cmd & (1 << 6)) != 0, (state & (1 << 6)) != 0,
                           solenoid::closeFaultCount, "CLOSE");

    // 故障判定がすべてクリアされた（またはデバウンス回数未満）場合はエラー解除
    error::set(error::SOLENOID, hasFault);
  }
  else
  {
    // Armed解除中はカウンタをリセット
    solenoid::fillFaultCount = 0;
    solenoid::dumpFaultCount = 0;
    solenoid::oxygenFaultCount = 0;
    solenoid::purgeFaultCount = 0;
    solenoid::openFaultCount = 0;
    solenoid::closeFaultCount = 0;
    error::set(error::SOLENOID, false);
  }
}

/// @brief 機体から算出された圧力値を受信した際のコールバック
void communication::onPressureSyncReceived(float pressure)
{
  RX_LOG(RX_PRESSURE);
  if (!communication::isLastReplyValid)
  {
    return;
  }
  // 7セグメントディスプレイに圧力を表示
  n2o::tm1637.displayNumber(pressure);
}

/**
 * @brief 機体から受信した生の電流値(mA)をシリアル出力
 */
void communication::onCurrentSyncReceived(float current_mA)
{
  RX_LOG(RX_CURRENT);
  if (!communication::isLastReplyValid)
  {
    return;
  }
  Serial.print(">VESIM10 Current: ");
  Serial.println(current_mA);
}

/// @brief 機体から通信生存確認（ハートビート）を受信した際のコールバック
/// サテライトは返信の先頭でこれを送るため、続く FEEDBACK/PRESSURE/CURRENT の採否はここで決まる
/// @param seq 応答元の CONTROL_SYNC の通し番号 (エコー)
/// @param isConfigured サテライトがセンサ設定・校正係数を受信済みか (false なら再起動直後)
void communication::onComCheckReceived(uint8_t seq, bool isConfigured)
{
  RX_LOG(RX_COMCHECK_S);

  // 最新の CONTROL_SYNC への応答でなければ、生存確認に使わない
  // (WiFi 再接続時に NPort から遅れてまとめて届いた古い応答など)
  communication::isLastReplyValid = (seq == communication::txSeq);
  if (!communication::isLastReplyValid)
  {
    Serial.print("[COM] stale reply ignored: seq=");
    Serial.print(seq);
    Serial.print(" expected=");
    Serial.println(communication::txSeq);
    return;
  }

  communication::isAwaitingReply = false;
  communication::lastRoundTrip_ms = millis() - communication::lastPollSentTime;
  // 通信受信時刻を更新
  communication::preReceivedTime = millis();

  if (!communication::isSatelliteLinkUp)
  {
    // 途絶 → 回復 (または起動後の初回接続)。設定一式を送り直す
    communication::isSatelliteLinkUp = true;
    Serial.print("[COM] SatelliteController link up (RTT=");
    Serial.print(communication::lastRoundTrip_ms);
    Serial.println("ms)");
    communication::requestSatelliteResync();

    // ゼロ点校正は起動後の最初の接続時のみ (再接続のたびには行わない)
    if (!communication::hasRequestedZeroCalib)
    {
      communication::requestSensorZeroCalib();
      communication::hasRequestedZeroCalib = true;
    }
  }
  else if (!isConfigured && !communication::hasPendingCalibCoeff)
  {
    // 通信は途切れていないが、サテライトだけが再起動して設定を失っている
    Serial.println("[COM] SatelliteController is not configured (rebooted?): resending sensor settings");
    Serial.println("[COM] note: zero-point calibration is NOT re-run automatically");
    communication::requestSatelliteResync();
  }

  // タイムアウト以内であれば通信正常ランプを点灯
  communication::statusLamp.on();
  error::set(error::COM_TIMEOUT, false);
}

/// @brief サテライトとの通信が途絶した（タイムアウト）場合に実行されるフェールセーフタスク
void communication::onComCheckFailed()
{
  bool controllerTimeout = (millis() - communication::preReceivedTime > communication::timeout);

  if (controllerTimeout)
  {
    static unsigned long lastLogTime = 0;
    if (millis() - lastLogTime > 2000) // 2秒おきに出力
    {
      Serial.println("[COM ERROR] SatelliteController timed out!");
      lastLogTime = millis();
    }
  }

  if (controllerTimeout && communication::isSatelliteLinkUp)
  {
    // 回復したときに設定一式を送り直すため、途絶を記録しておく
    communication::isSatelliteLinkUp = false;
    Serial.println("[COM] SatelliteController link down");
  }

  if (controllerTimeout)
  {
    communication::statusLamp.off();
    error::set(error::COM_TIMEOUT, true);

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
  uint8_t cmd_state = communication::buildControlState();

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
  if (power::killButton.isHigh() && !power::isShuttingDown)
  {
    mp3_play(12); // 電源を落とすときの音声(アラート)を再生

    // 電源断待ちに入る。この間も通信は続け、pollingTask は全弁OFFを送る
    // (以前は delay で 3.5 秒止めていたため、サテライトはタイムアウト(2秒)まで直前の指令を保持していた)
    power::isShuttingDown = true;
    power::shutdownStartTime = millis();

    if (sequence::emergencyStopSequenceIsActive ||
        sequence::fillSequenceIsActive || sequence::ignitionSequenceIsActive)
    {
      sequence::peacefulStop();
    }
  }

  if (power::isShuttingDown)
  {
    // システム電源を落とす前に、電源ランプを 500ms ごとに点滅させる (他の操作は受け付けない)
    const unsigned long elapsed = millis() - power::shutdownStartTime;
    power::powerLamp.set((elapsed / 500) % 2 == 1);

    if (elapsed >= power::SHUTDOWN_DELAY_MS)
    {
      // 最後にロードスイッチをオフにして物理的に電源供給を遮断する
      power::loadSwitch.off();
    }
    return;
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

    // 操作エラー (安全ゲート不成立など) は Armed を OFF にすることで解除する
    error::set(error::OPERATION, false);
    return;
  }

  // ★ リミットスイッチによる安全ゲートの常時監視 (シーケンス中)
  // シーケンス実行中に安全状態が崩れたら、強制的にシーケンスを止める
  if (sequence::fillSequenceIsActive || sequence::ignitionSequenceIsActive)
  {
    if (!communication::isSafetyGateMet())
    {
      Serial.println("[GATE] Limit switch released during sequence! Stopping sequence.");
      sequence::peacefulStop();
      error::set(error::OPERATION, true);
    }
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
  // ロック中もこの後の手動弁の読み取りは続ける (以前は return していたため手動弁が反映されなかった)
  const bool isDumpLocked = !control::dump.isManualRaised() &&
                            !sequence::emergencyStopSequenceIsActive;

  // シーケンス開始スイッチが押された場合の処理
  if (control::sequenceStart.isManualRaised())
  {
    // スイッチが「新しく押された瞬間」だけ処理を進める（長押しによる連打を防止）
    // ロック中もカウントは進めるので、押したまま DUMP を閉じても開始せず、押し直しが必要
    if (sequence::sequenceStartRiseCount == 0 && isDumpLocked)
    {
      mp3_play(13); // エラー音を再生
    }
    else if (sequence::sequenceStartRiseCount == 0)
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
      // 点火シーケンス完了後も ignitionSequenceIsActive は残るため、完了後に押すとここに来る。
      // これがリセット操作になる (自動弁をすべて解除し、OPEN も閉じる)。誤って再充填が始まらないよう意図的にフラグを残している
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

  // FILLのみリミットスイッチによる安全ゲートで制限しようと思ったが，
  // FILL弁からN2OをDUMPしたいときに不都合なためコメントアウト
  ///////////////////////////////////////////////////////////////////////////
  // static bool lastFillBlocked = false;
  // if (!communication::isSafetyGateMet())
  // {
  //   if (control::fill.isManualRaised())
  //   {
  //     control::fill.setManualOff();
  //     if (!lastFillBlocked)
  //     {
  //       Serial.println("[GATE] Manual FILL blocked: limit switch safety gate not met.");
  //       lastFillBlocked = true;
  //       error::statusLamp.on(); // エラーランプ点灯
  //     }
  //   }
  // }
  // else
  // {
  //   if (lastFillBlocked)
  //   {
  //     error::statusLamp.off();
  //     lastFillBlocked = false;
  //   }
  // }
  ///////////////////////////////////////////////////////////////////////////

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
  Tasks[control::PURGE_START]->stop();
  Tasks[control::PURGE_STOP]->stop();

  // 電磁弁を安全状態(主流路CLOSE・PURGE ON など)にセット
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
    error::set(error::OPERATION, true);
    return;
  }

  // 通信状態が正常でない（通信切断中）場合はシーケンスに進めない
  if (!communication::statusLamp.isHigh())
  {
    sequence::peacefulStop();
    return;
  }

  // ★ リミットスイッチによる安全ゲート
  // 安全ゲート（必要なリミットスイッチ）が満たされていない場合は充填シーケンスを開始しない
  if (!communication::isSafetyGateMet())
  {
    Serial.println("[GATE] sequence::fill() blocked: limit switch safety gate not met.");
    error::set(error::OPERATION, true); // 未押下をエラーランプで通知 (Armed OFF で解除)
    sequence::peacefulStop();           // シーケンスをキャンセル（穏便ストップ）
    mp3_play(13);                       // エラー音再生
    return;
  }

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

  // ★ リミットスイッチによる安全ゲート
  // 安全ゲート（必要なリミットスイッチ）が満たされていない場合は点火シーケンスに進めない
  if (!communication::isSafetyGateMet())
  {
    Serial.println("[GATE] sequence::ignition() blocked: limit switch safety gate not met.");
    error::set(error::OPERATION, true); // 未押下をエラーランプで通知 (Armed OFF で解除)
    sequence::peacefulStop();           // シーケンスをキャンセル（穏便ストップ）
    mp3_play(13);                       // エラー音再生
    return;
  }

  // フラグ更新
  sequence::ignitionSequenceIsActive = true;
  sequence::canConfirm = false;

  control::sequenceStart.setAutomaticOn();
  mp3_play(4); // 0104_ignitionSequenceStart

  // 「充填が確認されました．点火します．5秒前...」←これが4.5秒くらいかかる

  // 酸素(O2)バルブを開く (4.5秒後)
  Tasks[control::OXYGEN_START]->startOnceAfterSec(4.5); // THR-406K 用に変更
  // 点火器(IGNITER)をオンにする (5.5秒後)
  Tasks[control::IGNITER_START]->startOnceAfterSec(5.5); // THR-406K 用に変更

  // 充填(FILL)バルブを閉じる (9.5秒後: カウントダウン終了付近)
  Tasks[control::FILL_STOP]->startOnceAfterSec(9.5);
  // 主流路(OPEN)バルブを開く (9.5秒後)
  Tasks[control::OPEN_START]->startOnceAfterSec(9.5);

  // 酸素(O2)バルブと点火器(IGNITER)をオフにする (10.0秒後)
  Tasks[control::OXYGEN_STOP]->startOnceAfterSec(10.0);
  Tasks[control::IGNITER_STOP]->startOnceAfterSec(10.0);

  // 燃焼終了後、パージ(PURGE)バルブを開けて配管内をパージする
  Tasks[control::PURGE_START]->startOnceAfterSec(15.5);
  // パージを終了する (20.5秒後)
  Tasks[control::PURGE_STOP]->startOnceAfterSec(20.5);

  // ※ シーケンス完了後も ignitionSequenceIsActive は true のまま (OPEN も開いたまま)。
  //   次にシーケンス開始スイッチを押すと peacefulStop() でリセットされる (handleManualTask 参照)
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

  // DUMPは自動・手動とも(OFF = 開)にする
  // ※ シーケンス開始前に DUMP を閉じておく必要があるため、ここで ON にはしない
  control::dump.setAutomaticOff();
  control::dump.setManualOff();

  // パージをONにして消火する
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
