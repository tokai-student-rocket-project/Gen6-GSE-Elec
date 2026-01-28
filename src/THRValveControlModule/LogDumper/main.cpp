#include <Arduino.h>
#include "Lib_FRAM.hpp"
#include <MsgPacketizer.h>
#include <Lib_Neopixel.hpp>

FRAM fram0(D17, D16, D15, D12);
Neopixel status(RGB_BUILTIN);

void dump(FRAM *fram)
{
    for (uint32_t address = 0; address < FRAM::LENGTH; address++)
    {
        uint8_t b = fram->read(address);
        MsgPacketizer::feed(&b, 1);
    }
}

void printHeader() { Serial.print("time,x_acc,y_acc,z_acc\n"); }

void setup()
{
    Serial.begin(115200);

    pinMode(LED_BUILTIN, OUTPUT);
    status.init(PIN_RGB_EN);
    status.noticedPink();

    MsgPacketizer::subscribe_manual(
        0x0A, [](float time, float x_acc, float y_acc, float z_acc)
        {
        Serial.print(time);
        Serial.print(",");
        Serial.print(x_acc);
        Serial.print(",");
        Serial.print(y_acc);
        Serial.print(",");
        Serial.print(z_acc);
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
