# Lib_SC8721

SC8721 Buck-Boost コンバータを I2C 経由で制御するための Arduino ライブラリです。

## 特徴

- 出力電圧の動的制御 (2.7V - 22.0V, 20mVステップ)
- 出力電流制限の設定
- DCDC の有効/無効 (Standby) 切り替え
- 各種エラーフラグ (過電流、短絡、過電圧) の取得

## 使い方

### 初期化

```cpp
#include <Lib_SC8721.hpp>

SC8721 regulator(&Wire);

void setup() {
  Wire.begin();
  if (regulator.begin()) {
    Serial.println("SC8721 Initialized");
  }
}
```

### 電圧の設定

```cpp
// 12.0V に設定
regulator.setVoltage(12.0f);

// 5.0V に設定
regulator.setVoltage(5.0f);
```

### ステータスの確認

```cpp
if (regulator.isOverCurrent()) {
  Serial.println("Warning: Over Current!");
}
```

## API リファレンス

- `bool begin()`: ライブラリを初期化し、内部リファレンス制御を有効にします。
- `bool setVoltage(float voltage)`: 出力電圧を `voltage` [V] に設定します。
- `bool setCurrentLimit(uint8_t cso_set)`: `CSO_SET` レジスタ値を設定します。
- `bool setEnable(bool enable)`: コンバータの出力を有効/無効にします。
- `bool setFrequency(uint8_t freq_code)`: スイッチング周波数を設定します (0:260k, 1:500k, 2:720k, 3:920k)。
- `bool isOverCurrent()`: 過電流が発生しているか確認します。
- `bool isShortCircuit()`: 短絡が発生しているか確認します。
- `bool isOverVoltage()`: 出力過電圧が発生しているか確認します。
