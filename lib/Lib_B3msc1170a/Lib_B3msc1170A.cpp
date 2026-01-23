#include "Lib_B3msc1170a.hpp"

void B3MSC1170A::initialize(byte id)
{
    _b3msc1170a = new IcsHardSerialClass(&Serial1, 27, 115200, 10);
    _b3msc1170a->begin();

    uint8_t _id = (byte)(id);

    writeCommand(_id, 0x02, 0x28);
    writeCommand(_id, 0x02, 0x28);
    writeCommand(_id, 0x01, 0x29);
    writeCommand(_id, 0x00, 0x5c);
    writeCommand(_id, 0x00, 0x28);
}

void B3MSC1170A::torqueOff(byte id)
{
    writeCommand(id, 0x02, 0x28);
}

int B3MSC1170A::writeCommand(byte id, byte TxData, byte Address)
{
    byte txCmd[8];
    byte rxCmd[5];
    unsigned int reData;
    bool flag;

    txCmd[0] = (byte)(0x08); // SIZE      //一連コマンドのバイト数。今回は8バイト
    txCmd[1] = (byte)(0x04); // COMMAND   //何をするための処理か設定。 0x04はWrite
    txCmd[2] = (byte)(0x00); // OPTION    //ステータスの読み取り。 0x00はERROR STATUS
    txCmd[3] = (byte)(id);   // ID        //制御するサーボID番号を指定

    txCmd[4] = (byte)(TxData);  // DATA      //
    txCmd[5] = (byte)(Address); // ADDRESS   //

    txCmd[6] = (byte)(0x01); // COUNT
    txCmd[7] = (byte)(0x00); // 初期化

    // Serial.print("tx: ");
    for (int i = 0; i < 7; i++)
    {
        txCmd[7] += txCmd[i];
        // Serial.print(txCmd[i]);
        // Serial.print("|");
    }
    // Serial.println("");
    txCmd[7] = (byte)(txCmd[7]); // CHECKSUM

    // flag = B3M.synchronize(txCmd, sizeof txCmd, rxCmd, sizeof rxCmd);
    flag = _b3msc1170a->synchronize(txCmd, 8, rxCmd, 5);

    if (flag == false)
    {
        return -1;
    }

    // Serial.print("rx: ");
    for (int o = 0; o < 5; o++)
    {
        rxCmd[5] += rxCmd[o];
        // Serial.print(rxCmd[o]);
        // Serial.print("|");
    }
    // Serial.println("");

    reData = rxCmd[2];
    return reData;
}

int B3MSC1170A::setPosition(byte id, int Pos, int Time)
{
    byte txCmd[9];
    byte rxCmd[7];
    unsigned int reData;
    bool flag;

    txCmd[0] = (byte)(0x09); // SIZE
    txCmd[1] = (byte)(0x06); // COMMAND //0x06はポジションを変更する
    txCmd[2] = (byte)(0x00); // OPTION
    txCmd[3] = (byte)(id);   // ID

    txCmd[4] = (byte)(Pos & 0xFF);      // POS_L
    txCmd[5] = (byte)(Pos >> 8 & 0xFF); // POS_H

    txCmd[6] = (byte)(Time & 0xFF);      // TIME_L
    txCmd[7] = (byte)(Time >> 8 & 0xFF); // TIME_H

    txCmd[8] = 0x00;

    // Serial.print("tx: ");
    for (int i = 0; i < 8; i++)
    {
        txCmd[8] += txCmd[i];
        // Serial.print(txCmd[i]);
        // Serial.print("|");
    }
    // Serial.println("|");
    txCmd[8] = (byte)(txCmd[8]); // SUM

    // flag = B3M.synchronize(txCmd, sizeof txCmd, rxCmd, sizeof rxCmd);
    flag = _b3msc1170a->synchronize(txCmd, 9, rxCmd, 7);

    if (flag == false)
    {
        return -1;
    }

    // Serial.print("rx: ");
    for (int o = 0; o < 7; o++)
    {
        // rxCmd[7] += rxCmd[o];
        // Serial.print(rxCmd[o]);
        // Serial.print("|");
    }

    reData = rxCmd[7];
    return reData;
}

int16_t B3MSC1170A::readVoltage(byte id)
{
    byte txCmd[7];
    byte rxCmd[7];
    int16_t value;
    int16_t reData;
    bool flag;

    txCmd[0] = (byte)(0x07); // SIZE
    txCmd[1] = (byte)(0x03); // COMMAND
    txCmd[2] = (byte)(0x00); // OPTION
    txCmd[3] = (byte)(id);   // ID

    txCmd[4] = (byte)(0x4A); // ADDRESS  //現在負荷電流値のアドレス
    txCmd[5] = (byte)(0x02); // LENGTH

    txCmd[6] = 0x00; // SUM

    for (uint8_t i = 0; i < 6; i++)
    {
        txCmd[6] += txCmd[i];
    }

    txCmd[6] = (byte)(txCmd[6]);
    flag = _b3msc1170a->synchronize(txCmd, 7, rxCmd, 7);

    if (flag == false)
    {
        return -1;
    }

    for (uint8_t o = 4; o < 6; o++)
    {
        value = (rxCmd[o] << 8) | rxCmd[o];
    }

    reData = (value & 0xFF) << 8 | (value >> 8 & 0xFF);

    return reData;
}

int16_t B3MSC1170A::readCurrent(byte id)
{
    byte txCmd[7];
    byte rxCmd[7];
    int16_t value;
    int16_t reData;
    bool flag;

    txCmd[0] = (byte)(0x07); // SIZE
    txCmd[1] = (byte)(0x03); // COMMAND
    txCmd[2] = (byte)(0x00); // OPTION
    txCmd[3] = (byte)(id);   // ID

    txCmd[4] = (byte)(0x48); // ADDRESS  //現在負荷電流値のアドレス
    txCmd[5] = (byte)(0x02); // LENGTH

    txCmd[6] = 0x00; // SUM

    for (int i = 0; i < 6; i++)
    {
        txCmd[6] += txCmd[i];
    }

    txCmd[6] = (byte)(txCmd[6]);
    flag = _b3msc1170a->synchronize(txCmd, 7, rxCmd, 7);

    if (flag == false)
    {
        return -1;
    }

    for (int o = 4; o < 6; o++)
    {
        value = (rxCmd[o] << 8) | rxCmd[o];
    }

    reData = (value & 0xFF) << 8 | (value >> 8 & 0xFF);

    return reData;
}

int16_t B3MSC1170A::readDesiredPosition(byte id)
{
    byte txCmd[7];
    byte rxCmd[7];
    int16_t value;
    int16_t reData;
    bool flag;

    txCmd[0] = (byte)(0x07); // SIZE
    txCmd[1] = (byte)(0x03); // COMMAND
    txCmd[2] = (byte)(0x00); // OPTION
    txCmd[3] = (byte)(id);   // ID

    txCmd[4] = (byte)(0x2A); // ADDRESS //目標位置のアドレス
    txCmd[5] = (byte)(0x02); // LENGTH

    txCmd[6] = 0x00; // SUM

    for (uint8_t i = 0; i < 6; i++)
    {
        txCmd[6] += txCmd[i];
        // Serial.print(txCmd[i]);
        // Serial.print("|");
    }
    // Serial.println("");

    txCmd[6] = (byte)(txCmd[6]);
    flag = _b3msc1170a->synchronize(txCmd, 7, rxCmd, 7);

    if (flag == false)
    {
        return -1;
    }

    // Serial.print("RAM: ");
    for (uint8_t o = 4; o < 6; o++)
    {
        value = (rxCmd[o] << 8) | rxCmd[o];
        // Serial.print(rxCmd[o], HEX);
        // Serial.print(" ");
    }
    // Serial.print(" | ");

    reData = (value & 0xFF) << 8 | (value >> 8 & 0xFF);

    return reData;
}

int16_t B3MSC1170A::readMotorTemperature(byte id)
{
    byte txCmd[7];
    byte rxCmd[7];
    int16_t value;
    int16_t reData;
    bool flag;

    txCmd[0] = (byte)(0x07); // SIZE
    txCmd[1] = (byte)(0x03); // COMMAND
    txCmd[2] = (byte)(0x00); // OPTION
    txCmd[3] = (byte)(id);   // ID

    txCmd[4] = (byte)(0x46); // ADDRESS  //現在のモーター温度のアドレス
    txCmd[5] = (byte)(0x02); // LENGTH

    txCmd[6] = 0x00; // SUM

    for (int i = 0; i < 6; i++)
    {
        txCmd[6] += txCmd[i];
    }

    txCmd[6] = (byte)(txCmd[6]);
    flag = _b3msc1170a->synchronize(txCmd, 7, rxCmd, 7);

    if (flag == false)
    {
        return -1;
    }

    for (int o = 4; o < 6; o++)
    {
        value = (rxCmd[o] << 8) | rxCmd[o];
    }

    reData = (value & 0xFF) << 8 | (value >> 8 & 0xFF);

    return reData;
}

int16_t B3MSC1170A::readMcuTemperature(byte id)
{
    byte txCmd[7];
    byte rxCmd[7];
    int16_t value;
    int16_t reData;
    bool flag;

    txCmd[0] = (byte)(0x07); // SIZE
    txCmd[1] = (byte)(0x03); // COMMAND
    txCmd[2] = (byte)(0x00); // OPTION
    txCmd[3] = (byte)(id);   // ID

    txCmd[4] = (byte)(0x44); // ADDRESS  //現在のMCU温度のアドレス
    txCmd[5] = (byte)(0x02); // LENGTH

    txCmd[6] = 0x00; // SUM

    for (int i = 0; i < 6; i++)
    {
        txCmd[6] += txCmd[i];
    }

    txCmd[6] = (byte)(txCmd[6]);
    flag = _b3msc1170a->synchronize(txCmd, 7, rxCmd, 7);

    if (flag == false)
    {
        return -1;
    }

    for (int o = 4; o < 6; o++)
    {
        value = (rxCmd[o] << 8) | rxCmd[o];
    }

    reData = (value & 0xFF) << 8 | (value >> 8 & 0xFF);

    return reData;
}

int16_t B3MSC1170A::readCurrentPosition(byte id)
{
    byte txCmd[7];
    byte rxCmd[7];
    int16_t value;
    int16_t reData;
    bool flag;

    txCmd[0] = (byte)(0x07); // SIZE
    txCmd[1] = (byte)(0x03); // COMMAND
    txCmd[2] = (byte)(0x00); // OPTION
    txCmd[3] = (byte)(id);   // ID

    txCmd[4] = (byte)(0x2C); // ADDRESS  //現在位置のアドレス
    txCmd[5] = (byte)(0x02); // LENGTH

    txCmd[6] = 0x00; // SUM

    for (int i = 0; i < 6; i++)
    {
        txCmd[6] += txCmd[i];
    }

    txCmd[6] = (byte)(txCmd[6]);
    flag = _b3msc1170a->synchronize(txCmd, 7, rxCmd, 7);

    if (flag == false)
    {
        return -1;
    }

    for (int o = 4; o < 6; o++)
    {
        value = (rxCmd[o] << 8) | rxCmd[o];
    }

    reData = (value & 0xFF) << 8 | (value >> 8 & 0xFF);

    return reData;
}

int16_t B3MSC1170A::readCurrentVelosity(byte id)
{
    byte txCmd[7];
    byte rxCmd[7];
    int16_t value;
    int16_t reData;
    bool flag;

    txCmd[0] = (byte)(0x07); // SIZE
    txCmd[1] = (byte)(0x03); // COMMAND
    txCmd[2] = (byte)(0x00); // OPTION
    txCmd[3] = (byte)(id);   // ID

    txCmd[4] = (byte)(0x32); // ADDRESS  //現在速度のアドレス
    txCmd[5] = (byte)(0x02); // LENGTH

    txCmd[6] = 0x00; // SUM

    for (int i = 0; i < 6; i++)
    {
        txCmd[6] += txCmd[i];
    }

    txCmd[6] = (byte)(txCmd[6]);
    flag = _b3msc1170a->synchronize(txCmd, 7, rxCmd, 7);

    if (flag == false)
    {
        return -1;
    }

    for (int o = 4; o < 6; o++)
    {
        value = (rxCmd[o] << 8) | rxCmd[o];
    }

    reData = (value & 0xFF) << 8 | (value >> 8 & 0xFF);

    return reData;
}