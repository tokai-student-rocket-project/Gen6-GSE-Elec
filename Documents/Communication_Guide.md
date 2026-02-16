# 通信システム解説ガイド (Satellite ↔ Launch)

このドキュメントでは、サテライトコントローラー（Satellite）で取得した圧力センサの電流値を、ランチコントローラー（Launch）へ送信する仕組みを初心者向けに解説します。

## 1. 使用している主要ライブラリ

### MsgPacketizer
- **役割**: データの「梱包」と「開封」。
- **特徴**: 複雑なバイナリデータを、ひとまとめの「パケット」にして送受信します。データの壊れ（化け）をチェックする機能も備えています。

### TaskManager
- **役割**: 「いつ、どのくらいの頻度で」処理をするかの管理。
- **特徴**: `loop()` の中で複雑なタイマー処理を書かなくても、「1秒間に2回（2Hz）送る」といったスケジュール管理が簡単にできます。

---

## 2. データの流れ（電流値の例）

### ステップ1：機体側でデータを送る (SatelliteController.cpp)

1.  **パケットの定義**: `Packet` という列挙型（リスト）に `SENSOR_CURRENT_SYNC` という名前を追加しています。これがデータの種類（ID）になります。
2.  **送信関数の作成**: `sendCurrentSync()` という関数の中で、`vesim10.getCurrent_mA()` を呼んで現在の電流を取得し、`MsgPacketizer::send()` を使って送信します。
3.  **スケジュールの登録**: `setup()` 内で「1秒間に2回、この関数を呼んでね」と `TaskManager` に頼んでいます。

```cpp
// 2Hz (1秒間に2回) で実行
Tasks.add(&communication::sendCurrentSync)->startFps(2);
```

### ステップ2：通信経路
RS485 という通信規格（Serial1）を通って、デジタル信号が地上へ届きます。

### ステップ3：地上側でデータを受け取る (LaunchController.cpp)

1.  **待ち受け（講読）の設定**: `setup()` 内で「`SENSOR_CURRENT_SYNC` というIDのデータが届いたら、この関数を動かしてね」という予約をしています（これを `subscribe` と呼びます）。
2.  **ハンドラ関数の実行**: データが届くと `onCurrentSyncReceived()` が自動的に実行されます。
3.  **表示**: 届いたデータ（`current_mA`）を、デバッグ用の黒い画面（シリアルモニタ）に出力します。

```cpp
void communication::onCurrentSyncReceived(float current_mA) {
  Serial.print(">VESIM10 Current: ");
  Serial.print(current_mA);
  Serial.println(" mA");
}
```

---

## 3. なぜこの仕組みなのか？

- **安全のため**: 実験中に機体に近づかなくても、遠隔で数値が確認できます。
- **効率のため**: 無闇に高速で送り続けると通信がパンクするため、`TaskManager` で適切な頻度（2Hz）に制限しています。
- **拡張性のため**: 新しく「温度」や「電圧」も送りたくなった場合、同じ仕組み（IDを増やして `send` と `subscribe` を書く）で簡単に追加できます。
