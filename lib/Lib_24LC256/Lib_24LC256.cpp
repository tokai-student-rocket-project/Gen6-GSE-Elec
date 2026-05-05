#include "Lib_24LC256.hpp"

Lib_24LC256::Lib_24LC256(uint8_t i2cAddress) { _deviceAddress = i2cAddress; }

void Lib_24LC256::begin() { Wire.begin(); }

void Lib_24LC256::writeByte(uint16_t memAdress, uint8_t data) {
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

  Wire.requestFrom(_deviceAddress, 1);
  if (Wire.available()) {
    data = Wire.read();
  }
  return data;
}