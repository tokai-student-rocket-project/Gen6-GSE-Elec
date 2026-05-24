#include "Lib_ADS1115.hpp"

Lib_ADS1115::Lib_ADS1115() {
    // コンストラクタでは特に何もしません
}

bool Lib_ADS1115::init(uint8_t i2cAddress) {
    // Adafruitのbegin関数を呼び出して初期化
    return ads.begin(i2cAddress);
}

float Lib_ADS1115::readVoltage(uint8_t channel) {
    // 無効なチャンネルが指定された場合は0を返す
    if (channel > 3) {
        return 0.0f; 
    }
    
    // 生のデータを読み取る
    int16_t raw = ads.readADC_SingleEnded(channel);
    
    // Adafruitの機能を使って電圧(V)に変換して返す
    return ads.computeVolts(raw);
}

int16_t Lib_ADS1115::readRaw(uint8_t channel) {
    // 無効なチャンネルが指定された場合は0を返す
    if (channel > 3) {
        return 0; 
    }
    
    return ads.readADC_SingleEnded(channel);
}

void Lib_ADS1115::setVoltageRange(uint8_t rangeIndex) {
    // 入力された番号に応じて、Adafruitライブラリのゲインを設定
    switch (rangeIndex) {
        case 0: 
            ads.setGain(GAIN_TWOTHIRDS); // ±6.144V
            break;
        case 1: 
            ads.setGain(GAIN_ONE);       // ±4.096V
            break;
        case 2: 
            ads.setGain(GAIN_TWO);       // ±2.048V
            break;
        case 3: 
            ads.setGain(GAIN_FOUR);      // ±1.024V
            break;
        case 4: 
            ads.setGain(GAIN_EIGHT);     // ±0.512V
            break;
        case 5: 
            ads.setGain(GAIN_SIXTEEN);   // ±0.256V
            break;
        default: 
            // 範囲外の数字が入力された場合は最も安全な±6.144Vにする
            ads.setGain(GAIN_TWOTHIRDS); 
            break;
    }
}
