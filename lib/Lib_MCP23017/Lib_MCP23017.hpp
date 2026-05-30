#pragma once

#include <Arduino.h>
#include <Wire.h>

/**
 * @brief MCP23017 16ビットIOエキスパンダーのラッパーライブラリ
 *
 * I2C接続のMCP23017を簡単に扱うためのクラスです。
 * PortA (GPA0〜GPA7) と PortB (GPB0〜GPB7) の入出力を制御します。
 *
 * @note XIAO SAMD21 では Wire は D4(SDA) / D5(SCL) に対応します。
 */
class Lib_MCP23017 {
public:
    /// @brief ポートを示す列挙型
    enum class Port : uint8_t {
        A = 0, ///< GPA ポート
        B = 1  ///< GPB ポート
    };

    /**
     * @brief コンストラクタ
     * @param address I2Cアドレス (A0〜A2ピンの設定で決まる。デフォルト 0x20)
     * @param wire    使用する TwoWire インスタンス（デフォルト Wire）
     */
    Lib_MCP23017(uint8_t address = 0x20, TwoWire &wire = Wire);

    /**
     * @brief MCP23017 を初期化します。Wire.begin() の後に呼び出してください。
     * @return true: 初期化成功 / false: デバイスが見つからない
     */
    bool begin();

    /**
     * @brief 指定ポートの入出力方向を一括設定します。
     * @param port      設定するポート (Port::A or Port::B)
     * @param direction 8ビットの方向レジスタ値。1=入力, 0=出力
     */
    void setPinMode(Port port, uint8_t direction);

    /**
     * @brief 指定ポートのプルアップ抵抗を一括設定します。
     * @param port   設定するポート
     * @param pullup 8ビットのプルアップ設定値。1=有効, 0=無効
     */
    void setPullup(Port port, uint8_t pullup);

    /**
     * @brief 指定ポートの全ピンの状態を一括読み取りします（入力用）。
     * @param port 読み取るポート
     * @return uint8_t 各ビットがピンの状態 (1=HIGH, 0=LOW)
     */
    uint8_t readPort(Port port);

    /**
     * @brief 指定ポートに値を一括書き込みします（出力用）。
     * @param port  書き込むポート
     * @param value 書き込む値（各ビットがピンの状態）
     */
    void writePort(Port port, uint8_t value);

    /**
     * @brief 指定ポートの特定ピンの状態を読み取ります（入力用）。
     * @param port 読み取るポート
     * @param pin  ピン番号 (0〜7)
     * @return bool true=HIGH, false=LOW
     */
    bool readPin(Port port, uint8_t pin);

    /**
     * @brief 指定ポートの特定ピンに値を書き込みます（出力用）。
     * @param port  書き込むポート
     * @param pin   ピン番号 (0〜7)
     * @param value true=HIGH, false=LOW
     */
    void writePin(Port port, uint8_t pin, bool value);

private:
    uint8_t  _address;       ///< I2Cアドレス
    TwoWire &_wire;          ///< 使用する TwoWire インスタンス
    uint8_t  _outputLatchB;  ///< GPB の出力ラッチ状態（writePin で使用）

    // MCP23017 レジスタアドレス (IOCON.BANK=0 デフォルト)
    static constexpr uint8_t REG_IODIRA   = 0x00; ///< PortA 入出力方向
    static constexpr uint8_t REG_IODIRB   = 0x01; ///< PortB 入出力方向
    static constexpr uint8_t REG_GPPUA    = 0x0C; ///< PortA プルアップ
    static constexpr uint8_t REG_GPPUB    = 0x0D; ///< PortB プルアップ
    static constexpr uint8_t REG_GPIOA    = 0x12; ///< PortA GPIO 読み書き
    static constexpr uint8_t REG_GPIOB    = 0x13; ///< PortB GPIO 読み書き
    static constexpr uint8_t REG_OLATA    = 0x14; ///< PortA 出力ラッチ
    static constexpr uint8_t REG_OLATB    = 0x15; ///< PortB 出力ラッチ

    /**
     * @brief 指定レジスタに1バイト書き込む低レベル関数
     */
    void writeRegister(uint8_t reg, uint8_t value);

    /**
     * @brief 指定レジスタから1バイト読み取る低レベル関数
     */
    uint8_t readRegister(uint8_t reg);
};
