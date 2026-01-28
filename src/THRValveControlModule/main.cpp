#include <Arduino.h>
#include <Lib_ADXL375.hpp>
#include <Lib_Logger1.hpp>
#include <MsgPacketizer.h>
#include <SPI.h>
#include <TaskManager.h>
#include <Wire.h>
#include <Lib_Neopixel.hpp>

const int speakerPin = 11;

ADXL375 adxl375(D17, D16, D15, D14);
// ADXL375 adxl375(D14, &SPI1);
Logger logger(D17, D16, D15, D12);
Neopixel status(RGB_BUILTIN);

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
static float startTime;

void taskAcc();
void measureFeRAM();

void calibrateSensor()
{
    Serial.println("Calibration starting...");
    Serial.println("Keep sensor flat and stationary (Z should be +1g).");
    delay(2000); // 準備待ち

    long x_sum = 0, y_sum = 0, z_sum = 0;
    int samples = 100;

    Serial.println("Sampling...");
    for (int i = 0; i < samples; i++)
    {
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

void taskAcc()
{
    startTime = millis() / 1000.0;

    adxl375.getAcceleration(&x_acc, &y_acc, &z_acc);

    Serial.print("> Time: ");
    Serial.println((millis() - startTime) / 1000.0);
    Serial.print("> X: ");
    Serial.println(x_acc);
    Serial.print("> Y: ");
    Serial.println(y_acc);
    Serial.print("> Z: ");
    Serial.println(z_acc);
}

void measureFeRAM()
{
    const auto &logPacket =
        MsgPacketizer::encode(0x0A, startTime, x_acc, y_acc, z_acc);

    logger.write(logPacket.data.data(), logPacket.data.size());
}

void setup()
{
    Serial.begin(115200);

    while (!Serial)
        delay(100);

    SPI.begin();

    if (!adxl375.begin())
    {
        Serial.println("adxl375 is not found...");
        digitalWrite(LED_BUILTIN, HIGH);
        while (1)
            ;
    }

    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(11, OUTPUT);
    status.init(PIN_RGB_EN);

    uint8_t fram_id[4];
    logger.getFram()->getId(fram_id);
    Serial.print("FRAM ID: ");
    for (int i = 0; i < 4; i++)
    {
        Serial.print(fram_id[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    adxl375.setTrimOffsets(1, -3, -5);

    Tasks.add(&taskAcc)->startFps(100);
    Tasks.add(&measureFeRAM)->startFps(400);
}
void loop()
{
    Tasks.update();

    status.noticedBlue();

    if (Serial.available())
    {
        char ch = Serial.read();
        if (ch == 'c')
        {
            calibrateSensor();
        }
    }
}