/**
 * @file main.cpp
 * @brief RS485バス マスターノード (Nucleo F756ZG + ADM2582E)
 */

#include <Arduino.h>
#include <MsgPacketizer.h>
#include <TaskManager.h>

HardwareSerial SerialRS485(PG9, PG14); // RX, TX

#define RS485_SERIAL SerialRS485
#define RS485_DE_PIN PF15
#define DEBUG_SERIAL Serial

static constexpr uint32_t RS485_BAUD = 115200;
static constexpr uint32_t DEBUG_BAUD = 115200;

static constexpr uint8_t IDX_POLL = 0x01;
static constexpr uint8_t IDX_STATUS_SUB = 0x11;
static constexpr uint8_t IDX_ANGLE_SUB1 = 0x12;

static constexpr uint8_t NODE_ID_MAIN = 0x01;
static constexpr uint8_t NODE_ID_SUB1 = 0x02;
static constexpr uint8_t NODE_ID_SUB = 0x03;

struct SubStatusPacket {
  float test_value_a;
  float test_value_b;
  uint32_t uptime_ms;
  MSGPACK_DEFINE(test_value_a, test_value_b, uptime_ms);
};

struct AnglePacket {
  float angle;
  uint32_t uptime_ms;
  MSGPACK_DEFINE(angle, uptime_ms);
};

inline void rs485TxMode() { digitalWrite(RS485_DE_PIN, HIGH); }

inline void rs485RxMode()
{
  RS485_SERIAL.flush(); // TX FIFO空待ち
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

static uint32_t g_poll_count = 0;
void taskPoll()
{
  g_poll_count++;
  if (g_poll_count % 5 == 0) {
    rs485Send(IDX_POLL, NODE_ID_SUB1);
  } else {
    rs485Send(IDX_POLL, NODE_ID_SUB);
  }
}

void taskUpdateReceiver() { MsgPacketizer::update(); }

void taskHandleSerial()
{
  if (!DEBUG_SERIAL.available()) return;

  char ch = DEBUG_SERIAL.read();
  switch (ch)
  {
  case 's':
    rs485Send(IDX_POLL, NODE_ID_SUB);
    DEBUG_SERIAL.println("[TX] Manual poll -> sub");
    break;
  case '1':
    rs485Send(IDX_POLL, NODE_ID_SUB1);
    DEBUG_SERIAL.println("[TX] Manual poll -> sub1");
    break;
  case 'h':
    DEBUG_SERIAL.println("Commands: s=poll sub, 1=poll sub1, h=help");
    break;
  default:
    break;
  }
}

void setup()
{
  DEBUG_SERIAL.begin(DEBUG_BAUD);
  delay(500);
  DEBUG_SERIAL.println("=================================");
  DEBUG_SERIAL.println(" main node - RS485 Master");
  DEBUG_SERIAL.println("=================================");
  DEBUG_SERIAL.println("Commands: s=poll sub, 1=poll sub1, h=help");
  
  pinMode(RS485_DE_PIN, OUTPUT);
  rs485RxMode();

  RS485_SERIAL.begin(RS485_BAUD);

  MsgPacketizer::subscribe(RS485_SERIAL, IDX_STATUS_SUB,
                           [](const SubStatusPacket &pkt)
                           {
                             DEBUG_SERIAL.print("[RX] sub -> a:");
                             DEBUG_SERIAL.print(pkt.test_value_a, 2);
                             DEBUG_SERIAL.print(" b:");
                             DEBUG_SERIAL.print(pkt.test_value_b, 2);
                             DEBUG_SERIAL.print(" up:");
                             DEBUG_SERIAL.println(pkt.uptime_ms);
                           });

  MsgPacketizer::subscribe(RS485_SERIAL, IDX_ANGLE_SUB1,
                           [](const AnglePacket &pkt)
                           {
                             DEBUG_SERIAL.print("[RX] sub1 -> angle:");
                             DEBUG_SERIAL.print(pkt.angle, 2);
                             DEBUG_SERIAL.print(" up:");
                             DEBUG_SERIAL.println(pkt.uptime_ms);
                           });

  Tasks.add(taskUpdateReceiver)->startFps(200);   
  Tasks.add(taskPoll)->startFps(5); // Automatic polling at a comfortable 5Hz              
  Tasks.add(taskHandleSerial)->startFps(50);      
  
  DEBUG_SERIAL.println("TaskManager initialized. Master active.");
}

void loop() { Tasks.update(); }
