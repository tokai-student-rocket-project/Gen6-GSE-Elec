# Lib_Neopixel

このライブラリは、フルカラーLED（NeoPixel）を簡単に制御するための Arduino 用ライブラリです。
特に、XIAO シリーズなどの内蔵 RGB LED や、外付けの NeoPixel を「色」や「明るさ」で直感的に操作できるように設計されています。

## ✨ 特徴
- **簡単な初期化**: ピン番号を指定するだけで準備完了。
- **直感的な色指定**: RGB（赤・緑・青）だけでなく、HSV（色相・彩度・明度）での指定にも対応。
- **プリセット機能**: よく使う色（ピンク、緑、赤、青、白）を 1 行で光らせる関数を用意。
- **バッテリー状態表示**: 電圧値に応じて色を自動変化させる機能付き。

## 🚀 使い方

### 1. 基本的な準備
```cpp
#include "Lib_Neopixel.hpp"

// 使用するピンを指定してインスタンスを作成
// XIAO RA4M1 の内蔵LEDの場合は RGB_BUILTIN を指定
Neopixel status(RGB_BUILTIN);

void setup() {
    // 初期化（電源供給用のピンがある場合はそのピン番号を渡す）
    // 特に指定がない場合は PIN_RGB_EN などの電源ピンを指定
    status.init(PIN_RGB_EN);
}
```

### 2. 色を光らせる（プリセット）
```cpp
status.noticedPink();  // ピンク
status.noticedGreen(); // 緑
status.noticedRed();   // 赤
status.noticedBlue();  // 青
status.noticedWhite(); // 白
status.off();          // 消灯
```

### 3. HSV空間での高度な制御
「虹色にしたい」「ゆっくり明るさを変えたい」というときは `setHSV` が便利です。

```cpp
// setHSV(色相, 彩度, 明度)
// 色相(Hue): 0〜65535（0: 赤, 10922: 黄, 21845: 緑, 43690: 青）
// 彩度(Saturation): 0〜255（255が最も鮮やか）
// 明度(Value): 0〜255（明るさ）

status.setHSV(21845, 255, 128); // 鮮やかな緑で、明るさ半分
status.show(); // 忘れずに show() を呼ぶと反映されます
```

## 📚 API リファレンス

| 関数名 | 説明 |
| :--- | :--- |
| `init(uint8_t power_pin)` | LEDを初期化し、電源ピンをHIGHにします。 |
| `off()` | LEDを消灯します。 |
| `noticed[Color]()` | プリセットされた色ですぐに光らせます。 |
| `setHSV(h, s, v)` | HSV形式で色をセットします。反映には `show()` が必要です。 |
| `show()` | `setHSV` 等で設定した色を実際に反映させます。 |
| `setBatteryStatus(float voltage)` | 電圧(13.1V以上で緑、11.5V以上で橙、それ以下で赤)を表示します。 |

## 🛠 依存ライブラリ
このライブラリは内部で以下のライブラリを使用しています。
- [Adafruit NeoPixel Library](https://github.com/adafruit/Adafruit_NeoPixel)

## 📝 注意事項
`setHSV` を使用した場合は、最後に必ず `show()` を呼び出さないと LED の色は変わりません。プリセット関数（`noticedPink` 等）は内部で `show` を呼んでいるため、呼び出しは不要です。
