# 自動シークエンスタイミングの考え方

> [!NOTE]
> 本資料は **LaunchController2.0** (`src/LaunchController2.0/LaunchController2.0.cpp`) の実装に基づく．
> SatelliteController2.0 側にはシークエンスのタイミングを持つ処理はなく，LaunchController2.0 からの指令に従って電磁弁を動かすだけである（[SatelliteController2.0 の役割](#satellitecontroller20-の役割)を参照）．

## 目次

- [基本的な考え方](#基本的な考え方)
- [シークエンス全体の流れ](#シークエンス全体の流れ)
- [プログラムの読み方](#プログラムの読み方)
- [fill 関数（充填シークエンス）](#fill-関数充填シークエンス)
- [ignition 関数（点火シークエンス）](#ignition-関数点火シークエンス)
- [実行タイミングの考え方](#実行タイミングの考え方)
- [シークエンスの中断](#シークエンスの中断)
- [SatelliteController2.0 の役割](#satellitecontroller20-の役割)
- [補足](#補足)

---

## 基本的な考え方

シークエンスは **「充填シークエンス」** と **「点火シークエンス」** の2段階で進む．


| 段階   | 関数                     | きっかけ                                        | 内容                         |
| ---- | ---------------------- | ------------------------------------------- | -------------------------- |
| ① 充填 | `sequence::fill()`     | シークエンス開始スイッチを押す                             | 酸化剤（N<sub>2</sub>O）の充填を始める |
| ② 点火 | `sequence::ignition()` | 充填確認スイッチ 3 つを同時に押す（または，もう一度シークエンス開始スイッチを押す） | 酸素・イグナイターで点火し，主流路弁を開く      |


点火シークエンスは，**充填が確認された瞬間**（充填確認スイッチが押された時）を起点として進む．

> **用語**
>
> - FILL: 酸化剤（N<sub>2</sub>O）充填
> - OXYGEN: 酸素（O<sub>2</sub>）充填
> - IGNITER: イグナイター（点火器）への通電
> - OPEN: 主流路弁を開く
> - PURGE: 窒素放出による消火

## シークエンス全体の流れ

シークエンス開始スイッチを押してから，パージが終わるまでの順番は以下の通り．

1. シークエンス開始スイッチ ON → **充填シークエンス開始**（`fill()`）
2. FILL 開始（FILL 弁 開）
3. 点火の最終確認が可能になる（`canConfirm = true`）
4. 充填確認スイッチ 3 つを同時に押す → **点火シークエンス開始**（`ignition()`）
5. 酸素充填 開始
6. イグナイター 点火開始
7. FILL 停止
8. 主流路弁 開（点火）
9. 酸素充填 停止
10. イグナイター 点火停止
11. パージ 開始
12. パージ 停止

> [!IMPORTANT]
> 点火シークエンスに進むためには，以下の条件がすべて満たされている必要がある．
>
>
> | 条件                               | 確認している箇所                                  |
> | -------------------------------- | ----------------------------------------- |
> | セーフティスイッチが Armed である             | `control::handleManualTask()`             |
> | DUMP スイッチが ON（DUMP 弁を閉じた状態）である   | `control::handleManualTask()`             |
> | ランチ ⇔ サテライト間の通信が正常である            | `communication::statusLamp`               |
> | リミットスイッチによる安全ゲートが成立している          | `communication::isSafetyGateMet()`        |
> | エマストが動作していない                     | `sequence::emergencyStopSequenceIsActive` |
> | 自動の FILL が上がっている（充填シークエンスが進んでいる） | `control::fill.isAutomaticRaised()`       |
> | 手動の FILL スイッチが OFF である           | `control::fill.isManualRaised()`          |
>

## プログラムの読み方

シークエンスの動作は，TaskManager の「やることリスト」(`Tasks`) に動作を登録していく形で書かれている．

```cpp
Tasks[control::OXYGEN_START]->startOnceAfterMsec(50);
```

この1行は，主に2つの要素でできている．


| 要素                       | 意味                                    |
| ------------------------ | ------------------------------------- |
| `control::OXYGEN_START`  | 何をするか．ここでは「酸素の流路につながっている電磁弁を ON にする」  |
| `startOnceAfterMsec(50)` | いつするか．ここでは「呼び出しから 50 ms 後に 1 度だけ実行する」 |


「Tasks というやることリストに `control::OXYGEN_START` を追加し，50 ms 後に 1 度だけ実行する」というイメージである．
`startOnceAfterSec(n)` の場合は，n 秒後に 1 度だけ実行する．

`control::` で始まる名前と，その動作の対応は下記の通り．


| タスク名                     | 動作                         |
| ------------------------ | -------------------------- |
| `control::FILL_START`    | FILL 弁 開 （＋ 点火の最終確認を可能にする） |
| `control::FILL_STOP`     | FILL 弁 閉                   |
| `control::OXYGEN_START`  | 酸素 開始                      |
| `control::OXYGEN_STOP`   | 酸素 停止                      |
| `control::IGNITER_START` | イグナイター 通電開始                |
| `control::IGNITER_STOP`  | イグナイター 通電停止                |
| `control::OPEN_START`    | 主流路弁 開                     |
| `control::PURGE_START`   | パージ 開始                     |
| `control::PURGE_STOP`    | パージ 停止                     |
| `control::PLAY_MUSIC`    | 音楽再生（音声ファイル 9 番）           |


## fill 関数（充填シークエンス）

`void sequence::fill()` は，戻り値なし，引数なしの `sequence` 名前空間の関数である．
開始条件を確認したあと，次の 2 つのタスクを登録して終了する．

```cpp
mp3_play(10);                            // 音声再生
Tasks[control::PLAY_MUSIC]->startOnceAfterSec(15.0);
Tasks[control::FILL_START]->startOnceAfterSec(24.0);
```


| 呼び出しからの経過時間 | 動作                                                    |
| ----------- | ----------------------------------------------------- |
| 0 s         | 音声 10 番を再生．シークエンスランプ点灯                                |
| 15.0 s      | 音楽（9 番）を再生                                            |
| 24.0 s      | FILL 弁 開．点火の最終確認が可能になる（`sequence::canConfirm = true`） |


充填シークエンスを始めないケースは以下の通り．


| ケース                            | 動作                            |
| ------------------------------ | ----------------------------- |
| すでに充填シークエンス中                   | 何もしない（重複実行防止）                 |
| エマスト中                          | 何もしない                         |
| シークエンス開始時点で，充填確認スイッチがすでに押されている | シークエンスを中断し，エラーランプ点灯           |
| 通信が正常でない                       | シークエンスを中断                     |
| 安全ゲートが成立していない                  | シークエンスを中断．エラーランプ点灯，エラー音（13 番） |


## ignition 関数（点火シークエンス）

基本的な点火のシークエンスは `void sequence::ignition()` が担っている．戻り値なし，引数なしの `sequence` 名前空間の関数である．

### 開始条件

`ignition()` は，冒頭で下記のように開始条件を確認している．1 つでも満たさなければ，点火シークエンスには進まない．


| 条件                         | 満たさなかった場合                     |
| -------------------------- | ----------------------------- |
| すでに点火シークエンス中でない            | 何もしない（重複実行防止）                 |
| 通信が正常である                   | シークエンスを中断（`peacefulStop()`）   |
| エマスト中でない                   | 何もしない                         |
| 自動の FILL が上がっている（充填開始後である） | 何もしない                         |
| 手動の FILL が OFF である         | 何もしない                         |
| 安全ゲートが成立している               | シークエンスを中断．エラーランプ点灯，エラー音（13 番） |


### シークエンスの登録部分

開始条件を通過すると，フラグを更新し，音声を再生してから，各動作を `Tasks` に登録する．

```cpp
control::sequenceStart.setAutomaticOn();
mp3_play(4); // 0104_ignitionSequenceStart

Tasks[control::OXYGEN_START]->startOnceAfterMsec(50);  // THR-E820L で点火
Tasks[control::IGNITER_START]->startOnceAfterSec(1.0); // THR-E820L で点火

Tasks[control::FILL_STOP]->startOnceAfterSec(10.0);
Tasks[control::OPEN_START]->startOnceAfterSec(10.0);

Tasks[control::OXYGEN_STOP]->startOnceAfterSec(10.5);
Tasks[control::IGNITER_STOP]->startOnceAfterSec(10.5);

Tasks[control::PURGE_START]->startOnceAfterSec(30.5);
Tasks[control::PURGE_STOP]->startOnceAfterSec(35.5);
```

上の行から順番に見ていく．

#### 音声の再生

```cpp
mp3_play(4); // 0104_ignitionSequenceStart
```

この行はマイクロ SD カードに保存された 4 番目のファイルを指定し，音声出力している．点火のカウントダウン音声である．

#### 酸素充填・イグナイターの開始

```cpp
Tasks[control::OXYGEN_START]->startOnceAfterMsec(50);
Tasks[control::IGNITER_START]->startOnceAfterSec(1.0);
```

「Tasks というやることリストに `control::OXYGEN_START` を追加し，50 ms 後に 1 度だけ実行する」，続けて「`control::IGNITER_START` を追加し，1.0 秒後に 1 度だけ実行する」という指示である．
`ignition()` が呼ばれた直後に酸素が流れ始め，1 秒後にイグナイターへの通電が始まる．

#### FILL 停止・主流路弁 開

```cpp
Tasks[control::FILL_STOP]->startOnceAfterSec(10.0);
Tasks[control::OPEN_START]->startOnceAfterSec(10.0);
```

10.0 秒後に FILL を止め，同時に主流路弁を開く．エンジンに点火するタイミングは，この時刻である．

#### 酸素・イグナイターの停止

```cpp
Tasks[control::OXYGEN_STOP]->startOnceAfterSec(10.5);
Tasks[control::IGNITER_STOP]->startOnceAfterSec(10.5);
```

主流路弁が開いた 0.5 秒後に，酸素とイグナイターを同時に止める．

#### パージ

```cpp
Tasks[control::PURGE_START]->startOnceAfterSec(30.5);
Tasks[control::PURGE_STOP]->startOnceAfterSec(35.5);
```

燃焼終了後，30.5 秒後にパージを開始して消火し，5 秒間（35.5 秒後まで）流して停止する．

### 実行順序とシークエンスの対応

[シークエンス全体の流れ](#シークエンス全体の流れ)に戻ってみると，ここまで見てきた動作は 5. 〜 12. の項目と一致する．
`Tasks` への登録は上から下に順に書かれているが，実行されるタイミングは `startOnce...` の引数で決まる．そのため，実行順序は各引数の値の小さい順になる．

## 実行タイミングの考え方

ここまで，シークエンスとプログラムの対応関係について見てきたが，ここからはその動作がどのタイミングで実行されるのかを見ていく．

基本的には，`startOnceAfterSec()` / `startOnceAfterMsec()` の引数を見ていけばよい．引数は，**`ignition()` が呼ばれた直後からの経過時間**である．

点火の基準時刻（T）は，**主流路弁が開く時刻**，つまり下記の行の引数である 10.0 秒後とする．

```cpp
Tasks[control::OPEN_START]->startOnceAfterSec(10.0);
```

この時間を基準として，点火操作を時系列順にまとめると，どの動作がどのタイミングで実行されるか理解しやすい．

### 点火シークエンス（時系列順）


| T 基準        | `ignition()` からの経過 | 動作             | タスク             |
| -----------: | ------------------: | -------------- | --------------- |
| T − 9.95 s  | 0.05 s             | 酸素充填 開始        | `OXYGEN_START`  |
| T − 9.0 s   | 1.0 s              | イグナイター 点火開始    | `IGNITER_START` |
| **T ± 0 s** | **10.0 s**         | **FILL 停止**    | `FILL_STOP`     |
| **T ± 0 s** | **10.0 s**         | **主流路弁 開（点火）** | `OPEN_START`    |
| T + 0.5 s   | 10.5 s             | 酸素充填 停止        | `OXYGEN_STOP`   |
| T + 0.5 s   | 10.5 s             | イグナイター 点火停止    | `IGNITER_STOP`  |
| T + 20.5 s  | 30.5 s             | パージ 開始         | `PURGE_START`   |
| T + 25.5 s  | 35.5 s             | パージ 停止         | `PURGE_STOP`    |


### 充填シークエンス（時系列順）


| `fill()` からの経過 | 動作                     | タスク          |
| --------------: | ---------------------- | ------------ |
| 0 s            | 音声 10 番を再生             | －            |
| 15.0 s         | 音楽（9 番）を再生             | `PLAY_MUSIC` |
| 24.0 s         | FILL 弁 開，点火の最終確認が可能になる | `FILL_START` |


> [!NOTE]
> 点火シークエンスの引数は，音声の長さに合わせて 0.1 秒単位で調整されていたが，現在は THR-E820L 用に，酸素・イグナイターの開始を前倒ししている（`startOnceAfterSec(4.5)` / `startOnceAfterSec(6.0)` から変更）．音声とぴったり合わせるのは難しく，多少ずれる．

## シークエンスの中断

シークエンスの中断には「穏便ストップ」と「エマスト」の 2 種類がある．


|           | 穏便ストップ `sequence::peacefulStop()`       | エマスト `sequence::emergencyStop()`                         |
| --------- | --------------------------------------- | -------------------------------------------------------- |
| きっかけ      | シークエンス中にスイッチ操作，Safe に戻す，通信断，安全ゲート不成立 など | エマストスイッチ ON                                              |
| シークエンスフラグ | すべて解除                                   | 充填・点火フラグを解除し，エマストフラグを立てる                                 |
| 登録済みタスク   | **すべて停止**（パージ含む）                        | FILL / OXYGEN / IGNITER / OPEN の各タスクと音楽を停止               |
| 電磁弁       | 自動制御をすべて OFF                            | FILL・OXYGEN・IGNITER・OPEN の自動制御 OFF，CLOSE を ON，PURGE を ON |
| 音声        | 停止                                      | エマスト音声（3 番）                                              |


エマスト中は，新たな充填・点火シークエンスは開始されない．解除は，シークエンス開始スイッチを押して穏便ストップ（リセット）することで行う．

シークエンス中に，自動的に穏便ストップが発動する条件は以下の通り．

- セーフティスイッチが Safe になった
- ランチ ⇔ サテライト間の通信が 5 秒以上途絶えた
- リミットスイッチによる安全ゲートが成立しなくなった
- シークエンスの途中で，もう一度シークエンス開始スイッチが押された（確認可能な状態を除く）



## THR-E820Nのシーケンス

TSRPに所属している人のみが閲覧可能.
- [THR-E820N 点火シークエンス タイミング設定 | TSRP BOX](https://u-tokai.box.com/s/ij2yk944d7rt8ybjq47xpfkimphpgx4y)

## SatelliteController2.0 の役割

SatelliteController2.0 は，シークエンスの時間管理を行わない．LaunchController2.0 から送られてくる電磁弁の指令状態を受け取り，その通りに電磁弁を動かす．


| 項目       | 内容                                                                                  |
| -------- | ----------------------------------------------------------------------------------- |
| 安全ゲートの入力 | SatelliteNode のリミットスイッチ状態を LaunchController2.0 へ転送する（`LIMIT_SWITCH_SYNC`）           |
| フェールセーフ  | LaunchController2.0 からの通信が 5 秒（`communication::timeout`）以上途絶えた場合，**全出力を強制 OFF** にする |


通信が切れたときの動作の対応は下記の通り．


| 検出した側                  | 動作                                                              |
| ---------------------- | --------------------------------------------------------------- |
| LaunchController2.0    | 進行中のシークエンスを穏便ストップ．フィードバック LED をすべて消灯                            |
| SatelliteController2.0 | DUMP / FILL / OXYGEN / IGNITER / OPEN / CLOSE / PURGE の全出力を OFF |


## 補足

### デモモード

`LaunchController2.0.cpp` 冒頭の `DEMO_MODE` を有効にしてビルドすると，SatelliteNode（リミットスイッチ）と RocketNode が未接続の状態でも充填〜点火シークエンスを最後まで流せる．
安全ゲートが常に成立扱いになるため，**現場運用では必ず無効にすること**．詳細は [LaunchController2.0_DemoMode_Guide.md](./LaunchController2.0_DemoMode_Guide.md) を参照．

### 音声ファイル番号


| 番号  | 用途                                       |
| ---: | ---------------------------------------- |
| 3   | エマスト（`0102_emergencyStop`）               |
| 4   | 点火シークエンス開始（`0104_ignitionSequenceStart`） |
| 9   | 充填中の音楽                                   |
| 10  | 充填シークエンス開始                               |
| 11  | 起動完了                                     |
| 12  | 電源を落とすとき（キルスイッチ）                         |
| 13  | エラー音                                     |
