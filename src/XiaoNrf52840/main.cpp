#include <Arduino.h>
#include "LSM6DS3.h"
#include <Wire.h>
#include <TaskManager.h>
#include <MsgPacketizer.h>

// ===== IMU =====
LSM6DS3 myIMU(I2C_MODE, 0x6A);
bool ledState = false;

// ===== IMU タスク 400Hz =====
void taskReadIMU()
{
    float ax = myIMU.readFloatAccelX();
    float ay = myIMU.readFloatAccelY();
    float az = myIMU.readFloatAccelZ();
    float gx = myIMU.readFloatGyroX();
    float gy = myIMU.readFloatGyroY();
    float gz = myIMU.readFloatGyroZ();

    MsgPacketizer::send(Serial, 0x10, ax, ay, az, gx, gy, gz);

    // デバッグ ASCII (10Hz)
    static uint16_t cnt = 0;
    if (++cnt >= 40)
    {
        cnt = 0;
        Serial.print(">Acc_X[g]:");
        Serial.println(ax, 3);
        Serial.print(">Acc_Y[g]:");
        Serial.println(ay, 3);
        Serial.print(">Acc_Z[g]:");
        Serial.println(az, 3);
        ledState = !ledState;
        digitalWrite(LED_GREEN, ledState ? LOW : HIGH);
    }
}

void setup()
{
    Serial.begin(921600);
    delay(2000);

    pinMode(LED_RED, OUTPUT);
    digitalWrite(LED_RED, HIGH);
    pinMode(LED_GREEN, OUTPUT);
    digitalWrite(LED_GREEN, HIGH);
    pinMode(LED_BLUE, OUTPUT);
    digitalWrite(LED_BLUE, HIGH);
    digitalWrite(LED_BLUE, LOW);

    Serial.println("=== XIAO nRF52840 Sense: IMU + Mic Logger ===");

    // ---- IMU 設定 ----
    myIMU.settings.accelRange = 16;
    myIMU.settings.accelSampleRate = 1666;
    myIMU.settings.accelBandWidth = 400;
    myIMU.settings.gyroRange = 2000;
    myIMU.settings.gyroSampleRate = 1666;

    if (myIMU.begin() != IMU_SUCCESS)
    {
        Serial.println("[ERROR] IMU init failed!");
        while (true)
        {
            digitalWrite(LED_RED, LOW);
            delay(200);
            digitalWrite(LED_RED, HIGH);
            delay(200);
        }
    }
    Serial.println("[OK] IMU ready (±16g, 1666Hz)");

    digitalWrite(LED_BLUE, HIGH);

    Tasks.add(&taskReadIMU)->startFps(400);
}

void loop()
{
    Tasks.update();
}
