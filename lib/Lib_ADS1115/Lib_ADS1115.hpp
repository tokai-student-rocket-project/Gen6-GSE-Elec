#pragma once

#include <Arduino.h>
#include <Adafruit_ADS1X15.h>

/**
 * @brief 初心者向けのADS1115（A/Dコンバータ）ラッパーライブラリ
 * 
 * Adafruit_ADS1X15ライブラリをより簡単に使うためのクラスです。
 */
class Lib_ADS1115 {
private:
    Adafruit_ADS1115 ads; // AdafruitのADS1115インスタンス

public:
    /**
     * @brief コンストラクタ
     */
    Lib_ADS1115();

    /**
     * @brief ADS1115を初期化します。最初に必ず呼び出してください。
     * 
     * @param i2cAddress I2Cアドレス（デフォルトは0x48。ADDRピンがGNDの場合）
     * @return true: 初期化成功 / false: 初期化失敗（配線等を確認してください）
     */
    bool init(uint8_t i2cAddress = 0x48);

    /**
     * @brief 指定したチャンネルの電圧（V）を読み取ります。
     * 
     * @param channel 読み取るチャンネル番号 (0 〜 3)
     * @return float 読み取った電圧値（ボルト）
     */
    float readVoltage(uint8_t channel);

    /**
     * @brief 指定したチャンネルの生のデジタル値を読み取ります。
     * 
     * @param channel 読み取るチャンネル番号 (0 〜 3)
     * @return int16_t 読み取ったADCの生データ
     */
    int16_t readRaw(uint8_t channel);

    /**
     * @brief 測定可能な最大の電圧範囲（ゲイン）を設定します。
     * 測定したい電圧に合わせて設定すると、より正確に測れます。
     * 
     * 0: ±6.144V (デフォルト: 広い範囲を測れるが精度は普通)
     * 1: ±4.096V
     * 2: ±2.048V
     * 3: ±1.024V
     * 4: ±0.512V
     * 5: ±0.256V (狭い範囲しか測れないが精度が高い)
     * 
     * @param rangeIndex 設定する電圧範囲の番号 (0 〜 5)
     */
    void setVoltageRange(uint8_t rangeIndex);
};
