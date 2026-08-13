# Launch3.0 & Satellite3.0 無線通信遠隔制御システム仕様ガイド

このドキュメントでは，Gen6 GSE システムにおける **Launch3.0** および **Satellite3.0** の新環境構成と，**Raspberry Pi 4 を用いた無線通信遠隔制御アルゴリズム**およびフェールセーフ仕様を解説します．

---

## 1. システム概要とハードウェア構成

Launch3.0 / Satellite3.0 は，従来の RS485 有線通信システム（Serial1）をそのまま全維持しつつ，新たに **Raspberry Pi 4 からの無線通信を用いた遠隔制御・テレメトリ監視** に対応させた最新世代の環境です．

### マイコン通信ポート割り当て (ATmega2560)
| シリアルポート | ボーレート | 接続先・用途 |
|---|---|---|
| **`Serial` (更新)** | **115200 bps** | **USB (FT232RL) - Raspberry Pi 4 無線通信 / 遠隔制御インターフェース**（※従来の Python GUI 機能は無効化） |
| `Serial1` | 115200 bps | RS485 (LTC485) - サテライト・ノード間有線通信 |
| `Serial2` | 9600 bps | DFPlayer Mini - 音声アナウンス再生 |

---

## 2. 無線通信パケット仕様 (`MsgPacketizer`)

Raspberry Pi 4 とのパケット送信には `MsgPacketizer` ライブラリを使用します．

### パケット ID 定義 (Launch3.0)

| パケット ID | Hex / Dec | パケット名 | 送信方向 | 説明 / ペイロード |
|---|---|---|---|---|
| `0x20` | 32 | `RASPI_COMMAND` | RasPi → Launch3 | 遠隔制御コマンド (`uint8_t cmdType`, `uint8_t param`) |
| `0x21` | 33 | `RASPI_HEARTBEAT_L_TO_R` | Launch3 → RasPi | 生存確認（ハートビート送信） |
| `0x22` | 34 | `RASPI_HEARTBEAT_R_TO_L` | RasPi → Launch3 | 生存確認（ハートビート受信） |
| `0x23` | 35 | `RASPI_TELEMETRY` | Launch3 → RasPi | テレメトリ一括送信 (`cmd_state`, `fb_state`, `sequence_flag`, `pressure_MPa`, `limitSwitchState`) |
| `0x24` | 36 | `RASPI_WIRELESS_STATUS` | Launch3 → RasPi | 無線リンクステータス同期 (`uint8_t status`) |

### コマンド種別 (`cmdType`)

| 値 | コマンド名 | 説明 |
|---|---|---|
| `1` | `EMERGENCY_STOP` | 遠隔緊急停止トリガー |
| `2` | `PEACEFUL_STOP` | 遠隔通常停止トリガー |
| `3` | `FILL_START` | 遠隔充填シーケンス開始（セーフティ解除必須） |
| `4` | `IGNITION_START` | 遠隔点火シーケンス開始（セーフティ解除必須） |
| `5` | `ARM_SAFETY` | 遠隔セーフティ状態設定 (`param: 1=ARMED, 0=DISARMED`) |
| `6` | `VALVE_CONTROL` | 遠隔手動弁制御 (`param`: 各弁ビットマップ) |
| `7` | `ZERO_CALIB` | 遠隔ゼロ点校正要求 |

---

## 3. 無線化必須条件アルゴリズム (Wireless Interlock & Fail-Safe)

本環境では，「**無線通信が正常に接続・維持されていること**」をアルゴリズム実行の**必須条件**として実装しています．

### 1. 生存監視タスク (`checkWirelessTask`)
- **実行頻度**: 2Hz (0.5秒おき)
- **タイムアウト時間**: 3.0秒 (3000 ms)
- 3.0秒以上 Raspberry Pi 4 からのハートビートパケット（またはコマンド）の受信がない場合，`isWirelessConnected = false` と判定します．

### 2. 遠隔コマンド受付インターロック
- `isWirelessConnected == false` の場合，受信したあらゆる遠隔動作コマンド（弁開閉，シーケンス開始）を直ちに**拒否（REJECT）**し，エラーランプ（ERR LED）を点灯させます．

### 3. 無線断絶時のフェールセーフ（自動緊急停止）
- 自動充填シーケンス（`fillSequenceIsActive`）または自動点火シーケンス（`ignitionSequenceIsActive`）の実行中に無線リンクが途絶した場合：
  - ただちに `sequence::emergencyStop()` が自動発動します．
  - 各種バルブは直ちに安全状態（FILL閉，O2閉，点火OFF，CLOSE発動，パージON）へと遷移し，事故を未然に防止します．

---

## 4. PlatformIO でのビルドと書き込み方法

### Launch3.0 のビルド
```bash
pio run -e Launch3
```

### Satellite3.0 のビルド
```bash
pio run -e Satellite3
```
