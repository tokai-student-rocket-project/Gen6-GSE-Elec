/**
 * @file sub1.cpp
 * @brief RS485バス スレーブノード (Nucleo F446RE + LTC1480/ADM2582E)
 */

#include <Arduino.h>
#include <MsgPacketizer.h>
#include <TaskManager.h>

HardwareSerial SerialRS485(PC11, PC10); 

#define RS485_SERIAL SerialRS485 
#define RS485_DE_PIN PA10        
#define DEBUG_SERIAL Serial      

static constexpr uint32_t RS485_BAUD = 115200;
static constexpr uint32_t DEBUG_BAUD = 115200;

static constexpr uint8_t IDX_POLL = 0x01;
static constexpr uint8_t IDX_ANGLE_SUB1 = 0x12;
static constexpr uint8_t NODE_ID_SUB1 = 0x02;

struct AnglePacket {
  float angle;
  uint32_t uptime_ms;
  MSGPACK_DEFINE(angle, uptime_ms);
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
  DEBUG_SERIAL.print("--- sub1 node alive --- replied ");
  DEBUG_SERIAL.print(g_reply_count);
  DEBUG_SERIAL.println(" times / sec");
  g_reply_count = 0;
}

void setup()
{
  DEBUG_SERIAL.begin(DEBUG_BAUD);
  delay(500);
  DEBUG_SERIAL.println("=================================");
  DEBUG_SERIAL.println(" sub1 node - RS485 Slave");
  DEBUG_SERIAL.println("=================================");

  pinMode(RS485_DE_PIN, OUTPUT);
  rs485RxMode();

  RS485_SERIAL.begin(RS485_BAUD);

  MsgPacketizer::subscribe(
      RS485_SERIAL, IDX_POLL, [](uint8_t target_id)
      {
        if (target_id != NODE_ID_SUB1 && target_id != 0xFF) return;

        float t = millis() / 1000.0f;
        AnglePacket resp;
        resp.angle = 180.0f + 180.0f * sinf(t);
        resp.uptime_ms = millis();

        rs485Send(IDX_ANGLE_SUB1, resp);
        g_reply_count++;
      });

  Tasks.add(taskUpdateReceiver)->startFps(200);
  Tasks.add(taskPrintDebug)->startFps(1);
}

void loop() { Tasks.update(); }
