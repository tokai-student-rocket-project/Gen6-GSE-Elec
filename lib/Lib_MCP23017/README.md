# Lib_MCP23017

MCP23017 16ビット I/O エキスパンダー用のシンプルなラッパーライブラリです。  
Arduino の `Wire` (I2C) を通じてデバイスを制御します。

---

## 対応デバイス

| デバイス | パッケージ |
|---|---|
| MCP23017 | DIP / SP など |

---

## ピン配置（本プロジェクト: XIAO SAMD21）

| 信号名 | XIAO SAMD21 ピン | 説明 |
|---|---|---|
| SDA | D4 (GPIO22) | I2C データ |
| SCL | D5 (GPIO23) | I2C クロック |

MCP23017 の A0〜A2 ピンをすべて GND に接続した場合、I2C アドレスは `0x20` になります。

---

## 使い方

### インクルード

```cpp
#include "Lib_MCP23017.hpp"
```

### 初期化

```cpp
Lib_MCP23017 mcp(0x20); // I2Cアドレス 0x20

void setup() {
    Wire.begin();
    if (!mcp.begin()) {
        Serial.println("Error: MCP23017 not found!");
    }

    // PortA を全ピン入力に設定 (0xFF = 全ビット 1 = 入力)
    mcp.setPinMode(Lib_MCP23017::Port::A, 0xFF);

    // PortA のプルアップを有効化
    mcp.setPullup(Lib_MCP23017::Port::A, 0xFF);

    // PortB を全ピン出力に設定 (0x00 = 全ビット 0 = 出力)
    mcp.setPinMode(Lib_MCP23017::Port::B, 0x00);
}
```

### 入力の読み取り

```cpp
// PortA の全ピンを一括読み取り（1バイト）
uint8_t portAValue = mcp.readPort(Lib_MCP23017::Port::A);

// GPA0 のみ読み取り
bool pin0 = mcp.readPin(Lib_MCP23017::Port::A, 0);
```

### 出力の書き込み

```cpp
// PortB の全ピンを一括書き込み
mcp.writePort(Lib_MCP23017::Port::B, 0b00110101);

// GPB2 だけ HIGH にする
mcp.writePin(Lib_MCP23017::Port::B, 2, true);
```

---

## API リファレンス

| 関数 | 説明 |
|---|---|
| `begin()` | デバイスの存在確認。`true` なら正常 |
| `setPinMode(port, direction)` | 入出力方向を設定。1=入力, 0=出力 |
| `setPullup(port, pullup)` | プルアップ有効化。1=有効, 0=無効 |
| `readPort(port)` | ポートの全ピン状態を 1バイトで返す |
| `writePort(port, value)` | ポートの全ピンに 1バイトを書き込む |
| `readPin(port, pin)` | 特定ピンの状態を `bool` で返す |
| `writePin(port, pin, value)` | 特定ピンに `bool` 値を書き込む |

---

## 注意事項

- `begin()` は `Wire.begin()` の**後**に呼び出してください。
- XIAO SAMD21 の I2C は `Wire` で D4(SDA) / D5(SCL) に対応しています。
- `setPinMode` の引数はビットマスクです。GPA0〜GPA5 を入力にしたい場合は `0x3F` (= `0b00111111`) を指定します。
