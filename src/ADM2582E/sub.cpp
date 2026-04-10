/**
 * @file sub.cpp
 * @brief RS485バス スレーブエンドノード (Nucleo F756ZG + ADM2582E)
 *
 * バス構成:
 *   [main (F756ZG)] ---RS485--- [sub1 (F446RE)] ---RS485--- [sub (F756ZG)]
 *                                                                ↑
 *                                                           このファイル
 *
 * 役割:
 *   - sub1から転送されたコマンドパケット(index=0x21)を受信・処理
 *   - 自ノードのステータスをsub1へ返信(index=0x31) → sub1がmainへ中継
 *   - TaskManagerで定期的な自発ステータス送信も行う
 *
 * ピン配置 (Nucleo F756ZG):
 *   RS485 UART : USART6 (PG9=RX, PG14=TX)
 *   RS485 DE   : PF15 (HIGH=送信, LOW=受信)
 *   Debug UART : Serial (USB/ST-LINK)
 *
 * パケットIndex一覧:
 *   0x01 : main → 全ノード  コマンド/ポーリング要求
 *   0x21 : sub1 → sub      転送パケット
 *   0x31 : sub  → main     subのステータス (sub1経由で中継される)
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
static constexpr uint8_t IDX_FORWARD_TO_SUB = 0x21;  // sub1 → sub
static constexpr uint8_t IDX_STATUS_FROM_SUB = 0x31; // sub → main (sub1経由)

// ノードID
static constexpr uint8_t NODE_ID_SUB = 0x03;

// ============================================================
// データ構造
// ============================================================

// sub1から転送されてくるコマンドパケット
struct ForwardPacket
{
  uint8_t from_id;
  uint8_t target_id;
  uint8_t cmd;
  uint32_t seq;
  MSGPACK_DEFINE(from_id, target_id, cmd, seq);
};

// subがsub1へ返すステータスパケット (sub1がmainへ中継)
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
static uint32_t g_last_seq = 0;
static uint32_t g_recv_count = 0;

// ============================================================
// RS485送受信切替ヘルパー
// ============================================================
inline void rs485TxMode() { digitalWrite(RS485_DE_PIN, HIGH); }

inline void rs485RxMode()
{
  RS485_SERIAL.flush();
  delayMicroseconds(100);
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
 * @brief 定期ステータス送信タスク (2Hz)
 *
 * sub1へ自ノードのテストデータを送信する。
 * sub1はこれをmainへ中継する。
 */
void taskSendStatus()
{
  float t = millis() / 1000.0f;

  StatusPacket pkt;
  pkt.node_id = NODE_ID_SUB;
  pkt.seq = g_last_seq;
  pkt.test_value_a =
      5.0f * (0.5f + 0.5f * sinf(t * 1.5f));                  // 0.0〜5.0V 疑似電圧
  pkt.test_value_b = 200.0f * (0.5f + 0.5f * cosf(t * 0.3f)); // 0〜200 疑似値
  pkt.uptime_ms = millis();

  rs485Send(IDX_STATUS_FROM_SUB, pkt);

  DEBUG_SERIAL.print("[TX] Status seq=");
  DEBUG_SERIAL.print(pkt.seq);
  DEBUG_SERIAL.print(" A=");
  DEBUG_SERIAL.print(pkt.test_value_a, 3);
  DEBUG_SERIAL.print(" B=");
  DEBUG_SERIAL.println(pkt.test_value_b, 3);
}

/**
 * @brief MsgPacketizer受信処理タスク (高頻度)
 */
void taskUpdateReceiver() { MsgPacketizer::update(); }

// ============================================================
// セットアップ
// ============================================================
void setup()
{
  DEBUG_SERIAL.begin(DEBUG_BAUD);
  delay(500);
  DEBUG_SERIAL.println("================================");
  DEBUG_SERIAL.println(" sub node - RS485 test (F756ZG)");
  DEBUG_SERIAL.println("================================");

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

  // sub1からの転送コマンドを受信
  MsgPacketizer::subscribe(
      RS485_SERIAL, IDX_FORWARD_TO_SUB, [](const ForwardPacket &fwd)
      {
        ++g_recv_count;
        g_last_seq = fwd.seq;

        DEBUG_SERIAL.print("[RX] ForwardPacket from=0x");
        DEBUG_SERIAL.print(fwd.from_id, HEX);
        DEBUG_SERIAL.print(" target=0x");
        DEBUG_SERIAL.print(fwd.target_id, HEX);
        DEBUG_SERIAL.print(" cmd=0x");
        DEBUG_SERIAL.print(fwd.cmd, HEX);
        DEBUG_SERIAL.print(" seq=");
        DEBUG_SERIAL.println(fwd.seq);

        // 自ノード宛またはブロードキャストの場合のみ返信
        if (fwd.target_id != NODE_ID_SUB && fwd.target_id != 0xFF)
          return;

        float t = millis() / 1000.0f;
        StatusPacket resp;
        resp.node_id = NODE_ID_SUB;
        resp.seq = fwd.seq;
        resp.test_value_a = 5.0f * (0.5f + 0.5f * sinf(t * 1.5f));
        resp.test_value_b = 200.0f * (0.5f + 0.5f * cosf(t * 0.3f));
        resp.uptime_ms = millis();

        rs485Send(IDX_STATUS_FROM_SUB, resp);

        DEBUG_SERIAL.print("[TX] Immediate reply seq=");
        DEBUG_SERIAL.println(resp.seq); });

  // -------------------------------------------------------
  // TaskManager タスク登録
  // -------------------------------------------------------
  Tasks.add(taskUpdateReceiver)->startFps(200); // 受信処理: 高頻度
  Tasks.add(taskSendStatus)->startFps(2);       // 定期ステータス送信: 2Hz

  DEBUG_SERIAL.println("TaskManager tasks registered.");
  DEBUG_SERIAL.println("Waiting for packets from sub1...");
  DEBUG_SERIAL.println("--------------------------------");
}

// ============================================================
// メインループ
// ============================================================
void loop()
{
  Tasks.update();
  // Serial.println(digitalRead(RS485_DE_PIN));
}
