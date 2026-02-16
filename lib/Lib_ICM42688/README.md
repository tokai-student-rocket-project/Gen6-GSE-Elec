# Lib_ICM42688

ICM-42688-P 6軸IMU（加速度・ジャイロ）用のArduino用ライブラリです。
StrawberryLinux製 [ICM-42688 6軸センサモジュール](https://strawberry-linux.com/catalog/items?code=42688) での使用を想定しています。

## 特徴
- I2C 通信に対応
- ハードウェア SPI 通信に対応
- ソフトウェア SPI（ビットバン）通信に対応（任意のピンで SPI0 相当の動作が可能）
- 加速度 (±2g, ±4g, ±8g, ±16g) および ジャイロ (±15.625dps ～ ±2000dps) のフルスケール設定に対応
- データレート (ODR) の設定に対応
- 温度データの取得に対応

## 接続方法 (StrawberryLinux モジュール)

### I2C 接続
| モジュールピン | マイコンピン | 備考 |
| :--- | :--- | :--- |
| VCC | 3.3V | 電源 (1.8V - 3.3V) |
| GND | GND | グランド |
| SCL | SCL | I2C クロック |
| SDA | SDA | I2C データ |
| AD0 | GND / 3.3V | アドレス設定 (GND: 0x68, 3.3V: 0x69) |
| ~CS | 3.3V | I2Cモード時はHIGHに固定 |

### SPI 接続
| モジュールピン | マイコンピン | 備考 |
| :--- | :--- | :--- |
| VCC | 3.3V | 電源 (1.8V - 3.3V) |
| GND | GND | グランド |
| SCLK | SCK | SPI クロック |
| SDI | MOSI | SPI データ入力 |
| SDO | MISO | SPI データ出力 |
| ~CS | CS | チップセレクト |

## 使い方

### I2C の場合
```cpp
#include <Lib_ICM42688.hpp>

ICM42688 imu(Wire);

void setup() {
    Serial.begin(115200);
    if (!imu.begin(ICM42688_ADDR_GND)) {
        Serial.println("Failed to find ICM42688");
        while (1);
    }
}

void loop() {
    imu.readSensor();
    Serial.print("Accel: ");
    Serial.print(imu.getAccelX()); Serial.print(", ");
    Serial.print(imu.getAccelY()); Serial.print(", ");
    Serial.println(imu.getAccelZ());
    delay(100);
}
```

### SPI の場合
```cpp
#include <Lib_ICM42688.hpp>

const int CS_PIN = 10;
ICM42688 imu(CS_PIN, &SPI); // SPI インスタンスはポインタで渡します

void setup() {
    Serial.begin(115200);
    if (!imu.begin()) {
        Serial.println("Failed to find ICM42688");
        while (1);
    }
}

void loop() {
    imu.readSensor();
    // ... (データ取得)
}
```

### ソフトウェア SPI（ビットバン）の場合
XIAO RA4M1 の SPI0 ピンなどを使用する場合に便利です。
```cpp
#include <Lib_ICM42688.hpp>

// SCK, MISO, MOSI, CS の順でピンを指定
ICM42688 imu(D16, D15, D17, D18);

void setup() {
    Serial.begin(115200);
    if (!imu.begin()) {
        Serial.println("Failed to find ICM42688");
        while (1);
    }
}
```

## API リファレンス

### コンストラクタ
- `ICM42688(TwoWire &wire = Wire)`
    - I2C通信を使用する場合のコンストラクタ。
- `ICM42688(uint8_t cs, SPIClass *spi = &SPI)`
    - ハードウェア SPI 通信を使用する場合のコンストラクタ。`cs` はチップセレクトピン番号。
- `ICM42688(uint8_t sck, uint8_t miso, uint8_t mosi, uint8_t cs)`
    - ソフトウェア SPI（ビットバン）を使用する場合のコンストラクタ。

### メソッド
- `bool begin(uint8_t i2caddr = ICM42688_ADDR_GND)`
    - センサーを初期化します。I2Cの場合はアドレスを指定可能です。ソフトウェアSPIの場合はピンの初期化も行います。成功時に `true` を返します。
- `void readSensor()`
    - センサーから最新のデータを全て読み取ります。
- `float getAccelX()`, `getAccelY()`, `getAccelZ()`
    - 加速度（単位: G）を取得します。事前に `readSensor()` を呼ぶ必要があります。
- `float getGyroX()`, `getGyroY()`, `getGyroZ()`
    - 角速度（単位: deg/s）を取得します。事前に `readSensor()` を呼ぶ必要があります。
- `float getTemp()`
    - センサー温度（単位: ℃）を取得します。
- `void setAccelFS(AccelFS fs)`
    - 加速度のフルスケールを設定します。
- `void setGyroFS(GyroFS fs)`
    - ジャイロのフルスケールを設定します。
- `void setAccelODR(ODR odr)`, `void setGyroODR(ODR odr)`
    - 出力データレートを設定します。

## 注意事項
- このモジュールは 3.3V 動作です。
- SPI通信時は、通信速度を最大12MHzとしています（ハードウェアSPI時）。
