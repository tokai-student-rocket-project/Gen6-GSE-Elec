#include <Arduino.h>
#include <TaskManager.h>

void task1Hz();

void task1Hz()
{
    // digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    // digitalWrite(11, !digitalRead(11)); // はんだづけOK
    // digitalWrite(12, !digitalRead(12)); // はんだづけOK
    // digitalWrite(13, !digitalRead(13)); // はんだづけOK
    // digitalWrite(14, !digitalRead(14)); // はんだづけOK
    digitalWrite(15, !digitalRead(15)); // はんだづけOK
    digitalWrite(16, !digitalRead(16)); // はんだづけOK
    digitalWrite(17, !digitalRead(17)); // はんだづけOK
    digitalWrite(18, !digitalRead(18)); // はんだづけOK
}

void setup()
{
    Serial.begin(115200);

    // pinMode(LED_BUILTIN, OUTPUT);
    pinMode(11, OUTPUT);
    pinMode(12, OUTPUT);
    pinMode(13, OUTPUT);
    pinMode(14, OUTPUT);
    pinMode(15, OUTPUT);
    pinMode(16, OUTPUT);
    pinMode(17, OUTPUT);
    pinMode(18, OUTPUT);

    Tasks.add(&task1Hz)->startFps(0.5);
}

void loop()
{
    Tasks.update();
}