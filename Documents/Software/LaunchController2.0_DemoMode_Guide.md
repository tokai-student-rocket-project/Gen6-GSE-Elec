# ランチコントローラー2.0 / サテライトコントローラー2.0 デモモード実行ガイド

本ドキュメントは，`src/LaunchController2.0` に実装した **デモモード（`DEMO_MODE`）** の目的・仕組み・実行手順・注意事項をまとめたものです．

> 対象ファーム: `LaunchController2.0.cpp`
> 追加環境: `platformio.ini` の `[env:Launch2_demo]`
> 実装日: 2026-09-06

---

## 1. デモモードとは

**ランチコントローラー ＋ サテライトコントローラーを接続した状態**で，
`SatelliteNode`（リミットスイッチ）と `RocketNode` が**未接続でも**，
充填〜点火シーケンスを最後まで流せるようにするためのビルドオプションです．

展示・動作説明・GUI 確認など，実機ノード類を持ち出せない場面での使用を想定しています．

### 想定する接続構成

| 機器 | デモモード時 |
|---|---|
| ランチコントローラー2.0 | 接続（**デモファームを書き込む**） |
| サテライトコントローラー2.0 | 接続（**通常ファーム `Satellite2` のまま**） |
| SatelliteNode（リミットSW） | 未接続でよい |
| RocketNode（アンビリカル） | 未接続でよい |

---

## 2. デモモードで変わること

`#ifdef DEMO_MODE` で囲まれた以下の **SatelliteNode 依存部分のみ**が変化します．
それ以外の挙動・安全ロックは通常ビルドと完全に同一です．

| 箇所（関数） | 通常ビルド | デモモード |
|---|---|---|
| `communication::isSafetyGateMet()` | リミットSW（ch5 / `SAFETY_GATE_MASK`）が押されていること | **常に成立扱い（`return true`）** |
| `communication::onComCheckReceived()` | サテライト＋ノード両方のハートビートが新しいと通信正常ランプ点灯 | **ノードの生存は問わない**（サテライトのハートビートだけで点灯） |
| `communication::onComCheckFailed()` | ノード途絶でもフェールセーフ（シーケンス強制停止） | **`nodeTimeout` を無視** |
| 起動時 | － | シリアルに **警告バナー**を表示 |

### ★ 変わらないこと（重要）

- **ランチ⇔サテライト間の通信タイムアウト・フェールセーフ（`controllerTimeout`）はそのまま有効**．
  デモ中でもサテライトとの通信が切れれば，通常どおりシーケンスは強制停止され，通信エラーランプが点灯します．
- DUMP クローズ確認，3 人同時押し，エマージェンシーストップ等の他の安全ロックは変更なし．
- `RocketNode` は元々ブロッキング要素ではない（警告ログのみ）ため，特別な処理は追加していません．

---

## 3. 実行手順

### 3.1 サテライトコントローラー（通常どおり）

```bash
pio run -e Satellite2 -t upload
```

> サテライト側はデモ用の変更が一切ありません．普段どおりのファームを使用します．

### 3.2 ランチコントローラー（デモファームを書き込む）

```bash
pio run -e Launch2_demo -t upload
```

> `pio` が PATH に無い場合は
> `~/.platformio/penv/Scripts/pio run -e Launch2_demo -t upload`

### 3.3 起動確認

ランチコントローラーをシリアルモニタ（115200 bps）で開くと，起動時に以下のバナーが表示されればデモモードが有効です．

```
#########################################################
#   DEMO MODE ENABLED / デモモード有効                   #
#   SatelliteNode(リミットSW)/RocketNode 未接続でも      #
#   ランチ+サテライトのみでシーケンスを実行できます      #
#   ランチ<->サテライト間のフェールセーフは通常どおり有効 #
#   ※現場運用時は DEMO_MODE を無効化してビルドする事     #
#########################################################
```

### 3.4 デモの流れ

1. ランチ・サテライトを RS485 で接続して両方に通電．
2. ランチの「通信正常ランプ」が点灯することを確認（サテライトのハートビートのみで点灯）．
3. 充填シーケンス開始条件を満たして `sequence::fill()` を実行．
   - `communication::statusLamp.isHigh()` … 上記2で成立
   - `communication::isSafetyGateMet()` … デモモードでは常に `true`
4. 続けて点火シーケンス `sequence::ignition()` も同様に実行可能．

---

## 4. 通常運用（現場）ビルドへの戻し方

デモ環境は独立しているため，**通常は何もする必要がありません**．
現場では従来どおり以下を使用します．

```bash
pio run -e Launch2    -t upload
pio run -e Satellite2 -t upload
```

`[env:Launch2]` には `-D DEMO_MODE` が含まれないため，
`isSafetyGateMet()` 等はすべて通常のリミットSW判定に戻ります．

### ソース先頭の `#define` について

`LaunchController2.0.cpp` の先頭付近に手動切り替え用の行があります．

```cpp
// #define DEMO_MODE
```

- 通常はこの行を **コメントアウトのまま**にしておくこと．
- PlatformIO 環境（`Launch2_demo`）を使えばこの行を触らずにデモビルドできます．

---

## 5. 安全に関する注意

- **現場運用のファームウェアに `DEMO_MODE` を絶対に含めないこと．**
  デモモードではリミットスイッチによる安全ゲートが無効化されます．
- デモに使ったランチコントローラーは，現場投入前に必ず `Launch2`（通常環境）で再書き込みすること．
- デモモードでもランチ⇔サテライト間のフェールセーフは生きているが，
  これは「サテライトとの通信断」だけを守るものであり，リミットSWによる機械的安全は守られない．
- `platformio.ini` の `[env:Launch2_demo]` を誤って `Launch2` にリネーム・統合しないこと．

---

## 6. 関連ファイル

| ファイル | 内容 |
|---|---|
| `src/LaunchController2.0/LaunchController2.0.cpp` | `#ifdef DEMO_MODE` ガード（先頭コメント，`isSafetyGateMet`，`onComCheckReceived`，`onComCheckFailed`，`setup` バナー） |
| `platformio.ini` → `[env:Launch2_demo]` | `extends = env:Launch2` ＋ `-D DEMO_MODE` |

### ビルド確認結果（2026-09-06）

| 環境 | 結果 | Flash | RAM |
|---|---|---|---|
| `Launch2`（通常） | SUCCESS | 変化なし | 変化なし |
| `Satellite2`（通常） | SUCCESS | 変化なし | 変化なし |
| `Launch2_demo`（デモ） | SUCCESS | 17.8%（46574 B） | 33.0%（2700 B） |
