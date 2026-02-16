#pragma once

#include <Arduino.h>
#include <Wire.h>

/**
 * @brief SC8721 Buck-Boost Converter Library
 *
 * Supports dynamic voltage control (2.7V - 22V) and current limit settings.
 */
class SC8721 {
public:
  static const uint8_t I2C_ADDRESS = 0x62;

  // Registers
  static const uint8_t REG_CSO = 0x01;
  static const uint8_t REG_SLOPE_COMP = 0x02;
  static const uint8_t REG_VOUT_SET_MSB = 0x03;
  static const uint8_t REG_VOUT_SET_LSB = 0x04;
  static const uint8_t REG_GLOBAL_CTRL = 0x05;
  static const uint8_t REG_SYS_SET = 0x06;
  static const uint8_t REG_FREQ_SET = 0x08;
  static const uint8_t REG_STATUS_1 = 0x09;
  static const uint8_t REG_STATUS_2 = 0x0A;

  SC8721(TwoWire *wire = &Wire);

  bool begin();

  /**
   * @brief Set output voltage
   * @param voltage Voltage in Volts (2.7 to 22.0)
   * @return true if success
   */
  bool setVoltage(float voltage);

  /**
   * @brief Set output current limit
   * @param cso_set CSO_SET value (0-255). Refer to datasheet for formula.
   * @return true if success
   */
  bool setCurrentLimit(uint8_t cso_set);

  /**
   * @brief Enable or disable DCDC converter
   * @param enable true to enable, false to disable (standby)
   */
  bool setEnable(bool enable);

  /**
   * @brief Set switching frequency
   * @param freq_code 0: 260kHz, 1: 500kHz, 2: 720kHz, 3: 920kHz
   */
  bool setFrequency(uint8_t freq_code);

  uint8_t getStatus1();
  uint8_t getStatus2();

  bool isOverCurrent();
  bool isShortCircuit();
  bool isOverVoltage();

private:
  TwoWire *_wire;

  void writeRegister(uint8_t reg, uint8_t val);
  uint8_t readRegister(uint8_t reg);
  void updateLoad();
};
