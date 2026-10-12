# Documents (ドキュメント一覧)

本ディレクトリは、第6世代 Ground Support Equipment (Gen6-GSE) の設計資料、運用マニュアル、ソフトウェア仕様、開発ドキュメントを管理しています。

---

## 📁 ディレクトリ構成

```text
Documents/
├── README.md                      # 本ドキュメント（目次・総合案内）
├── Manuals/                       # システム仕様・電装運用マニュアル
├── Software/                      # 通信プロトコル・制御シーケンス・ライブラリ仕様
├── Sensor/                        # 圧力センサ（VESIM10）マニュアル・較正シート
├── RaspberryPi/                   # Raspberry Pi 運用・CLI操作・セットアップ
├── Extensions/                    # 将来拡張（ロケット子機追加・無線化構想）
├── Development/                   # ハードウェア開発資料（回路図・データシート・図面・パーツリスト）
├── Pictures/                      # 外観写真・安全審査写真・ギャラリー
├── Poster/                        # ポスター発表用資材
└── QR/                            # プロジェクト用QRコード
```

---

## 📚 ドキュメント一覧

### 1. [Manuals/](./Manuals/) - システム仕様・運用マニュアル
- **[Specification.md](./Manuals/Specification.md)**: GSEシステム仕様書（ランチコントローラー／サテライトコントローラーの外形、電源、端子配置、部品仕様）
- **[Gen6GSEManual-Elec.md](./Manuals/Gen6GSEManual-Elec.md)**: GSE電装マニュアル（現場配線工程、電源投入手順、チェックリスト）

### 2. [Software/](./Software/) - 通信・制御ソフトウェア仕様
- **[AutoSequence.md](./Software/AutoSequence.md)**: 自動シークエンスタイミングの考え方（充填・点火シーケンスのステートマシン設計と解説）
- **[Communication_Guide.md](./Software/Communication_Guide.md)**: 通信システム解説ガイド（Satellite ↔ Launch 間のRS485/MsgPacketizer通信概要）
- **[MsgPacketizer_Spec.md](./Software/MsgPacketizer_Spec.md)**: MsgPacketizer プロトコル仕様とトラブルシューティング（COBS, MsgPack, CRC8, ビルドフラグ）
- **[library_guide.md](./Software/library_guide.md)**: 通信・タスク管理ライブラリについて（MsgPacketizer / TaskManager のAVR環境での制約と対処法）
- **[LaunchController2.0_DemoMode_Guide.md](./Software/LaunchController2.0_DemoMode_Guide.md)**: ランチコントローラー2.0 デモモード解説

### 3. [Sensor/](./Sensor/) - 圧力計測
- **[VESIM10_Manual.md](./Sensor/VESIM10_Manual.md)**: 亜酸化窒素 (N2O) 圧力計測マニュアル（VESIM10 圧力センサの配線・ADS1115 ADC接続・計測手順）
- **[VESIM10_Calibration.xlsx](./Sensor/VESIM10_Calibration.xlsx)**: 圧力センサ較正シート（電流値 mA から圧力 MPa への換算データ）

### 4. [RaspberryPi/](./RaspberryPi/) - Raspberry Pi 運用・設定
- **[RaspberryPi_CLI_Cheatsheet.md](./RaspberryPi/RaspberryPi_CLI_Cheatsheet.md)**: Raspberry Pi CLI / CUI 運用チートシート（コマンド一覧・systemd・ログ監視）
- **[RaspberryPi_Debug_and_Demo_Guide.md](./RaspberryPi/RaspberryPi_Debug_and_Demo_Guide.md)**: Raspberry Pi 実機デバッグ＆デモモード運用ガイド
- **[RaspberryPi_OS_Comparison_and_Setup.md](./RaspberryPi/RaspberryPi_OS_Comparison_and_Setup.md)**: OS選定（Bookworm Lite vs Desktop）とヘッドレス環境構築手順

### 5. [Extensions/](./Extensions/) - 将来拡張・ロードマップ
- **[rocket_node_addition_guide.md](./Extensions/rocket_node_addition_guide.md)**: ロケット搭載用子機（RocketNode）追加手順書
- **[Launch3_Satellite3_Wireless_Guide.md](./Extensions/Launch3_Satellite3_Wireless_Guide.md)**: GSE 3.0 無線化・システム構成拡張ガイド

### 6. [Development/](./Development/) - ハードウェア開発資料
- **[PartsList.xlsx](./Development/PartsList.xlsx)**: GSE構成部品パーツリスト (BOM)
- **[Catalog/](./Development/Catalog/)**: 採用部品・コネクタのメーカーカタログPDF
- **[CircuitDiagram/](./Development/CircuitDiagram/)**: 回路図リビジョン履歴PDF
- **[DataFlowDiagram/](./Development/DataFlowDiagram/)**: データフロー図（drawio / png）
- **[DateSheet/](./Development/DateSheet/)**: 主要IC・マイコン・素子のデータシートPDF
- **[Drawing/](./Development/Drawing/)**: 加工図面・ケース寸法図PDF
- **[WireingDiagram/](./Development/WireingDiagram/)**: 配線システム図（drawio）

### 7. メディア・資材
- **[Pictures/](./Pictures/)**: 機体・機材写真、安全審査資料画像、[ギャラリー](./Pictures/Gallery/Gallery.md)
- **[Poster/](./Poster/)**: ポスター資料
- **[QR/](./QR/)**: プロジェクトリンク用QRコード
