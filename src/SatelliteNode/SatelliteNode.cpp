/**
 * @file SatelliteNode.cpp
 * @brief Seeed XIAO SAMD21 を用いたサテライトノードのメインプログラム
 *
 * ## 概要
 * リミットスイッチの状態を読み取り，RS485通信 (ADM2582E) を介して
 * SatelliteController2.0 に送信します。
 *
 * ## ハードウェア構成
 * - マイコン   : Seeed XIAO SAMD21
 * - RS485       : ADM2582E (RE/DE = D0, DI = D6/TX, RO = D7/RX)
 * - IOエキスパンダー: MCP23017 (SDA = D4, SCL = D5, I2Cアドレス = 0x20)
 *   - GPA0 : スイッチ入力2  (74HC14 経由)
 *   - GPA1 : スイッチ入力1  (74HC14 経由)
 *   - GPA2 : スイッチ入力0  (74HC14 経由)
 *   - GPA3 : スイッチ入力5  (74HC14 経由)
 *   - GPA4 : スイッチ入力4  (74HC14 経由)
 *   - GPA5 : スイッチ入力3  (74HC14 経由)
 *   - GPB0 : LED制御0
 *   - GPB1 : LED制御1
 *   - GPB2 : LED制御2
 *   - GPB3 : LED制御3
 *   - GPB4 : LED制御4
 *   - GPB5 : LED制御5
 * - A/D変換    : D1(A1) — 10kΩ × 2 分圧で 5V 電源電圧を計測
 *
 * ## スイッチ入力のビット定義
 * スイッチからの入力は 74HC14 (シュミットトリガインバータ) を経由するため，
 * リミットスイッチが押されると MCP23017 の対応ピンが HIGH になります。
 *
 * MCP23017 GPA ピンと論理チャンネルの対応:
 *   GPA0 → チャンネル2, GPA1 → チャンネル1, GPA2 → チャンネル0
 *   GPA3 → チャンネル5, GPA4 → チャンネル4, GPA5 → チャンネル3
 *
 * 送信パケットの limitSwitchState (uint8_t) ビット定義:
 *   Bit0 = チャンネル0 (GPA2)
 *   Bit1 = チャンネル1 (GPA1)
 *   Bit2 = チャンネル2 (GPA0)
 *   Bit3 = チャンネル3 (GPA5)
 *   Bit4 = チャンネル4 (GPA4)
 *   Bit5 = チャンネル5 (GPA3)
 */

#include "Lib_MCP23017.hpp"
#include "Lib_NeoPixel.hpp"
#include <Arduino.h>
#include <MsgPacketizer.h>
#include <TaskManager.h>
#include <Wire.h>

// ============================================================
// ピン定義 (XIAO SAMD21)
// ============================================================
/// @brief RS485 RE/DE 制御ピン (HIGH=送信, LOW=受信)
static constexpr uint8_t PIN_RS485_DERE = 0; // D0

/// @brief 5V電源電圧計測用アナログ入力ピン (10kΩ×2 分圧)
static constexpr uint8_t PIN_VOLTAGE_IN = A1; // D1/A1

/// @brief MCP23017 の I2C アドレス (A0〜A2 = GND の場合)
static constexpr uint8_t MCP23017_ADDRESS = 0x20;

/// @brief NeoPixel の制御ピン
static constexpr uint8_t RGB_LED = 20;

/// @brief NeoPixel の電源制御ピン
static constexpr uint8_t RGB_LED_EN = 21;

// ============================================================
// 通信パケット定義
// SatelliteController2.0 側の Packet 列挙体と値を合わせてください。
// ============================================================
namespace communication
{
    /// @brief 通信パケット種別 (SatelliteController2.0 の Packet enum と同値)
    enum class Packet : uint8_t
    {
        CONTROL_SYNC = 0,              ///< ランチコントローラーからの制御コマンド同期
        FEEDBACK_SYNC = 1,             ///< サテライトコントローラーからのフィードバック
        PRESSURE_SYNC = 2,             ///< 算出された圧力値 (MPa) の同期
        COM_CHECK_L_TO_S = 3,          ///< ランチ → サテライト 生存確認
        COM_CHECK_S_TO_L = 4,          ///< サテライト → ランチ 生存確認
        SENSOR_CONFIG_SYNC = 5,        ///< センサ基本設定の同期
        SENSOR_DUMMY_CURRENT_SYNC = 6, ///< ダミー電流値同期
        SENSOR_CALIB_COEFF_SYNC = 7,   ///< 校正係数同期
        SENSOR_ZERO_CALIB_REQ = 8,     ///< ゼロ点校正実行要求
        SENSOR_CURRENT_SYNC = 9,       ///< 生電流値同期

        /// @brief リミットスイッチ状態をサテライトノードから送信するパケット
        LIMIT_SWITCH_SYNC = 10, ///< ノード → ランチ 生存確認

        COM_CHECK_L_TO_N = 11, ///< ランチ → ノード 生存確認
        COM_CHECK_N_TO_L = 12, ///< ノード → ランチ 生存確認
    };

    /// @brief RS485 送信を有効化する
    inline void enableOutput()
    {
        digitalWrite(PIN_RS485_DERE, HIGH);
    }

    /// @brief RS485 受信モードに戻す
    inline void disableOutput()
    {
        digitalWrite(PIN_RS485_DERE, LOW);
    }

    /// @brief 最後に通信を受信した時刻 (タイムアウト判定用)
    unsigned long preReceivedTime = 0;

    /// @brief 通信タイムアウト時間 (ミリ秒)
    static constexpr unsigned long TIMEOUT_MS = 5000;

    Neopixel status(RGB_LED);

} // namespace communication

// ============================================================
// IOエキスパンダー
// ============================================================
namespace ioexp
{
    /// @brief MCP23017 インスタンス
    Lib_MCP23017 mcp(MCP23017_ADDRESS);

    /**
     * @brief GPA の生ビット列からチャンネル順にビットを並び替えて返す
     *
     * MCP23017 GPA と論理チャンネルの対応:
     *   GPA0 → ch2, GPA1 → ch1, GPA2 → ch0
     *   GPA3 → ch5, GPA4 → ch4, GPA5 → ch3
     *
     * @param rawGPA readPort(Port::A) の戻り値
     * @return uint8_t ビット0=ch0, ビット1=ch1, ... ビット5=ch5
     */
    uint8_t remapSwitchBits(uint8_t rawGPA)
    {
        bool ch0 = (rawGPA >> 2) & 0x01; // GPA2 → ch0
        bool ch1 = (rawGPA >> 1) & 0x01; // GPA1 → ch1
        bool ch2 = (rawGPA >> 0) & 0x01; // GPA0 → ch2
        bool ch3 = (rawGPA >> 5) & 0x01; // GPA5 → ch3
        bool ch4 = (rawGPA >> 4) & 0x01; // GPA4 → ch4
        bool ch5 = (rawGPA >> 3) & 0x01; // GPA3 → ch5

        return (ch0 << 0) | (ch1 << 1) | (ch2 << 2) |
               (ch3 << 3) | (ch4 << 4) | (ch5 << 5);
    }

} // namespace ioexp

// ============================================================
// 電源電圧計測
// ============================================================
namespace power
{
    /// @brief 分圧抵抗の上側 (kΩ) — 10kΩ 1%
    static constexpr float R_UPPER_KOHM = 10.0f;
    /// @brief 分圧抵抗の下側 (kΩ) — 10kΩ 1%
    static constexpr float R_LOWER_KOHM = 10.0f;

    /// @brief XIAO SAMD21 の ADC 基準電圧 (V)
    static constexpr float ADC_VREF = 3.3f;
    /// @brief ADC の最大カウント値 (10bit = 1023)
    /// @note  variant.h が #define ADC_RESOLUTION 12 を定義しているため，
    ///        ADC_RESOLUTION という名前は使用できません。
    static constexpr float ADC_MAX_COUNT = 1023.0f;

    /// @brief 計測した 5V 電源電圧 (V)
    float supplyVoltage_V = 0.0f;

    /**
     * @brief A/D変換で 5V 電源電圧を計測する
     *
     * ADC の読み値を分圧比で逆算して実際の電圧を求めます。
     * 分圧比 = R_LOWER / (R_UPPER + R_LOWER) = 10k / 20k = 0.5
     * よって: Vout = Vadc / 0.5 = Vadc × 2.0
     */
    void measure()
    {
        int rawAdc = analogRead(PIN_VOLTAGE_IN);
        float vAdc = (rawAdc / ADC_MAX_COUNT) * ADC_VREF;
        float dividerRatio = R_LOWER_KOHM / (R_UPPER_KOHM + R_LOWER_KOHM);
        supplyVoltage_V = vAdc / dividerRatio;

        // teleplot 用シリアル出力
        // Serial.print(">supplyVoltage_V:");
        // Serial.println(supplyVoltage_V);
    }

} // namespace power

// ============================================================
// タスク関数
// ============================================================

uint8_t currentLimitSwitchState = 0;

void updateLocalIO()
{
    // Serial.println("[DEBUG] I2C Read Start");
    uint8_t rawGPA = ioexp::mcp.readPort(Lib_MCP23017::Port::A);
    // Serial.println("[DEBUG] I2C Read End");
    uint8_t validGPA = rawGPA & 0x3F;
    currentLimitSwitchState = ioexp::remapSwitchBits(validGPA);

    uint8_t writeVal = currentLimitSwitchState & 0x3F;
    // Serial.println("[DEBUG] I2C Write Start");
    ioexp::mcp.writePort(Lib_MCP23017::Port::B, writeVal);
    // Serial.println("[DEBUG] I2C Write End");
}

/**
 * @brief LaunchControllerからの要求受信時に一括でステータスを返信する
 */
void sendReplyToLaunch()
{
    // RS485 でパケット一括送信
    communication::enableOutput();
    MsgPacketizer::send(
        Serial1,
        static_cast<uint8_t>(communication::Packet::LIMIT_SWITCH_SYNC),
        currentLimitSwitchState);
    MsgPacketizer::send(Serial1, static_cast<uint8_t>(communication::Packet::COM_CHECK_N_TO_L));
    Serial1.flush(); // 送信完了まで待機
    delay(2); // RS485トランシーバの切り替え待機
    communication::disableOutput();

    communication::status.noticedBlueBreath(500);
    Serial.println("OK");
}

static unsigned long rxPacketCount = 0;

void onComCheckReceived()
{
    communication::preReceivedTime = millis();
    rxPacketCount++;
    // 受信完了後、直ちにLaunchControllerへ状態を一括返信する
    sendReplyToLaunch();
}

// ============================================================
// setup / loop
// ============================================================

void setup()
{
    // ---- シリアル初期化 ----
    Serial.begin(115200); // USB シリアル（デバッグ用）

    // ---- NeoPixel 初期化 ----
    communication::status.init(RGB_LED_EN);

    // ---- RS485 制御ピン初期化 ----
    pinMode(PIN_RS485_DERE, OUTPUT);
    communication::disableOutput(); // 初期状態は受信モード

    // ---- RS485 ハードウェアシリアル初期化 ----
    // ★重要: pinModeはSerial1.begin()の前に呼ぶ必要があります。（後に呼ぶとUART機能が無効化されピンがGPIOになってしまいます）
    pinMode(D7, INPUT_PULLUP);
    Serial1.begin(115200);
    
    communication::preReceivedTime = millis();

    // ---- I2C / MCP23017 初期化 ----
    Wire.begin(); // D4(SDA) / D5(SCL)
#if defined(WIRE_HAS_TIMEOUT)
    Wire.setWireTimeout(3000, true); // I2Cの無限ループ（フリーズ）を防ぐ
#endif
    
    if (!ioexp::mcp.begin())
    {
        Serial.println("Error: MCP23017 not found! Check wiring.");
    }

    // GPA0〜GPA5 を入力に設定 (GPA6, GPA7 も未使用のため全ビット入力)
    ioexp::mcp.setPinMode(Lib_MCP23017::Port::A, 0xFF);
    // MCP23017 内部プルアップは無効 (74HC14 入力側の外部 10kΩ pull-up に委ねる)
    ioexp::mcp.setPullup(Lib_MCP23017::Port::A, 0x00);

    // GPB0〜GPB5 を出力に設定 (0xC0 = 0b11000000 で GPB6, GPB7 は入力)
    ioexp::mcp.setPinMode(Lib_MCP23017::Port::B, 0xC0);
    // 全 LED を初期消灯
    ioexp::mcp.writePort(Lib_MCP23017::Port::B, 0x00);

    // ---- アナログ入力初期化 ----
    analogReadResolution(10); // 10bit 分解能

    // ---- タスク登録 ----
    // 非同期送信タスクは削除し、ポーリング受信時に返信するように変更しました
    // Tasks.add(&measureVoltageTask)->startFps(5);   // 5Hz で電源電圧計測

    // ---- パケット受信コールバック登録 ----
    // MsgPacketizer::subscribe(Serial1, static_cast<uint8_t>(communication::Packet::COM_CHECK_L_TO_N), &onComCheckReceived);

    Serial.println("SatelliteNode: Initialized.");
}

void loop()
{
    // IOの更新を定期的に実行 (20Hz)
    static unsigned long lastIoUpdate = 0;
    if (millis() - lastIoUpdate > 50) {
        lastIoUpdate = millis();
        updateLocalIO();
    }

    // MsgPacketizerによるパース処理の代わりに、特定のパケットだけを手動でフィルタリングする
    // MsgPacketizerが長いパケット（LaunchController <-> SatelliteController間の通信）を
    // パースしようとして内部メモリリークや無限ループ（フリーズ）を起こすのを防ぐための専用処理です。
    static uint8_t rxBuffer[256];
    static uint8_t rxIndex = 0;

    while (Serial1.available()) {
        uint8_t b = Serial1.read();
        
        if (rxIndex < sizeof(rxBuffer)) {
            rxBuffer[rxIndex++] = b;
        }

        if (b == 0x00) { // COBSの終端
            // COM_CHECK_L_TO_N (Index 11 = 0x0B) のみを検出する
            // COBSエンコードでは通常、[0]がオーバヘッド(0x02等)、[1]がインデックス(0x0B)となる
            if (rxIndex >= 3 && rxBuffer[1] == 0x0B) {
                onComCheckReceived();
            }
            rxIndex = 0; // バッファをリセット
        }
    }

    communication::status.update();

    // 登録されたタスクを実行
    Tasks.update();

    // 動作確認用ハートビート
    static unsigned long lastHb = 0;
    if (millis() - lastHb > 2000) {
        lastHb = millis();
        Serial.println("[DEBUG] Node loop is running...");
    }
}
