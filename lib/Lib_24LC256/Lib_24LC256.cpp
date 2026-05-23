#include "Lib_24LC256.hpp"

Lib_24LC256::Lib_24LC256(uint8_t i2cAddress) { _deviceAddress = i2cAddress; }

void Lib_24LC256::begin() { Wire.begin(); }

void Lib_24LC256::writeByte(uint16_t memAddress, uint8_t data) {
  Wire.beginTransmission(_deviceAddress);

  Wire.write((uint8_t)(memAddress >> 8));
  Wire.write((uint8_t)(memAddress & 0xFF));

  Wire.write(data);
  Wire.endTransmission();

  uint32_t startTime = millis();
  while (millis() - startTime < 10) {
    Wire.beginTransmission(_deviceAddress);
    // endTransmission()が0を返せばACKを受信（書き込み完了）
    if (Wire.endTransmission() == 0) {
      break;
    }
  }
}

uint8_t Lib_24LC256::readByte(uint16_t memAddress) {
  uint8_t data;

  Wire.beginTransmission(_deviceAddress);
  Wire.write((uint8_t)(memAddress >> 8));
  Wire.write((uint8_t)(memAddress & 0xFF));
  Wire.endTransmission();

  Wire.requestFrom(_deviceAddress, (uint8_t)1);
  if (Wire.available()) {
    data = Wire.read();
  }
  return data;
}

// ---------------------------------------------------------
// 複数バイトの書き込み（ページ境界とWireバッファ制限を自動処理）
// ---------------------------------------------------------
void Lib_24LC256::writeBuffer(uint16_t memAddress, const uint8_t* data, uint16_t length) {
  uint16_t dataIndex = 0;
  
  // Wireライブラリの送信バッファは通常32バイト制限です。
  // メモリアドレスの指定で2バイト使用するため、
  // 1回の送信で送れる実際のデータは最大30バイトになります。
  const uint16_t MAX_PAYLOAD_SIZE = 30;

  while (dataIndex < length) {
    // 24LC256のページサイズは64バイトです。
    // ページをまたぐと先頭に戻って上書きしてしまうため、
    // 現在のアドレスから「次のページ境界まで何バイトあるか」を計算します。
    uint16_t spaceInPage = 64 - (memAddress % 64);
    
    // 今回の1回の送信で書き込むバイト数を決定します。
    // 「残りのデータ長」「ページ残量」「Wireのバッファ限界(30)」の中で一番小さい値を選びます。
    uint16_t bytesToWrite = length - dataIndex;
    if (bytesToWrite > spaceInPage) bytesToWrite = spaceInPage;
    if (bytesToWrite > MAX_PAYLOAD_SIZE) bytesToWrite = MAX_PAYLOAD_SIZE;

    // --- データの送信開始 ---
    Wire.beginTransmission(_deviceAddress);
    Wire.write((uint8_t)(memAddress >> 8));
    Wire.write((uint8_t)(memAddress & 0xFF));

    // バッファからデータを取り出して箱（送信バッファ）に詰める
    for (uint16_t i = 0; i < bytesToWrite; i++) {
      Wire.write(data[dataIndex + i]);
    }
    Wire.endTransmission(); // 実際に送信（発送）

    // 次回の送信に備えてアドレスとインデックスを進める
    memAddress += bytesToWrite;
    dataIndex += bytesToWrite;

    // --- EEPROMが内部に焼き付けるのを待つ（ACKポーリング） ---
    uint32_t startTime = millis();
    while (millis() - startTime < 10) {
      Wire.beginTransmission(_deviceAddress);
      if (Wire.endTransmission() == 0) {
        break; // 返事(ACK)が来たら書き込み完了
      }
    }
  }
}

// ---------------------------------------------------------
// 複数バイトの読み出し（Wireバッファ制限を自動処理）
// ---------------------------------------------------------
void Lib_24LC256::readBuffer(uint16_t memAddress, uint8_t* data, uint16_t length) {
  uint16_t dataIndex = 0;
  
  // 読み出しの場合も、Wireのバッファ制限（最大32バイト）を考慮します。
  const uint16_t MAX_READ_SIZE = 32;

  while (dataIndex < length) {
    // 今回要求するバイト数を計算
    uint16_t bytesToRead = length - dataIndex;
    if (bytesToRead > MAX_READ_SIZE) {
      bytesToRead = MAX_READ_SIZE;
    }

    // 1. 読み出したい番地をデバイスに伝える
    Wire.beginTransmission(_deviceAddress);
    Wire.write((uint8_t)(memAddress >> 8));
    Wire.write((uint8_t)(memAddress & 0xFF));
    Wire.endTransmission();

    // 2. データを要求する
    Wire.requestFrom(_deviceAddress, (uint8_t)bytesToRead);

    // 3. 受け取ったデータをバッファに格納する
    for (uint16_t i = 0; i < bytesToRead; i++) {
      if (Wire.available()) {
        data[dataIndex + i] = Wire.read();
      }
    }

    // 次回の読み出しのためにアドレスとインデックスを進める
    memAddress += bytesToRead;
    dataIndex += bytesToRead;
  }
}