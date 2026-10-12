# Raspberry Pi 4 デモモード＆ターミナルデバッグガイド

本ドキュメントでは，実機マイコン（Launch3.0 / Satellite3.0）が手元にない環境でも，Raspberry Pi 4（または開発用 PC）単体で**デモモード（シミュレータ）およびコマンドプロンプト（CLI）を用いたデバッグ手順**について解説します．

---

## 1. 概要とデモモードの仕組み

`raspi_gse_control.py` には，実機マイコンが接続されていない状態でも動作確認ができる **デモシミュレータ（--demo）** および **ターミナル直接操作 shell (--cli)** が組み込まれています．

- **デモモード (`--demo`)**:
  - 実機のシリアル接続 (`/dev/ttyUSB0`) を必要としません．
  - 圧力センサー数値（0.00 ～ 4.50 MPa）の充填・排出ダイナミクスを内部スレッドで擬似計算します．
  - 各電磁弁の指示（CMD）とフィードバック（FB）を自動追従・シミュレートします．
  - バックグラウンドで Web ダッシュボード (Port 5000) も同時に動作するため，ブラウザと CLI の両方からデバッグ可能です．

---

## 2. 起動方法

### 2.1 デモモード (実機不要・Web+CLI デバッグ)
ターミナルで以下のコマンドを実行します：

```bash
python3 tools/raspi_gse_control/raspi_gse_control.py --demo
```

### 2.2 実機接続時の CLI デバッグモード
実機マイコンが接続されている状態で CLI プロンプトを起動する場合：

```bash
python3 tools/raspi_gse_control/raspi_gse_control.py --port /dev/ttyUSB0
```

---

## 3. ターミナル CLI コマンド一覧

起動すると，ターミナル上に `GSE-Debug>` プロンプトが表示され，キーボードから直接制御コマンドを入力できます．

| コマンド | 引数例 | 説明 |
|---|---|---|
| `status` | - | 現在の圧力，セーフティ，リミットスイッチ，全バルブ状態を一覧表示 |
| `arm` | `on` / `off` | セーフティ装置を ARM (解除) / DISARM (施錠) |
| `fill` | - | 遠隔充填シーケンス開始 (圧力上昇シミュレーション開始) |
| `ignite` | - | 遠隔点火シーケンス開始 (O2, IGNITER, OPEN 作動) |
| `estop` | - | 🚨 **緊急停止 (EMERGENCY STOP)** (即座に DUMP, PURGE 開) |
| `peace` | - | ⏹️ 通常停止 (PEACEFUL STOP) |
| `valve` | `FILL 1` / `DUMP 0` | 指定電磁弁の手動トグル (`SHIFT`, `FILL`, `DUMP`, `OXYGEN`, `IGNITER`, `OPEN`, `CLOSE`, `PURGE`) |
| `limit` | `1` / `0` | リミットスイッチ ch5 の押下 / 解放シミュレーション |
| `zero` | - | ゼロ点校正リクエスト送信 |
| `help` | - | コマンドヘルプを表示 |
| `quit` | - | デバッグサーバーを終了 |

---

## 4. デバッグ実行例 (Walkthrough Example)

### ステップ 1: ステータスの確認
```text
GSE-Debug> status

------------------- [ GSE CURRENT STATUS ] -------------------
  Mode           : [DEMO MOCK MODE]
  Wireless Link  : ● OK (CONNECTED)
  Safety Armed   : 🔒 DISARMED (施錠中)
  N2O Pressure   : 0.000 MPa
  Limit Switch 5 : CLOSED (SAFE)
  Sequence State : Fill=False, Ignite=False, E-Stop=False
  Valves State   :
    - SHIFT    : CMD=OFF | FB=OFF
    - FILL     : CMD=OFF | FB=OFF
    - DUMP     : CMD=OFF | FB=OFF
    - OXYGEN   : CMD=OFF | FB=OFF
    - IGNITER  : CMD=OFF | FB=OFF
    - OPEN     : CMD=OFF | FB=OFF
    - CLOSE    : CMD=OFF | FB=OFF
    - PURGE    : CMD=OFF | FB=OFF
--------------------------------------------------------------
```

### ステップ 2: セーフティの解除 (ARM)
```text
GSE-Debug> arm on
[SIMULATOR] 🛡️ SAFETY ARM SET TO: ARMED
```

### ステップ 3: 充填シーケンスの開始
```text
GSE-Debug> fill
[SIMULATOR] ⛽ FILL SEQUENCE STARTED!
```
`status` を入力すると，N2O 圧力が 0.000 MPa から 4.500 MPa へ向けて徐々に上昇するアニメーションシミュレーションを確認できます．

### ステップ 4: 緊急停止 (EMERGENCY STOP) テスト
```text
GSE-Debug> estop
[SIMULATOR] 🚨 EMERGENCY STOP TRIGGERED!
```
緊急停止により `DUMP`, `PURGE`, `CLOSE` が即座に ON になり，圧力が 0.000 MPa へ降下することを確認できます．

---

## 5. Web ダッシュボードとの同時デバッグ

デモモード起動中，ブラウザで `http://localhost:5000` （Raspberry Pi の場合は `http://<IP>:5000`）にアクセスすると，ターミナルでのコマンド入力結果がリアルタイムでブラウザ画面上のメーターおよびバルブ LED に反映されます．
