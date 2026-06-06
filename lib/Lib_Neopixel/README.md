# Lib_Neopixel

このライブラリは、フルカラーLED（NeoPixel）を簡単に制御するための Arduino 用ライブラリです。  
特に、XIAO シリーズなどの内蔵 RGB LED や、外付けの NeoPixel を「点灯」「点滅」「ホタル（フェード）点滅」などの多様なパターンで直感的に操作できるように設計されています。

---

## ✨ 特徴
*   **簡単な初期化**: ピン番号を指定するだけで準備完了。
*   **直感的な色指定**: RGB（赤・緑・青）だけでなく、HSV（色相・彩度・明度）での指定にも対応。
*   **ノンブロッキング制御**: `delay()` を使わずに `TaskManager` などと連携して、滑らかな「点滅」や「ホタル（フェード）点滅」が動作します。
*   **バッテリー状態表示**: 電圧値に応じて緑・橙・赤へ自動的に色が変化する機能付き。

---

## 🚀 使い方

### 1. 基本的な準備（初期化とタスク登録）
点滅やフェードなどのアニメーションを動作させるには、`setup()` で NeoPixel を初期化し、定期的に LED の状態を更新する `update()` 処理をタスクスケジューラー（`TaskManager` など）に登録します。

```cpp
#include "Lib_Neopixel.hpp"
#include <TaskManager.h>

// 1. 使用するピンを指定してインスタンスを作成
// (例: XIAO RA4M1 の内蔵LED制御ピンは 20)
Neopixel status(20);

// 2. LEDの表示更新用タスクの作成
void updateStatusLedTask() {
    status.update(); // 内部の時間経過に合わせてLEDの状態を更新する関数
}

void setup() {
    // 3. 初期化（電源ピン21を供給用に指定して開始）
    status.init(21);

    // 4. 定期実行タスクに登録 (10Hz〜50Hzでの実行を推奨)
    Tasks.add(&updateStatusLedTask)->startFps(10);
}
```

> [!IMPORTANT]
> **なぜ `update()` が必要なの？**  
> `delay()` を使って点滅やフェードを作ってしまうと、LED が光っている間、マイコンが完全に「フリーズ」してしまい、スイッチの読み取りや通信処理が行えなくなります。  
> このライブラリでは、`update()` がミリ秒単位の経過時間を監視して自動で輝度を計算するため、**メインプログラムを一切止めずに滑らかな点滅を実行可能**です。

---

### 2. アニメーション表示させる（おすすめ）
メインコードのどこからでも、1行の命令を呼び出すだけでアニメーション状態に切り替えることができます。

```cpp
// 🟢 緑色で 500ms（0.5秒）間隔で点滅させる
status.noticedGreenBlink(500);

// 🟢 緑色で 2000ms（2秒）周期でホタルのようにフェード（呼吸）点滅させる
status.noticedGreenBreath(2000);

// 🔴 赤色で 300ms 間隔で高速に点滅させる (異常時など)
status.noticedRedBlink(300);

// 🔵 青色で 3000ms（3秒）周期でゆっくりホタル点滅させる (待機中など)
status.noticedBlueBreath(3000);
```

---

### 3. 通常の点灯（プリセット）
アニメーションをさせずに、特定の色で常時点灯（または消灯）させることもできます。

```cpp
status.noticedPink();  // ピンクで常時点灯
status.noticedGreen(); // 緑で常時点灯
status.noticedRed();   // 赤で常時点灯
status.noticedBlue();  // 青で常時点灯
status.noticedWhite(); // 白で常時点灯
status.off();          // 消灯
```

---

## 📚 API リファレンス

| 関数名 | 引数 | 説明 |
| :--- | :--- | :--- |
| `init(power_pin)` | `uint8_t` | LEDを初期化し、指定した電源供給ピンを起動します。 |
| `update()` | なし | LEDのアニメーション状態を時間経過に合わせて更新します（タスク内で呼び出します）。 |
| `off()` | なし | LEDを即時消灯します。 |
| `noticed[Color]()` | なし | 常時点灯モードで指定色に光らせます（Pink/Green/Red/Blue/White）。 |
| `noticed[Color]Blink(interval)` | `uint32_t` (ms) | 指定色でチカチカと点滅させます（Green/Red/Blue）。 |
| `noticed[Color]Breath(period)` | `uint32_t` (ms) | 指定色でホタルのように滑らかに明暗フェードさせます（Green/Red/Blue）。 |
| `setBatteryStatus(voltage)` | `float` | 電圧に応じて色（13.1V以上:緑、11.5V以上:橙、それ以下:赤）を自動で変化させます。 |
| `setHSV(h, s, v)` | `uint16_t`, `uint8_t`, `uint8_t` | 色相(H), 彩度(S), 明度(V)で自由な色と明るさをセットします（反映には `show()` が必要です）。 |

---

## 🛠 依存ライブラリ
このライブラリの動作には、以下のライブラリが必要です。
*   [Adafruit NeoPixel Library](https://github.com/adafruit/Adafruit_NeoPixel)
*   [hideakitai/TaskManager](https://github.com/hideakitai/TaskManager) (アニメーション更新用タスク管理)
