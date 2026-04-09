/**
 * @file sub1.cpp
 * @brief RS485バス 中継ノード (Nucleo F446RE + ADM2582E)
 *
 * バス構成:
 *   [main (F756ZG)] ---RS485--- [sub1 (F446RE)] ---RS485--- [sub (F756ZG)]
 *                                    ↑
 *                                 このファイル
 *
 * 役割:
 *   - mainからのコマンドパケット(index=0x01)を受信
 *   - 自ノードのテストデータをmainへ返信(index=0x11)
 *   - mainからのパケットをsubへ転送(index=0x21)
 *   - TaskManagerで定期的に自発的なデータ送信も行う
 *
 * ピン配置 (Nucleo F446RE):
 *   RS485 UART : USART2 (PA_3=RX, PA_2=TX)
 *   RS485 DE   : PA_10 (HIGH=送信, LOW=受信)
 *   Debug UART : Serial (USB/ST-LINK)
 *
 * パケットIndex一覧:
 *   0x01 : main → 全ノード  コマンド/ポーリング要求
 *   0x11 : sub1 → main     sub1のステータス返信
 *   0x21 : sub1 → sub      転送パケット
 *   0x31 : sub  → main     subのステータス (本ファイルでは中継のみ)
 */

#include <Arduino.h>
#include <MsgPacketizer.h>
#include <TaskManager.h>

// ============================================================
// RS485 UART インスタンス (USART2: PA_3=RX, PA_2=TX)
// STM32duino では Serial がこのピンにマップされる場合があるため明示宣言
// ============================================================
HardwareSerial SerialRS485(PC11, PC10); // RX, TX

// ============================================================
// ピン定義
// ============================================================
#define RS485_SERIAL SerialRS485 // USART2: PA_3(RX), PA_2(TX)
#define RS485_DE_PIN PA10        // ADM2582E DE (HIGH=TX, LOW=RX)
#define DEBUG_SERIAL Serial      // USB経由デバッグ出力

// ============================================================
// 通信設定
// ============================================================
static constexpr uint32_t RS485_BAUD = 115200;
static constexpr uint32_t DEBUG_BAUD = 115200;

// パケットインデックス
static constexpr uint8_t IDX_CMD_FROM_MAIN = 0x01;   // main → 全ノード
static constexpr uint8_t IDX_STATUS_SUB1 = 0x11;     // sub1 → main
static constexpr uint8_t IDX_FORWARD_TO_SUB = 0x21;  // sub1 → sub (転送)
static constexpr uint8_t IDX_STATUS_FROM_SUB = 0x31; // sub → main (中継)

// ノードID
static constexpr uint8_t NODE_ID_MAIN = 0x01;
static constexpr uint8_t NODE_ID_SUB1 = 0x02;
static constexpr uint8_t NODE_ID_SUB = 0x03;

// ============================================================
// データ構造 (MsgPack シリアライズ対象)
// ============================================================

// mainから受信するコマンドパケット
struct CmdPacket {
  uint8_t target_id; // 宛先ノードID (0xFF = ブロードキャスト)
  uint8_t cmd;       // コマンド番号
  uint32_t seq;      // シーケンス番号
  MSGPACK_DEFINE(target_id, cmd, seq);
};

// sub1がmainへ返すステータスパケット
struct StatusPacket {
  uint8_t node_id;    // 自ノードID
  uint32_t seq;       // 対応するシーケンス番号
  float test_value_a; // テストデータA (例: 電圧)
  float test_value_b; // テストデータB (例: 電流)
  uint32_t uptime_ms; // 起動からの経過時間[ms]
  MSGPACK_DEFINE(node_id, seq, test_value_a, test_value_b, uptime_ms);
};

// subへ転送するパケット (mainのコマンドをそのまま渡す)
struct ForwardPacket {
  uint8_t from_id;   // 転送元ノードID
  uint8_t target_id; // 転送先ノードID
  uint8_t cmd;
  uint32_t seq;
  MSGPACK_DEFINE(from_id, target_id, cmd, seq);
};

// ============================================================
// 状態変数
// ============================================================
static uint32_t g_last_seq = 0;
static uint32_t g_recv_count = 0;
static bool g_cmd_received = false;
static CmdPacket g_last_cmd = {};

// テストデータ (実際のセンサ値の代わりに疑似データを生成)
static float g_test_a = 0.0f;
static float g_test_b = 0.0f;

// ============================================================
// RS485送受信切替ヘルパー
// ============================================================
inline void rs485TxMode() { digitalWrite(RS485_DE_PIN, HIGH); }

inline void rs485RxMode() {
  RS485_SERIAL.flush();
  delayMicroseconds(100);
  digitalWrite(RS485_DE_PIN, LOW);
}

// ============================================================
// RS485送信ラッパー
//   MsgPacketizer::send() はデフォルトで flush を行わないため
//   DE制御をこのラッパー内で管理する
// ============================================================
template <typename... Args> void rs485Send(uint8_t index, Args &&...args) {
  rs485TxMode();
  MsgPacketizer::send(RS485_SERIAL, index, args...);
  rs485RxMode();
}

// ============================================================
// タスク関数
// ============================================================

/**
 * @brief 定期ステータス送信タスク
 *
 * mainからのポーリングに依存せず、定期的に自ノードの状態を送信する。
 * 疑似データとして時間変化するサイン波を使用。
 */
void taskSendStatus() {
  // 疑似センサデータ生成 (実装時はセンサ読み取りに置き換える)
  float t = millis() / 1000.0f;
  g_test_a = 3.3f * (0.5f + 0.5f * sinf(t));          // 0.0〜3.3V の疑似電圧
  g_test_b = 100.0f * (0.5f + 0.5f * cosf(t * 0.5f)); // 0〜100 の疑似値

  StatusPacket pkt;
  pkt.node_id = NODE_ID_SUB1;
  pkt.seq = g_last_seq;
  pkt.test_value_a = g_test_a;
  pkt.test_value_b = g_test_b;
  pkt.uptime_ms = millis();

  rs485Send(IDX_STATUS_SUB1, pkt);

  DEBUG_SERIAL.print("[TX] StatusPacket seq=");
  DEBUG_SERIAL.print(pkt.seq);
  DEBUG_SERIAL.print(" A=");
  DEBUG_SERIAL.print(pkt.test_value_a, 3);
  DEBUG_SERIAL.print(" B=");
  DEBUG_SERIAL.println(pkt.test_value_b, 3);
}

/**
 * @brief mainから受信したコマンドをsubへ転送するタスク
 *
 * コマンド受信フラグが立っている場合のみ転送を行う。
 * フラグはコールバック内でセットされる。
 */
void taskForwardToSub() {
  if (!g_cmd_received)
    return;
  g_cmd_received = false;

  // target がsub (0x03) またはブロードキャスト (0xFF) の場合のみ転送
  if (g_last_cmd.target_id != NODE_ID_SUB && g_last_cmd.target_id != 0xFF) {
    return;
  }

  ForwardPacket fwd;
  fwd.from_id = NODE_ID_SUB1;
  fwd.target_id = g_last_cmd.target_id;
  fwd.cmd = g_last_cmd.cmd;
  fwd.seq = g_last_cmd.seq;

  rs485Send(IDX_FORWARD_TO_SUB, fwd);

  DEBUG_SERIAL.print("[FWD] Forward to sub: cmd=0x");
  DEBUG_SERIAL.print(fwd.cmd, HEX);
  DEBUG_SERIAL.print(" seq=");
  DEBUG_SERIAL.println(fwd.seq);
}

/**
 * @brief MsgPacketizerのパケット受信処理タスク
 *
 * loop()内で呼ぶ代わりにTaskManagerで管理することで
 * 他のタスクとのスケジューリングを統一する。
 */
void taskUpdateReceiver() { MsgPacketizer::update(); }

// ============================================================
// セットアップ
// ============================================================
void setup() {
  // デバッグシリアル初期化
  DEBUG_SERIAL.begin(DEBUG_BAUD);
  delay(500);
  DEBUG_SERIAL.println("=================================");
  DEBUG_SERIAL.println(" sub1 node - RS485 test (F446RE)");
  DEBUG_SERIAL.println("=================================");

  // RS485 DE/RE ピン初期化 (受信モードから開始)
  pinMode(RS485_DE_PIN, OUTPUT);
  rs485RxMode();

  // RS485 UART初期化
  RS485_SERIAL.begin(RS485_BAUD);
  DEBUG_SERIAL.print("RS485 UART started at ");
  DEBUG_SERIAL.print(RS485_BAUD);
  DEBUG_SERIAL.println(" bps");

  // -------------------------------------------------------
  // MsgPacketizer 受信コールバック登録
  // -------------------------------------------------------

  // (1) mainからのコマンドを受信
  MsgPacketizer::subscribe(
      RS485_SERIAL, IDX_CMD_FROM_MAIN, [](const CmdPacket &pkt) {
        ++g_recv_count;
        g_last_seq = pkt.seq;

        DEBUG_SERIAL.print("[RX] CmdPacket target=0x");
        DEBUG_SERIAL.print(pkt.target_id, HEX);
        DEBUG_SERIAL.print(" cmd=0x");
        DEBUG_SERIAL.print(pkt.cmd, HEX);
        DEBUG_SERIAL.print(" seq=");
        DEBUG_SERIAL.println(pkt.seq);

        // 自ノード宛またはブロードキャストの場合のみ処理
        if (pkt.target_id == NODE_ID_SUB1 || pkt.target_id == 0xFF) {
          // 即座にステータスを返信
          float t = millis() / 1000.0f;
          StatusPacket resp;
          resp.node_id = NODE_ID_SUB1;
          resp.seq = pkt.seq;
          resp.test_value_a = 3.3f * (0.5f + 0.5f * sinf(t));
          resp.test_value_b = 100.0f * (0.5f + 0.5f * cosf(t * 0.5f));
          resp.uptime_ms = millis();

          rs485Send(IDX_STATUS_SUB1, resp);

          DEBUG_SERIAL.print("[TX] Immediate reply seq=");
          DEBUG_SERIAL.println(resp.seq);
        }

        // subへの転送フラグをセット (taskForwardToSubで処理)
        g_last_cmd = pkt;
        g_cmd_received = true;
      });

  // (2) subからのステータスを受信してmainへ中継
  //     sub1はバスの途中にいるため、subのステータスをmainへ再送する
  MsgPacketizer::subscribe(
      RS485_SERIAL, IDX_STATUS_FROM_SUB, [](const StatusPacket &pkt) {
        // subのステータスをそのままmainへ転送 (index維持)
        DEBUG_SERIAL.print("[RELAY] Got status from sub, seq=");
        DEBUG_SERIAL.println(pkt.seq);
        rs485Send(IDX_STATUS_FROM_SUB, pkt);
      });

  // -------------------------------------------------------
  // TaskManager タスク登録
  // -------------------------------------------------------

  // MsgPacketizer受信処理: できる限り高頻度で実行
  Tasks.add(taskUpdateReceiver)->startFps(200);

  // mainへのステータス定期送信: 2Hz (500ms間隔)
  Tasks.add(taskSendStatus)->startFps(2);

  // subへの転送: 受信チェックを高頻度で
  Tasks.add(taskForwardToSub)->startFps(100);

  DEBUG_SERIAL.println("TaskManager tasks registered.");
  DEBUG_SERIAL.println("Waiting for packets on RS485 bus...");
  DEBUG_SERIAL.println("---------------------------------");
}

// ============================================================
// メインループ
// ============================================================
void loop() { Tasks.update(); }
