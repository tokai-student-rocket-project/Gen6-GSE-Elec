/**
 * @file sub.cpp
 * @brief RS485バス スレーブノード (Nucleo F756ZG + ADM2582E)
 */

#include <Arduino.h>
#include <MsgPacketizer.h>
#include <TaskManager.h>

HardwareSerial SerialRS485(PG9, PG14); 

#define RS485_SERIAL SerialRS485 
#define RS485_DE_PIN PF15        
#define DEBUG_SERIAL Serial      

static constexpr uint32_t RS485_BAUD = 115200;
static constexpr uint32_t DEBUG_BAUD = 115200;

static constexpr uint8_t IDX_POLL = 0x01;
static constexpr uint8_t IDX_STATUS_SUB = 0x11;
static constexpr uint8_t NODE_ID_SUB = 0x03;

struct SubStatusPacket {
  float test_value_a;
  float test_value_b;
  uint32_t uptime_ms;
  MSGPACK_DEFINE(test_value_a, test_value_b, uptime_ms);
};

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

void taskUpdateReceiver() { MsgPacketizer::update(); }

static uint32_t g_reply_count = 0;
void taskPrintDebug()
{
  DEBUG_SERIAL.print("--- sub node alive --- replied ");
  DEBUG_SERIAL.print(g_reply_count);
  DEBUG_SERIAL.println(" times / sec");
  g_reply_count = 0;
}

void setup()
{
  DEBUG_SERIAL.begin(DEBUG_BAUD);
  delay(500);
  DEBUG_SERIAL.println("================================");
  DEBUG_SERIAL.println(" sub node - RS485 Slave");
  DEBUG_SERIAL.println("================================");

  pinMode(RS485_DE_PIN, OUTPUT);
  rs485RxMode();

  RS485_SERIAL.begin(RS485_BAUD);

  MsgPacketizer::subscribe(
      RS485_SERIAL, IDX_POLL, [](uint8_t target_id)
      {
        if (target_id != NODE_ID_SUB && target_id != 0xFF) return;

        float t = millis() / 1000.0f;
        SubStatusPacket resp;
        resp.test_value_a = 5.0f * (0.5f + 0.5f * sinf(t * 1.5f));
        resp.test_value_b = 200.0f * (0.5f + 0.5f * cosf(t * 0.3f));
        resp.uptime_ms = millis();

        rs485Send(IDX_STATUS_SUB, resp);
        g_reply_count++;
      });

  Tasks.add(taskUpdateReceiver)->startFps(200); 
  Tasks.add(taskPrintDebug)->startFps(1);       
}

void loop() { Tasks.update(); }
