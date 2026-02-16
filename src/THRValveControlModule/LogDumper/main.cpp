#include "Lib_FRAM.hpp"
#include "Lib_Neopixel.hpp"
#include "Lib_SC8721.hpp"
#include <Arduino.h>
#include <MsgPacketizer.h>

FRAM fram0(D17, D16, D15, D12);
Neopixel status(RGB_BUILTIN);
SC8721 regulator(&Wire);

void dump(FRAM *fram)
{
  uint32_t lastUpdate = 0;
  for (uint32_t address = 0; address < FRAM::LENGTH; address++)
  {
    uint8_t b = fram->read(address);
    MsgPacketizer::feed(&b, 1);

    // 20msごとにLEDを更新
    if (millis() - lastUpdate > 20)
    {
      lastUpdate = millis();
      float progress = (float)address / FRAM::LENGTH;

      // 進捗 0.0 -> Hue 21845 (Green), 進捗 1.0 -> Hue 0 (Red)
      uint16_t hp_hue = (uint16_t)(21845.0f * (1.0f - progress));

      // 蛍効果: サイン波で輝度をゆっくり明滅させる（Value: 10 - 150）
      uint8_t val = (uint8_t)((sin(millis() * 0.003) + 1.2) * 60);

      status.setHSV(hp_hue, 255, val);
      status.show();
    }
  }
  status.off(); // 終了時に消灯
}

// csv出力時のヘッダーを用意
void printHeader()
{
  Serial.println("Time_ms,adxl_x,adxl_y,adxl_z,icm_acc_x,icm_acc_y,icm_acc_z,"
                 "icm_gyro_x,icm_gyro_y,icm_gyro_z");
}

void setup()
{
  Serial.begin(115200);
  Wire.begin();
  regulator.begin();
  regulator.setVoltage(13.0f);

  pinMode(LED_BUILTIN, OUTPUT);
  status.init(PIN_RGB_EN);
  status.noticedPink();

  MsgPacketizer::subscribe_manual(
      0x0A, [](float time, float x_acc, float y_acc, float z_acc,
               float x_acc_icm, float y_acc_icm, float z_acc_icm,
               float x_gyro_icm, float y_gyro_icm, float z_gyro_icm)
      {
        Serial.print(time);
        Serial.print(",");
        Serial.print(x_acc);
        Serial.print(",");
        Serial.print(y_acc);
        Serial.print(",");
        Serial.print(z_acc);
        Serial.print(",");
        Serial.print(x_acc_icm);
        Serial.print(",");
        Serial.print(y_acc_icm);
        Serial.print(",");
        Serial.print(z_acc_icm);
        Serial.print(",");
        Serial.print(x_gyro_icm);
        Serial.print(",");
        Serial.print(y_gyro_icm);
        Serial.print(",");
        Serial.print(z_gyro_icm);
        Serial.print("\n"); });

  while (!Serial)
    ;
  delay(1000);

  uint8_t fram_id[4];
  fram0.getId(fram_id);
  Serial.print("FRAM ID: ");
  for (int i = 0; i < 4; i++)
  {
    Serial.print(fram_id[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  printHeader();

  digitalWrite(LED_BUILTIN, LOW);
  dump(&fram0);
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {}
