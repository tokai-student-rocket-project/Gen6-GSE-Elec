#include "Lib_B3msc1170a.hpp"
#include "Lib_ICM42688.hpp"
#include <Arduino.h>
#include <Lib_ADXL375.hpp>
#include <Lib_Logger1.hpp>
#include <Lib_Neopixel.hpp>
#include <MsgPacketizer.h>
#include <SPI.h>
#include <TaskManager.h>
#include <Wire.h>


const int speakerPin = 11;
const uint8_t mosi = D15;
const uint8_t miso = D16;
const uint8_t sck = D17;
const uint8_t cs_icm42688 = D18;
const uint8_t cs_adxl375 = D14;

ICM42688 icm42688(sck, miso, mosi, cs_icm42688);
ADXL375 adxl375(sck, miso, mosi, cs_adxl375);
// ADXL375 adxl375(D14, &SPI1);
Logger logger(D17, D16, D15, D12);

Neopixel status(RGB_BUILTIN);
B3MSC1170A b3m;

const byte SERVO_ID = 0x01;

// 動作設定
const int POS_PLUS_45 = 4500;   // +45度 (0.01deg単位)
const int POS_MINUS_45 = -4500; // -45度 (0.01deg単位)
const int MOVE_TIME = 2000;     // 移動時間[ms]

void taskAcc();
void measureFeRAM();
void taskServo();
void printCsvHeader();

// 音階の周波数定義 (ドレミ...)
#define NOTE_C4 262 // ド
#define NOTE_D4 294 // レ
#define NOTE_E4 330 // ミ
#define NOTE_F4 349 // ファ
#define NOTE_G4 392 // ソ
#define NOTE_A4 440 // ラ
#define NOTE_B4 494 // シ
#define NOTE_C5 523 // 高いド

// 鳴らしたいメロディの配列
int melody[] = {NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4,
                NOTE_G4, NOTE_A4, NOTE_B4, NOTE_C5};

// 各音の長さ (4 = 四分音符, 8 = 八分音符)
int noteDurations[] = {4, 4, 4, 4, 4, 4, 4, 4};

float x_acc, y_acc, z_acc;
float x_acc_icm, y_acc_icm, z_acc_icm;
float x_gyro_icm, y_gyro_icm, z_gyro_icm;
static float startTime;

void taskAcc();
void measureFeRAM();

void calibrateSensor() {
  Serial.println("Calibration starting...");
  Serial.println("Keep sensor flat and stationary (Z should be +1g).");
  delay(2000); // 準備待ち

  long x_sum = 0, y_sum = 0, z_sum = 0;
  int samples = 100;

  Serial.println("Sampling...");
  for (int i = 0; i < samples; i++) {
    int16_t x, y, z;
    adxl375.getXYZ(&x, &y, &z);
    x_sum += x;
    y_sum += y;
    z_sum += z;
    delay(10);
  }

  float x_avg = x_sum / (float)samples;
  float y_avg = y_sum / (float)samples;
  float z_avg = z_sum / (float)samples;

  Serial.print("Average RAW: X=");
  Serial.print(x_avg);
  Serial.print(" Y=");
  Serial.print(y_avg);
  Serial.print(" Z=");
  Serial.println(z_avg);

  // 目標値: X=0, Y=0, Z=20.4 (1g @ 49mg/LSB) -> 約20
  // オフセット計算: (Target - Measured) / OffsetScale
  // ADXL375 Offset Scale = 196 mg/LSB = 4 * 49mg/LSB (approx)
  // したがって、RAW値の差分を 4 で割ればよい (おおよそ)

  // 正確には:
  // Offset_Reg_Value = (Target_g - Measured_g) / 0.196g
  // Target_g for X,Y = 0
  // Target_g for Z = 1.0 (assuming flat table)

  // Measured_g = average_raw * 0.049

  // Offset X = (0 - x_avg * 0.049) / 0.196 = -x_avg * (0.049/0.196) = -x_avg /
  // 4 Offset Y = -y_avg / 4 Offset Z = (1.0 - z_avg * 0.049) / 0.196 =
  // (20.4*0.049 - z_avg*0.049) / 0.196 = (20.4 - z_avg) / 4

  int8_t off_x = round(-x_avg / 4.0);
  int8_t off_y = round(-y_avg / 4.0);
  int8_t off_z = round((20.4 - z_avg) / 4.0); // Z target is +1g (~20LSB)

  Serial.print("Calculated Offsets: X=");
  Serial.print(off_x);
  Serial.print(" Y=");
  Serial.print(off_y);
  Serial.print(" Z=");
  Serial.println(off_z);

  adxl375.setTrimOffsets(off_x, off_y, off_z);
  Serial.println("Offsets Applied!");
  Serial.println("----------------------------------------");
  Serial.println("To save permanently, add this line to setup():");
  Serial.print("accel.setTrimOffsets(");
  Serial.print(off_x);
  Serial.print(", ");
  Serial.print(off_y);
  Serial.print(", ");
  Serial.print(off_z);
  Serial.println(");");
  Serial.println("----------------------------------------");
}

void taskAcc() {
  startTime = millis() / 1000.0;

  icm42688.readSensor();
  x_acc_icm = icm42688.getAccelX();
  y_acc_icm = icm42688.getAccelY();
  z_acc_icm = icm42688.getAccelZ();
  x_gyro_icm = icm42688.getGyroX();
  y_gyro_icm = icm42688.getGyroY();
  z_gyro_icm = icm42688.getGyroZ();

  adxl375.getAcceleration(&x_acc, &y_acc, &z_acc);

  // Serial.print("> Time: ");
  // Serial.println((millis() - startTime) / 1000.0);
  // Serial.print("> X_highAcc: ");
  // Serial.println(x_acc);
  // Serial.print("> Y_highAcc: ");
  // Serial.println(y_acc);
  // Serial.print("> Z_highAcc: ");
  // Serial.println(z_acc);
  // Serial.print("> X_acc: ");
  // Serial.println(x_acc_icm);
  // Serial.print("> Y_acc: ");
  // Serial.println(y_acc_icm);
  // Serial.print("> Z_acc: ");
  // Serial.println(z_acc_icm);
  // Serial.print("> X_gyro: ");
  // Serial.println(x_gyro_icm);
  // Serial.print("> Y_gyro: ");
  // Serial.println(y_gyro_icm);
  // Serial.print("> Z_gyro: ");
  // Serial.println(z_gyro_icm);
}

void measureFeRAM() {
  const auto &logPacket = MsgPacketizer::encode(
      0x0A, startTime, x_acc, y_acc, z_acc, x_acc_icm, y_acc_icm, z_acc_icm,
      x_gyro_icm, y_gyro_icm, z_gyro_icm);

  logger.write(logPacket.data.data(), logPacket.data.size());
}

void taskServo() {
  // 45.00度 (4500) と -45.00度 (-4500) を交互に繰り返す例
  static int targetPos = 4500;
  b3m.setPosition(SERVO_ID, targetPos, 0);

  if (targetPos == 4500)
    targetPos = -4500;
  else
    targetPos = 4500;
}

void setup() {
  b3m.initialize(SERVO_ID);
  delay(100);
  Serial.begin(115200);

  // while (!Serial)
  delay(100);

  SPI.begin();

  if (!adxl375.begin()) {
    Serial.println("adxl375 is not found...");
  }

  if (!icm42688.begin()) {
    Serial.println("icm42688 is not found...");
  }

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(11, OUTPUT);
  status.init(PIN_RGB_EN);

  adxl375.setTrimOffsets(0, 0, 0);

  status.noticedBlue();

  // b3m.setPosition(SERVO_ID, 0, 0);

  Tasks.add(&taskAcc)->startFps(100);
  Tasks.add(&measureFeRAM)->startFps(100);
  Tasks.add(&taskServo)->startFps(0.5); // 2秒に1回更新
}
void loop() {
  Tasks.update();

  if (Serial.available()) {
    char ch = Serial.read();
    if (ch == 'c') {
      calibrateSensor();
    }
  }
}