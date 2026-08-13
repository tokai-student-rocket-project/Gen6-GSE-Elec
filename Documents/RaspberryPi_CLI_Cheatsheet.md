# Raspberry Pi 4 CLI 活用・運用チートシート (Gen6 GSE System)

本ドキュメントは，Gen6 GSE システム（Launch3.0 / Satellite3.0）の無線遠隔制御ノードとして使用する **Raspberry Pi 4 の環境構築，ネットワーク設定，遠隔制御プログラムの起動・デバッグ，systemd サービス管理，電源操作** において頻繁に使用する CLI コマンドをまとめた活用ガイドです．

---

## 1. 接続・ネットワーク疎通確認 (SSH & IP)

| コマンド | 説明 |
|---|---|
| `ssh pi@raspi-gse.local` | mDNS ホスト名指定で SSH ログイン |
| `ssh pi@192.158.x.x` | IP アドレス直接指定で SSH ログイン |
| `hostname -I` *(または `ip a`)* | Raspberry Pi に割り当てられている IP アドレスを確認 |
| `ping 192.158.x.x` | PC から Raspberry Pi へのネットワーク疎通確認 |

---

## 2. 固定 IP アドレス設定 (NetworkManager)

Raspberry Pi OS (Debian Bookworm) では NetworkManager を使用して固定 IP（`192.158.x.x` 等）を設定します．

| コマンド | 説明 |
|---|---|
| `sudo nmtui` | GUI 風の対話画面で Wi-Fi および固定 IP アドレスを設定（推奨） |
| `nmcli connection show` | 現在認識されているネットワーク接続名の一覧を表示 |
| `sudo nmcli connection modify "<接続名>" ipv4.method manual ipv4.addresses 192.158.x.x/24 ipv4.gateway 192.158.x.1` | コマンドラインで指定の IP アドレスを固定化 |
| `sudo nmcli connection up "<接続名>"` | 変更したネットワーク設定を即時適用 |

---

## 3. OS 更新・パッケージ管理 (Apt & Pip)

| コマンド | 説明 |
|---|---|
| `sudo apt update && sudo apt upgrade -y` | OS パッケージリスト更新と全パッケージの最新化 |
| `sudo apt install -y python3-pip python3-serial python3-msgpack python3-flask git` | 遠隔制御システムに必要な依存パッケージを一括インストール |
| `pip3 install -r requirements.txt --break-system-packages` | `raspi_gse_control` の Python パッケージ依存関係を適用 |

---

## 4. 制御プログラムの配置と実行 (GSE Control Server)

| コマンド | 説明 |
|---|---|
| `scp -r d:\Projects\TSRP\Gen6-GSE-Elec\tools\raspi_gse_control pi@192.158.x.x:~/` | Windows PC から Raspberry Pi へ制御プログラム一式を SCP 転送 |
| `python3 raspi_gse_control.py --demo` | **デモ / シミュレータモード** 起動（マイコン実機なしで通信・Web UI テスト可能） |
| `python3 raspi_gse_control.py --port /dev/ttyUSB0` | **実機接続モード** 起動（ATmega2560 マイコンと USB シリアル接続） |

---

## 5. ターミナル CLI デバッグコマンド (`GSE-Debug>` プロンプト内)

`raspi_gse_control.py` 起動後に画面に表示される `GSE-Debug>` 画面での直感操作コマンドです．

| コマンド | 引数例 | 説明 |
|---|---|---|
| `status` | - | 現在の圧力（MPa），セーフティ，リミットスイッチ，各弁状態（CMD/FB）一覧表示 |
| `arm` | `on` / `off` | 遠隔セーフティ装置の解除 (ARM) / 施錠 (DISARM) |
| `fill` | - | 遠隔充填シーケンス開始 |
| `ignite` | - | 遠隔点火シーケンス開始 |
| `estop` | - | 🚨 **緊急停止 (EMERGENCY STOP)**（DUMP, PURGE 開，全安全退避） |
| `peace` | - | 通常停止 (PEACEFUL STOP) |
| `valve` | `FILL 1` | 指定電磁弁の手動トグル (`SHIFT`, `FILL`, `DUMP`, `OXYGEN`, `IGNITER`, `OPEN`, `CLOSE`, `PURGE`) |
| `limit` | `1` / `0` | リミットスイッチ (ch5) の模擬押下 / 解放 |
| `zero` | - | 圧力センサーの遠隔ゼロ点校正要求 |
| `quit` | - | GSE デバッグターミナルおよびサーバーの終了 |

---

## 6. 自動起動サービス管理 (systemd)

実験場等で電源投入時にバックグラウンドで自動起動・継続稼働させるための管理コマンドです．

| コマンド | 説明 |
|---|---|
| `sudo cp raspi-gse.service /etc/systemd/system/` | サービス設定ファイルを配置 |
| `sudo systemctl daemon-reload` | systemd にサービス定義を再読み込み |
| `sudo systemctl enable raspi-gse.service` | OS 起動時の自動スタートを有効化 |
| `sudo systemctl start raspi-gse.service` | 今すぐバックグラウンドサービスを起動 |
| `sudo systemctl stop raspi-gse.service` | バックグラウンドサービスを一時停止 |
| `sudo systemctl status raspi-gse.service` | サービスの現在状態（稼働中/停止中）を表示 |
| `journalctl -u raspi-gse.service -f` | 自動起動プログラムのリアルタイム標準出力ログを監視 |

---

## 7. 電源操作 (Shutdown & Reboot)

> [!CAUTION]
> SD カードのデータ破損を防ぐため，電源プラグを抜く前に必ず安全なシャットダウンコマンドを実行してください．

| コマンド | 説明 |
|---|---|
| `sudo shutdown -h now` *(または `sudo poweroff`)* | **安全なシャットダウン**（緑色のアクセス LED 消灯を確認後に電源抜去） |
| `sudo reboot` | システムの即時再起動 |

---

## 8. 関連ドキュメントリンク

* [RaspberryPi_OS_Comparison_and_Setup.md](file:///d:/Projects/TSRP/Gen6-GSE-Elec/Documents/RaspberryPi_OS_Comparison_and_Setup.md)
* [RaspberryPi_Debug_and_Demo_Guide.md](file:///d:/Projects/TSRP/Gen6-GSE-Elec/Documents/RaspberryPi_Debug_and_Demo_Guide.md)
* [Launch3_Satellite3_Wireless_Guide.md](file:///d:/Projects/TSRP/Gen6-GSE-Elec/Documents/Launch3_Satellite3_Wireless_Guide.md)
* [raspi-gse.service](file:///d:/Projects/TSRP/Gen6-GSE-Elec/tools/raspi_gse_control/raspi-gse.service)
