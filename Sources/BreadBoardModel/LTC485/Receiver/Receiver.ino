#include <MsgPacketizer.h>

const uint8_t outputEnablePin = 2;
void enableOutput();
void disableOutput();

void setup() {
  pinMode(outputEnablePin, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  disableOutput();

  Serial.begin(115200);

  // 正常なパケットを受信したら点滅
  MsgPacketizer::subscribe(Serial, static_cast<uint8_t>(0xAA),
    [](float value) {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
  );
}


void loop() {
  MsgPacketizer::parse();
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