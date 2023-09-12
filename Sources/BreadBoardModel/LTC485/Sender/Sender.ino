#include <MsgPacketizer.h>
#include <TaskManager.h>

const uint8_t outputEnablePin = 2;
void enableOutput();
void disableOutput();

void sendTestPacketTask();

void setup() {
  // RS485の送信が終わったら割り込みを発生させる
  UCSR0B |= (1 << TXCIE0);

  pinMode(outputEnablePin, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  disableOutput();

  Serial.begin(115200);

  // 2Hzでテストパケットを送信するタスク
  Tasks.add([] {sendTestPacketTask();})->startFps(2);
}


void loop() {
  Tasks.update();
}


/// @brief RS485の送信が終わったら送信を無効にするイベントハンドラ
ISR(USART_TX_vect) {
  disableOutput();
}


/// @brief 送信を有効にする
void enableOutput() {
  digitalWrite(outputEnablePin, HIGH);
  digitalWrite(LED_BUILTIN, HIGH);
}


/// @brief 送信を無効にする
void disableOutput() {
  digitalWrite(outputEnablePin, LOW);
  digitalWrite(LED_BUILTIN, LOW);
}


/// @brief 2Hzでテストパケットを送信するタスク
void sendTestPacketTask() {
  // 適当な送信するデータ
  const float value = (float)millis() / 1000.0;

  enableOutput();
  MsgPacketizer::send(Serial, static_cast<uint8_t>(0xAA), value);
}