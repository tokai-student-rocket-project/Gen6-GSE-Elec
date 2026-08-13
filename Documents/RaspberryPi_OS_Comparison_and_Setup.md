# Raspberry Pi 4 OS 選定比較と GSE 無線遠隔制御システム構築ガイド

本ドキュメントでは，Gen6 GSE システム（Launch3.0 / Satellite3.0）の無線中継・遠隔制御ノードとして使用する **Raspberry Pi 4 の OS 選定の比較検討** および **セットアップ・運用手順** について解説します．

---

## 1. Raspberry Pi 4 OS 比較検討 (Evaluation & OS Selection)

ロケット実験場（Ground Support Equipment: GSE）における無線通信・遠隔制御用途として，代表的な 4 種類の OS を比較評価しました．

| OS 名 | カーネル / ベース | メモリ/CPU負荷 | シリアル/通信安定性 | 開発・構築容易性 | 総合評価 |
|---|---|---|---|---|---|
| **Raspberry Pi OS (64-bit) Lite** | Debian Bookworm | **極めて軽量** (<150MB RAM) | **最高 (公式最適化)** | **容易 (Python/pySerial完備)** | **◎ 推奨 (最優先)** |
| **Raspberry Pi OS (64-bit) Desktop** | Debian Bookworm | 軽量 (~400MB RAM) | 最高 (公式最適化) | 容易 (GUI操作可能) | ○ 推奨 (現地画面表示用) |
| **Ubuntu Server 24.04 LTS (64-bit)** | Canonical Ubuntu | 中程度 (~350MB RAM) | 良好 | 普通 (ROS2連携時に有利) | △ 条件付き推奨 |
| **Alpine Linux (64-bit)** | Alpine / musl | 極小 (<50MB RAM) | 良好 | 困難 (musl/パッケージ制約) | × 非推薦 |
| **Raspberry Pi OS + PREEMPT_RT** | RT-Patch Linux | 低 | 高精度 (低ジッター) | 難 (カーネル自作ビルド) | △ リアルタイム特化時 |

### 選定理由と結論

1. **推奨 OS: `Raspberry Pi OS (64-bit) Lite` (Headless 運用時) または `Desktop`**
   - **理由 1 (安定性と堅牢性)**: 公式の Broadcom SoC ドライバおよび UART/USB シリアルドライバが最も最適化されており，長時間のシリアル通信および Wi-Fi AP / クライアント動作においてドロップアウトが発生しにくい．
   - **理由 2 (自動アップデート事故の防止)**: Ubuntu などで問題となるバックグラウンドでの無告自動アップデート (`unattended-upgrades` や `snapd`) による CPU/ディスク負荷のスパイクが未然に防止でき，打ち上げ実験中の通信遅延を防ぐことができる．
   - **理由 3 (エコシステム)**: Python 3, `pySerial`, `msgpack`, `Flask` などの必須ライブラリが標準でスムーズに動作し，メンテナンスが容易．

> [!TIP]
> **推奨構成**: ディスプレイを接続せず Wi-Fi / Ethernet 経由でブラウザ・スマホ等から遠隔操作する場合は **Raspberry Pi OS Lite (64-bit)** を使用してください．現地現場で直接 HDMI モニタを接続して操作する場合は **Raspberry Pi OS Desktop (64-bit)** を選定します．

---

## 2. Raspberry Pi 4 の初期セットアップ手順

### Step 1: SDカードへの OS 書き込み
1. PC に **Raspberry Pi Imager** をインストールします．
2. OS に **Raspberry Pi OS (64-bit)** (Lite または Desktop) を選択します．
3. 設定（OS customization）で以下を有効化します：
   - ホスト名: `raspi-gse`
   - ユーザー名 / パスワードの設定
   - Wi-Fi 設定（実験場のアクセスポイント SSID とパスワード）
   - **SSH を有効化**

### Step 2: 依存ライブラリのインストール
Raspberry Pi 4 に SSH または ターミナルでログインし，以下のコマンドを実行します：

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y python3-pip python3-serial python3-msgpack python3-flask
```

---

## 3. 電源投入後の操作手順 (Post Boot Step-by-Step Guide)

Raspberry Pi 4 に OS を書き込んで電源を入れた後の詳細な操作手順です．

---

### Step 1: Raspberry Pi 4 へのログイン（接続）

接続方法は以下の 2 通りがあります：

#### A. 画面・キーボードを接続している場合（直接操作）
1. Raspberry Pi 4 の Micro HDMI ポートにディスプレイ，USB ポートにキーボードを接続します．
2. 画面にログインプロンプト（またはデスクトップ画面）が表示されたら，OS 書き込み時に設定した **ユーザー名** と **パスワード** でログインします．

#### B. 画面を接続していない場合（PC から SSH 遠隔接続）
1. PC（Windows / Mac）のコマンドプロンプトまたは PowerShell を開きます．
2. 以下のコマンドを入力して SSH 接続します：
   ```bash
   ssh <設定したユーザー名>@raspi-gse.local
   # 例: ssh pi@raspi-gse.local または ssh pi@192.168.x.x
   ```
3. パスワードを求められたら，Imager で設定したパスワードを入力します．

---

### Step 2: GSE 遠隔制御プログラムの配置

PC 上のプログラム（`tools/raspi_gse_control`）を Raspberry Pi 4 に転送します．

#### 方法 A: Git を使用する場合（推奨）
Raspberry Pi のターミナルでリポジトリをクローンします：
```bash
git clone <リポジトリのURL>
cd Gen6-GSE-Elec/tools/raspi_gse_control
```

#### 方法 B: Windows から SCP でファイルを直接転送する場合
Windows の PowerShell から以下のコマンドを実行して転送します：
```powershell
scp -r d:\Projects\TSRP\Gen6-GSE-Elec\tools\raspi_gse_control <ユーザー名>@raspi-gse.local:~/
```

---

### Step 3: 依存ライブラリのインストール

Raspberry Pi のターミナルで以下を実行し，必要な Python パッケージをインストールします：

```bash
cd ~/raspi_gse_control
pip3 install -r requirements.txt --break-system-packages
```
*(※ Debian Bookworm で `pip install` がブロックされる場合は `--break-system-packages` を付与します)*

---

### Step 4: デモモード（シミュレータ）での起動とデバッグ

実機マイコンが未接続でも，以下のコマンドで**デモモード**としてプログラムを起動できます：

```bash
python3 raspi_gse_control.py --demo
```

#### 起動後の画面と操作方法:
1. ターミナルに `GSE-Debug>` というコマンド入力画面が表示されます．
2. `status` と入力してエンターを押すと，現在の模擬圧熱やバルブ状態が表示されます．
3. `arm on` -> `fill` と入力すると，充填動作の擬似シミュレーションが始まります．
4. 同時に，同じ Wi-Fi に接続された PC やスマホのブラウザから `http://raspi-gse.local:5000` （または `http://<RaspberryPiのIPアドレス>:5000`）へアクセスすると，グラフィカルな画面でもリアルタイム操作・監視が可能です．

---

## 4. 自動起動サービス (Systemd) の登録（運用時）

Raspberry Pi 4 が起動すると，自動的に シリアル通信 (115200 bps) で Launch3.0 / Satellite3.0 と接続し，Web サーバー (Port 5000) を立ち上げます．

1. スマホ，タブレット，または PC を Raspberry Pi 4 と同一の Wi-Fi / LAN に接続します．
2. ブラウザで以下の URL にアクセスします：
   ```
   http://raspi-gse.local:5000   または   http://<RaspberryPiのIPアドレス>:5000
   ```
3. リアルタイムで N2O 圧力，電磁弁フィードバック，シーケンス状態が監視でき，安全装置（ARM）の解除や遠隔充填・点火・緊急停止コマンドを送信できます．
