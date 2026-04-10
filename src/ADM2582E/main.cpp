/**
 * @file main.cpp
 * @brief RS485バス マスターノード (Nucleo F756ZG + ADM2582E)
 *
 * バス構成:
 *   [main (F756ZG)] ---RS485--- [sub1 (F446RE)] ---RS485--- [sub (F756ZG)]
 *    ↑
 *  このファイル
 *
 * 役割:
 *   - コマンドパケット(index=0x01)を定期的に全ノードへブロードキャスト
 *   - sub1からのステータス(index=0x11)を受信・表示
 *   - sub1経由でsubから中継されるステータス(index=0x31)を受信・表示
 *   - Serialコマンドで手動送信も可能 (s: 送信, h: ヘルプ)
 *
 * ピン配置 (Nucleo F756ZG):
 *   RS485 UART : USART6 (PG9=RX, PG14=TX)
 *   RS485 DE   : PF15 (HIGH=送信, LOW=受信)
 *   Debug UART : Serial (USB/ST-LINK)
 *
 * パケットIndex一覧:
 *   0x01 : main → 全ノード  コマンド/ポーリング要求
 *   0x11 : sub1 → main     sub1のステータス返信
 *   0x21 : sub1 → sub      転送パケット (mainは傍受のみ)
 *   0x31 : sub  → main     subのステータス (sub1経由で中継)
 */

#include <Arduino.h>
#include <MsgPacketizer.h>
#include <TaskManager.h>

// ============================================================
// RS485 UART インスタンス (USART6: PG_9=RX, PG_14=TX)
// ============================================================
HardwareSerial SerialRS485(PG9, PG14); // RX, TX

// ============================================================
// ピン定義
// ============================================================
#define RS485_SERIAL SerialRS485 // USART6: PG_9(RX), PG_14(TX)
#define RS485_DE_PIN PF15        // ADM2582E DE (HIGH=TX, LOW=RX)
#define DEBUG_SERIAL Serial      // USB経由デバッグ出力

// ============================================================
// 通信設定
// ============================================================
static constexpr uint32_t RS485_BAUD = 115200;
static constexpr uint32_t DEBUG_BAUD = 115200;

// パケットインデックス
static constexpr uint8_t IDX_CMD_FROM_MAIN = 0x01;
static constexpr uint8_t IDX_STATUS_SUB1 = 0x11;
static constexpr uint8_t IDX_STATUS_FROM_SUB = 0x31;

// ノードID
static constexpr uint8_t NODE_ID_MAIN = 0x01;
static constexpr uint8_t NODE_ID_SUB1 = 0x02;
static constexpr uint8_t NODE_ID_SUB = 0x03;

// ============================================================
// データ構造
// ============================================================

// mainが送信するコマンドパケット
struct CmdPacket
{
  uint8_t target_id; // 0xFF = ブロードキャスト
  uint8_t cmd;
  uint32_t seq;
  MSGPACK_DEFINE(target_id, cmd, seq);
};

// sub1/subから受け取るステータスパケット
struct StatusPacket
{
  uint8_t node_id;
  uint32_t seq;
  float test_value_a;
  float test_value_b;
  uint32_t uptime_ms;
  MSGPACK_DEFINE(node_id, seq, test_value_a, test_value_b, uptime_ms);
};

// ============================================================
// 状態変数
// ============================================================
static uint32_t g_seq = 0;
static uint32_t g_send_count = 0;

// 受信した最新ステータス
static StatusPacket g_status_sub1 = {};
static StatusPacket g_status_sub = {};
static bool g_sub1_updated = false;
static bool g_sub_updated = false;

// ============================================================
// RS485送受信切替ヘルパー
// ============================================================
inline void rs485TxMode() { digitalWrite(RS485_DE_PIN, HIGH); }

inline void rs485RxMode()
{
  RS485_SERIAL.flush(); // TX FIFO空が待ち
  delayMicroseconds(
      100); // 最終ビットの物理送出完了を待つ (115200bps: 1byte≈1/115200≥8.7us)
  digitalWrite(RS485_DE_PIN, LOW);
}

template <typename... Args>
void rs485Send(uint8_t index, Args &&...args)
{
  rs485TxMode();
  MsgPacketizer::send(RS485_SERIAL, index, args...);
  rs485RxMode();
}

// ============================================================
// タスク関数
// ============================================================

/**
 * @brief 定期ポーリング送信タスク (1Hz)
 *
 * 全ノードへブロードキャストコマンドを送信する。
 * cmd=0x00 はポーリング要求 (ステータス返信を要求)。
 */
void taskPollAll()
{
  CmdPacket pkt;
  pkt.target_id = 0xFF; // ブロードキャスト
  pkt.cmd = 0x00;       // ポーリング要求
  pkt.seq = ++g_seq;

  rs485Send(IDX_CMD_FROM_MAIN, pkt);
  ++g_send_count;

  DEBUG_SERIAL.print("[TX] Poll #");
  DEBUG_SERIAL.print(g_seq);
  DEBUG_SERIAL.print("  total_sent=");
  DEBUG_SERIAL.println(g_send_count);
}

/**
 * @brief 受信ステータス表示タスク (2Hz)
 *
 * 受信フラグが立っているノードの最新データを表示する。
 */
void taskPrintStatus()
{
  if (g_sub1_updated)
  {
    g_sub1_updated = false;
    DEBUG_SERIAL.println("--- sub1 status ---");
    DEBUG_SERIAL.print("  seq=");
    DEBUG_SERIAL.println(g_status_sub1.seq);
    DEBUG_SERIAL.print("  val_a=");
    DEBUG_SERIAL.print(g_status_sub1.test_value_a, 3);
    DEBUG_SERIAL.print("  val_b=");
    DEBUG_SERIAL.println(g_status_sub1.test_value_b, 3);
    DEBUG_SERIAL.print("  uptime=");
    DEBUG_SERIAL.print(g_status_sub1.uptime_ms);
    DEBUG_SERIAL.println(" ms");
  }
  if (g_sub_updated)
  {
    g_sub_updated = false;
    DEBUG_SERIAL.println("--- sub status ---");
    DEBUG_SERIAL.print("  seq=");
    DEBUG_SERIAL.println(g_status_sub.seq);
    DEBUG_SERIAL.print("  val_a=");
    DEBUG_SERIAL.print(g_status_sub.test_value_a, 3);
    DEBUG_SERIAL.print("  val_b=");
    DEBUG_SERIAL.println(g_status_sub.test_value_b, 3);
    DEBUG_SERIAL.print("  uptime=");
    DEBUG_SERIAL.print(g_status_sub.uptime_ms);
    DEBUG_SERIAL.println(" ms");
  }
}

/**
 * @brief MsgPacketizer受信処理タスク (高頻度)
 */
void taskUpdateReceiver() { MsgPacketizer::update(); }

/**
 * @brief Serialコマンド受付タスク
 *
 * 's': 手動で即時ポーリング送信
 * 'h': ヘルプ表示
 */
void taskHandleSerial()
{
  if (!DEBUG_SERIAL.available())
    return;

  char ch = DEBUG_SERIAL.read();
  switch (ch)
  {
  case 's':
  {
    CmdPacket pkt{0xFF, 0x00, ++g_seq};
    rs485Send(IDX_CMD_FROM_MAIN, pkt);
    DEBUG_SERIAL.print("[TX] Manual poll seq=");
    DEBUG_SERIAL.println(g_seq);
    break;
  }
  case '1':
  {
    // sub1 専用コマンド
    CmdPacket pkt{NODE_ID_SUB1, 0x01, ++g_seq};
    rs485Send(IDX_CMD_FROM_MAIN, pkt);
    DEBUG_SERIAL.println("[TX] Command to sub1");
    break;
  }
  case '3':
  {
    // sub 専用コマンド (sub1経由で転送される)
    CmdPacket pkt{NODE_ID_SUB, 0x01, ++g_seq};
    rs485Send(IDX_CMD_FROM_MAIN, pkt);
    DEBUG_SERIAL.println("[TX] Command to sub (via sub1)");
    break;
  }
  case 'h':
    DEBUG_SERIAL.println(
        "Commands: s=send poll, 1=cmd->sub1, 3=cmd->sub, h=help");
    break;
  default:
    break;
  }
}

// ============================================================
// セットアップ
// ============================================================
void setup()
{
  DEBUG_SERIAL.begin(DEBUG_BAUD);
  delay(500);
  DEBUG_SERIAL.println("=================================");
  DEBUG_SERIAL.println(" main node - RS485 test (F756ZG)");
  DEBUG_SERIAL.println("=================================");
  DEBUG_SERIAL.println("Commands: s=poll, 1=->sub1, 3=->sub, h=help");
  DEBUG_SERIAL.println("---------------------------------");

  // RS485 DE初期化
  pinMode(RS485_DE_PIN, OUTPUT);
  rs485RxMode();

  // RS485 UART初期化
  RS485_SERIAL.begin(RS485_BAUD);
  DEBUG_SERIAL.print("RS485 UART started at ");
  DEBUG_SERIAL.println(RS485_BAUD);

  // -------------------------------------------------------
  // MsgPacketizer 受信コールバック登録
  // -------------------------------------------------------

  // sub1からのステータス受信
  MsgPacketizer::subscribe(RS485_SERIAL, IDX_STATUS_SUB1,
                           [](const StatusPacket &pkt)
                           {
                             g_status_sub1 = pkt;
                             g_sub1_updated = true;
                             DEBUG_SERIAL.print("[RX] sub1 status seq=");
                             DEBUG_SERIAL.println(pkt.seq);
                           });

  // sub から中継されてきたステータス受信
  MsgPacketizer::subscribe(RS485_SERIAL, IDX_STATUS_FROM_SUB,
                           [](const StatusPacket &pkt)
                           {
                             g_status_sub = pkt;
                             g_sub_updated = true;
                             DEBUG_SERIAL.print("[RX] sub status seq=");
                             DEBUG_SERIAL.println(pkt.seq);
                           });

  // -------------------------------------------------------
  // TaskManager タスク登録
  // -------------------------------------------------------
  Tasks.add(taskUpdateReceiver)->startFps(200); // 受信処理: 高頻度
  Tasks.add(taskPollAll)->startFps(1);          // ポーリング: 1Hz
  Tasks.add(taskPrintStatus)->startFps(2);      // ステータス表示: 2Hz
  Tasks.add(taskHandleSerial)->startFps(50);    // Serialコマンド受付

  DEBUG_SERIAL.println("TaskManager tasks registered.");
  DEBUG_SERIAL.println("Polling starts at 1Hz...");
}

// ============================================================
// メインループ
// ============================================================
void loop() { Tasks.update(); }
