#pragma once

#include <Arduino.h>
#include "IcsBaseClass.h"
#include "IcsHardSerialClass.h"

class B3MSC1170A
{
public:
    void initialize(byte id);
    void torqueOff(byte id);

    int writeCommand(byte id, byte TxData, byte Address);
    int setPosition(byte id, int Pos, int Time);
    int16_t readDesiredPosition(byte id);
    int16_t readMotorTemperature(byte id);
    int16_t readMcuTemperature(byte id);
    int16_t readCurrentPosition(byte id);
    int16_t readCurrentVelosity(byte id);
    int16_t readVoltage(byte id);
    int16_t readCurrent(byte id);

private:
    IcsHardSerialClass *_b3msc1170a;
};
