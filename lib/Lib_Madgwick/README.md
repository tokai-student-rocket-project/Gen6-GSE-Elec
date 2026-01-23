# Lib_Madgwick

Madgwickフィルタアルゴリズムを実装したArduinoライブラリです。
加速度、ジャイロ、磁気センサのデータを融合（センサフュージョン）し、ドリフトの少ない安定した姿勢（ロール、ピッチ、ヨー）を推定します。

## 概要

このライブラリは、Sebastian Madgwick氏によって開発された効率的な姿勢推定アルゴリズムを提供します。
計算負荷が比較的軽いのが特徴です。

## 特徴

- **6軸フュージョン (IMU)**: 加速度とジャイロのみを使用 (ヨー角はドリフトします)
- **9軸フュージョン (AHRS)**: 加速度、ジャイロ、磁気を使用 (ヨー角のドリフトを補正)
- **クォータニオン出力**: ジンバルロックのない姿勢表現
- **オイラー角変換**: Roll, Pitch, Yaw への変換関数を内蔵
- **重力分離**: 重力ベクトルと線形加速度（重力を除いた加速度）の分離が可能
- **ジャイロバイアス補正**: 9軸フュージョン時に、加速度・磁気情報を用いてジャイロのドリフト（バイアス）を自動推定・補正する機能（Zetaパラメータ）

## 使用方法

### 1. インスタンスの作成

```cpp
#include "Lib_Madgwick.hpp"

Madgwick filter;
```

### 2. 初期化

`setup()` 内でサンプリング周波数を設定します。
この周波数は、`update()` 関数を呼び出す頻度と一致させる必要があります。

```cpp
void setup() {
    // 100Hzで更新する場合
    filter.begin(100.0f);
}
```

### 3. フィルタの更新

`loop()` 内でセンサデータを取得し、フィルタを更新します。
更新頻度（dt）が一定になるように制御してください。

#### 9軸 (AHRS) モード - 推奨
磁気センサがある場合に使用します。ヨー角のドリフトを補正できます。

```cpp
// gx, gy, gz: ジャイロ (dps)
// ax, ay, az: 加速度 (G または m/s^2)
// mx, my, mz: 磁気 (uT)
filter.update(gx, gy, gz, ax, ay, az, mx, my, mz);
```

#### 6軸 (IMU) モード
磁気センサがない、または磁気環境が悪い場合に使用します。

```cpp
filter.updateIMU(gx, gy, gz, ax, ay, az);
```

### 4. 姿勢データの取得

更新後、オイラー角を取得できます。単位は度 (degree) です。

```cpp
float roll = filter.getRoll();
float pitch = filter.getPitch();
float yaw = filter.getYaw();
```

## API リファレンス

### `void begin(float sampleFrequency)`
フィルタを初期化します。
- `sampleFrequency`: フィルタの更新周波数 (Hz)。例: `100.0f`

### `void update(float gx, float gy, float gz, float ax, float ay, float az, float mx, float my, float mz)`
9軸データを用いてフィルタを更新します (AHRS)。
- `gx, gy, gz`: 角速度 (dps: degree per second)
- `ax, ay, az`: 加速度 (単位は任意だが、正規化されるため比率が重要)
- `mx, my, mz`: 磁束密度 (単位は任意)

### `void updateIMU(float gx, float gy, float gz, float ax, float ay, float az)`
6軸データを用いてフィルタを更新します (IMU)。磁気を使わないため、ヨー角は時間とともにドリフトします。

### `float getRoll()`
ロール角 (X軸周りの回転) を取得します。範囲: -180 ~ +180 度。

### `float getPitch()`
ピッチ角 (Y軸周りの回転) を取得します。範囲: -90 ~ +90 度。

### `float getYaw()`
ヨー角 (Z軸周りの回転) を取得します。範囲: -180 ~ +180 度 (磁気使用時は絶対方位)。

### `void getGravity(float *gx, float *gy, float *gz)`
推定された重力ベクトル（センサ座標系）を取得します。単位は 1G = 1.0 です。

### `void getLinearAcceleration(float ax, float ay, float az, float *lin_ax, float *lin_ay, float *lin_az)`
生の加速度から重力成分を除去した、線形加速度（運動加速度）を取得します。
- `ax, ay, az`: 生の加速度データ (G単位推奨)
- `lin_ax, lin_ay, lin_az`: 出力される線形加速度

### `void setZeta(float zeta)`
ジャイロバイアス補正のゲインを設定します。
- `zeta`: 補正ゲイン。`0.0f` で無効（デフォルト）。推奨値は非常に小さい値（例: `0.001f` など）から調整してください。大きくしすぎると不安定になります。
*注意: この機能は `update()` (9軸モード) を使用している場合のみ有効です。*

## 統合サンプルコード (with Lib_MPU9250)

```cpp
#include <Arduino.h>
#include "Lib_MPU9250.hpp"
#include "Lib_Madgwick.hpp"

MPU9250 mpu;
Madgwick filter;

const float SAMPLE_RATE = 100.0f; // 100Hz
const unsigned long SAMPLE_INTERVAL_MS = 1000 / SAMPLE_RATE;
unsigned long lastUpdate = 0;

void setup() {
    Serial.begin(115200);
    Wire.begin();
    
    if (!mpu.begin()) {
        Serial.println("MPU9250 init failed");
        while(1);
    }
    
    // フィルタ初期化
    filter.begin(SAMPLE_RATE);
    
    // 必要に応じてキャリブレーション値を設定
    // mpu.setAccelCalibration(...);
    // mpu.setGyroBias(...);
    // mpu.setMagCalibration(...);
}

void loop() {
    unsigned long now = millis();
    if (now - lastUpdate >= SAMPLE_INTERVAL_MS) {
        lastUpdate = now;
        
        float ax, ay, az;
        float gx, gy, gz;
        float mx, my, mz;
        
        mpu.getAcceleration(&ax, &ay, &az);
        mpu.getGyroscope(&gx, &gy, &gz);
        mpu.getMagnetometer(&mx, &my, &mz);
        
        // 9軸フュージョン
        filter.update(gx, gy, gz, ax, ay, az, mx, my, mz);
        
        float roll = filter.getRoll();
        float pitch = filter.getPitch();
        float yaw = filter.getYaw();
        
        Serial.print("Orientation: ");
        Serial.print(roll); Serial.print(", ");
        Serial.print(pitch); Serial.print(", ");
        Serial.println(yaw);
    }
}
```
# Lib_Madgwick

Madgwickフィルタアルゴリズムを実装したArduinoライブラリです。
加速度、ジャイロ、磁気センサのデータを融合（センサフュージョン）し、ドリフトの少ない安定した姿勢（ロール、ピッチ、ヨー）を推定します。

## 概要

このライブラリは、Sebastian Madgwick氏によって開発された効率的な姿勢推定アルゴリズムを提供します。
計算負荷が比較的軽いのが特徴です。

## 特徴

- **6軸フュージョン (IMU)**: 加速度とジャイロのみを使用 (ヨー角はドリフトします)
- **9軸フュージョン (AHRS)**: 加速度、ジャイロ、磁気を使用 (ヨー角のドリフトを補正)
- **クォータニオン出力**: ジンバルロックのない姿勢表現
- **オイラー角変換**: Roll, Pitch, Yaw への変換関数を内蔵
- **重力分離**: 重力ベクトルと線形加速度（重力を除いた加速度）の分離が可能
- **ジャイロバイアス補正**: 9軸フュージョン時に、加速度・磁気情報を用いてジャイロのドリフト（バイアス）を自動推定・補正する機能（Zetaパラメータ）

## 使用方法

### 1. インスタンスの作成

```cpp
#include "Lib_Madgwick.hpp"

Madgwick filter;
```

### 2. 初期化

`setup()` 内でサンプリング周波数を設定します。
この周波数は、`update()` 関数を呼び出す頻度と一致させる必要があります。

```cpp
void setup() {
    // 100Hzで更新する場合
    filter.begin(100.0f);
}
```

### 3. フィルタの更新

`loop()` 内でセンサデータを取得し、フィルタを更新します。
更新頻度（dt）が一定になるように制御してください。

#### 9軸 (AHRS) モード - 推奨
磁気センサがある場合に使用します。ヨー角のドリフトを補正できます。

```cpp
// gx, gy, gz: ジャイロ (dps)
// ax, ay, az: 加速度 (G または m/s^2)
// mx, my, mz: 磁気 (uT)
filter.update(gx, gy, gz, ax, ay, az, mx, my, mz);
```

#### 6軸 (IMU) モード
磁気センサがない、または磁気環境が悪い場合に使用します。

```cpp
filter.updateIMU(gx, gy, gz, ax, ay, az);
```

### 4. 姿勢データの取得

更新後、オイラー角を取得できます。単位は度 (degree) です。

```cpp
float roll = filter.getRoll();
float pitch = filter.getPitch();
float yaw = filter.getYaw();
```

## API リファレンス

### `void begin(float sampleFrequency)`
フィルタを初期化します。
- `sampleFrequency`: フィルタの更新周波数 (Hz)。例: `100.0f`

### `void update(float gx, float gy, float gz, float ax, float ay, float az, float mx, float my, float mz)`
9軸データを用いてフィルタを更新します (AHRS)。
- `gx, gy, gz`: 角速度 (dps: degree per second)
- `ax, ay, az`: 加速度 (単位は任意だが、正規化されるため比率が重要)
- `mx, my, mz`: 磁束密度 (単位は任意)

### `void updateIMU(float gx, float gy, float gz, float ax, float ay, float az)`
6軸データを用いてフィルタを更新します (IMU)。磁気を使わないため、ヨー角は時間とともにドリフトします。

### `float getRoll()`
ロール角 (X軸周りの回転) を取得します。範囲: -180 ~ +180 度。

### `float getPitch()`
ピッチ角 (Y軸周りの回転) を取得します。範囲: -90 ~ +90 度。

### `float getYaw()`
ヨー角 (Z軸周りの回転) を取得します。範囲: -180 ~ +180 度 (磁気使用時は絶対方位)。

### `void getGravity(float *gx, float *gy, float *gz)`
推定された重力ベクトル（センサ座標系）を取得します。単位は 1G = 1.0 です。

### `void getLinearAcceleration(float ax, float ay, float az, float *lin_ax, float *lin_ay, float *lin_az)`
生の加速度から重力成分を除去した、線形加速度（運動加速度）を取得します。
- `ax, ay, az`: 生の加速度データ (G単位推奨)
- `lin_ax, lin_ay, lin_az`: 出力される線形加速度

### `void setZeta(float zeta)`
ジャイロバイアス補正のゲインを設定します。
- `zeta`: 補正ゲイン。`0.0f` で無効（デフォルト）。推奨値は非常に小さい値（例: `0.001f` など）から調整してください。大きくしすぎると不安定になります。
*注意: この機能は `update()` (9軸モード) を使用している場合のみ有効です。*

## 統合サンプルコード (with Lib_MPU9250)

```cpp
#include <Arduino.h>
#include "Lib_MPU9250.hpp"
#include "Lib_Madgwick.hpp"

MPU9250 mpu;
Madgwick filter;

const float SAMPLE_RATE = 100.0f; // 100Hz
const unsigned long SAMPLE_INTERVAL_MS = 1000 / SAMPLE_RATE;
unsigned long lastUpdate = 0;

void setup() {
    Serial.begin(115200);
    Wire.begin();
    
    if (!mpu.begin()) {
        Serial.println("MPU9250 init failed");
        while(1);
    }
    
    // フィルタ初期化
    filter.begin(SAMPLE_RATE);
    
    // 必要に応じてキャリブレーション値を設定
    // mpu.setAccelCalibration(...);
    // mpu.setGyroBias(...);
    // mpu.setMagCalibration(...);
}

void loop() {
    unsigned long now = millis();
    if (now - lastUpdate >= SAMPLE_INTERVAL_MS) {
        lastUpdate = now;
        
        float ax, ay, az;
        float gx, gy, gz;
        float mx, my, mz;
        
        mpu.getAcceleration(&ax, &ay, &az);
        mpu.getGyroscope(&gx, &gy, &gz);
        mpu.getMagnetometer(&mx, &my, &mz);
        
        // 9軸フュージョン
        filter.update(gx, gy, gz, ax, ay, az, mx, my, mz);
        
        float roll = filter.getRoll();
        float pitch = filter.getPitch();
        float yaw = filter.getYaw();
        
        Serial.print("Orientation: ");
        Serial.print(roll); Serial.print(", ");
        Serial.print(pitch); Serial.print(", ");
        Serial.println(yaw);
    }
}
```

## パラメータ調整

### Beta (ゲイン)
`Lib_Madgwick.cpp` 内の `#define BETA_DEF 0.1f` で定義されています。
- **値を大きくする**: センサの応答が速くなりますが、ノイズの影響を受けやすくなります。
- **値を小さくする**: ノイズに強くなりますが、急激な動きへの追従が遅れます。
- デフォルトの `0.1f` はバランスの取れた値ですが、用途に応じて調整してください。

### Zeta (ジャイロバイアス補正ゲイン)
`setZeta()` 関数で設定します。デフォルトは `0.0f` (無効) です。
- ジャイロセンサのオフセット（バイアス）が時間とともに変動するのを、加速度・磁気センサの情報を使って補正します。
- **有効にする場合**: `0.001f` 程度の非常に小さい値から試してください。
- 補正が強すぎると、姿勢が振動したり発散したりする原因になります。
