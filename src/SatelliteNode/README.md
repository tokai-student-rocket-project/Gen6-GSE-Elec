# SatelliteNode

Seeed XIAO SAMD21 を用いたサテライトノードのファームウェアです。  
リミットスイッチの状態を RS485 通信で SatelliteController2.0 に送信します。

---

## ハードウェア構成

### マイコン

| 項目 | 内容 |
|---|---|
| ボード | Seeed XIAO SAMD21 |
| フレームワーク | Arduino (PlatformIO) |
| 環境名 | `SatelliteNode` |

### ピン配置

| 機能 | ハード記述 | Arduino ピン | 説明 |
|---|---|---|---|
| RS485 RE/DE 制御 | GPIO0 | D0 | HIGH=送信, LOW=受信 |
| 5V電源電圧計測 (ADC) | GPIO1 | A1 | 10kΩ×2 分圧入力 |
| RS485 DI (TX) | GPIO16 | D6 | `Serial1` TX |
| RS485 RO (RX) | GPIO17 | D7 | `Serial1` RX |
| I2C SDA | GPIO22 | D4 | `Wire` SDA |
| I2C SCL | GPIO23 | D5 | `Wire` SCL |

### RS485 トランシーバ (ADM2582E)

| ADM2582E ピン | 接続先 |
|---|---|
| RE (Low=受信有効) | D0 (GPIO0) |
| DE (High=送信有効) | D0 (GPIO0) |
| DI | D6 / TX (GPIO16) |
| RO | D7 / RX (GPIO17) |

RE と DE は同一ピンに接続されており，HIGH で送信モード，LOW で受信モードとなります。

### IOエキスパンダー (MCP23017)

I2C アドレス: `0x20` (A0〜A2 = GND)

| MCP23017 ピン | 論理チャンネル | 方向 | 説明 |
|---|---|---|---|
| GPA0 | チャンネル2 | 入力 | リミットスイッチ入力2 (74HC14経由) |
| GPA1 | チャンネル1 | 入力 | リミットスイッチ入力1 (74HC14経由) |
| GPA2 | チャンネル0 | 入力 | リミットスイッチ入力0 (74HC14経由) |
| GPA3 | チャンネル5 | 入力 | リミットスイッチ入力5 (74HC14経由) |
| GPA4 | チャンネル4 | 入力 | リミットスイッチ入力4 (74HC14経由) |
| GPA5 | チャンネル3 | 入力 | リミットスイッチ入力3 (74HC14経由) |
| GPB0 | — | 出力 | LED制御0 (チャンネル0対応) |
| GPB1 | — | 出力 | LED制御1 (チャンネル1対応) |
| GPB2 | — | 出力 | LED制御2 (チャンネル2対応) |
| GPB3 | — | 出力 | LED制御3 (チャンネル3対応) |
| GPB4 | — | 出力 | LED制御4 (チャンネル4対応) |
| GPB5 | — | 出力 | LED制御5 (チャンネル5対応) |

> **注意**: 74HC14 はシュミットトリガインバータです。  
> リミットスイッチが押される（導通する）と 74HC14 の入力が LOW となり，出力が HIGH になります。  
> そのため，`MCP23017 GPA ピン = HIGH` のとき，対応するリミットスイッチが押されていることを意味します。

### A/D 変換（5V電源電圧計測）

```
5V 電源
  │
  ├─ 10kΩ (R1, 1%)
  │
  ├─── A1 (ADC 入力)
  │
  ├─ 10kΩ (R2, 1%)
  │
GND
```

分圧比 = R2 / (R1 + R2) = 10k / 20k = **0.5**

ADC 基準電圧は 3.3V，分解能は 10bit です。

計算式:
```
V_adc = (analogRead(A1) / 1023.0) × 3.3
V_supply = V_adc / 0.5 = V_adc × 2.0
```

---

## ソフトウェア仕様

### 送信パケット

| パケット種別 | 値 | 内容 |
|---|---|---|
| `LIMIT_SWITCH_SYNC` | 10 | リミットスイッチ状態 (uint8_t) |
| `COM_CHECK_S_TO_L` | 4 | 生存確認 |

#### LIMIT_SWITCH_SYNC ビット定義

| ビット | チャンネル | 押されているとき |
|---|---|---|
| Bit0 | チャンネル0 (GPA2) | `1` |
| Bit1 | チャンネル1 (GPA1) | `1` |
| Bit2 | チャンネル2 (GPA0) | `1` |
| Bit3 | チャンネル3 (GPA5) | `1` |
| Bit4 | チャンネル4 (GPA4) | `1` |
| Bit5 | チャンネル5 (GPA3) | `1` |

### 受信パケット

| パケット種別 | 値 | 内容 |
|---|---|---|
| `COM_CHECK_L_TO_S` | 3 | 生存確認 (受信時刻を更新) |

### タスク周期

| タスク | 周期 |
|---|---|
| リミットスイッチ状態送信 | 10 Hz |
| 5V電源電圧計測 | 5 Hz |
| 生存確認送信 | 2 Hz |

---

## ビルド方法

```bash
# PlatformIO CLI の場合
pio run -e SatelliteNode

# 書き込み
pio run -e SatelliteNode -t upload
```

> `platformio.ini` の `[env:SatelliteNode]` に以下を追加する必要があります（詳細は `SatelliteNode_bugs.md` を参照）。

---

## 依存ライブラリ

| ライブラリ | 用途 |
|---|---|
| `Lib_MCP23017` (本プロジェクト内) | MCP23017 IOエキスパンダー制御 |
| `hideakitai/MsgPacketizer` | RS485 パケット通信 |
| `hideakitai/TaskManager` | 定周期タスク管理 |
