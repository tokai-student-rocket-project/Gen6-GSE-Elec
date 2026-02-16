#include "Lib_SC8721.hpp"

SC8721::SC8721(TwoWire *wire) : _wire(wire) {}

bool SC8721::begin() {
  _wire->begin();
  // Simple check: write default frequency and check if device responds
  // Default code 1 (500kHz) is usually safe.
  setFrequency(1);

  // Initialize FB control
  uint8_t reg04 = readRegister(REG_VOUT_SET_LSB);
  reg04 |= (1 << 4); // FB_SEL = 1
  reg04 |= (1 << 3); // FB_ON = 1
  writeRegister(REG_VOUT_SET_LSB, reg04);
  updateLoad();

  return true;
}

bool SC8721::setVoltage(float voltage) {
  if (voltage < 2.7f || voltage > 22.0f)
    return false;

  uint16_t vout_set;
  uint8_t fb_dir = 0;

  if (voltage >= 5.0f) {
    fb_dir = 0;
    vout_set = (uint16_t)((voltage - 5.0f) / 0.02f + 0.5f);
  } else {
    fb_dir = 1;
    vout_set = (uint16_t)((5.0f - voltage) / 0.02f + 0.5f);
  }

  if (vout_set > 1023)
    vout_set = 1023;

  // MSB: bits 9-2
  uint8_t msb = (uint8_t)(vout_set >> 2);
  // LSB: bits 1-0 in bits 1-0 of Reg04
  uint8_t lsb_val = (uint8_t)(vout_set & 0x03);

  // Read current Reg04 to preserve other bits (FB_SEL, FB_ON, FB_DIR)
  uint8_t reg04 = readRegister(REG_VOUT_SET_LSB);
  reg04 &= 0xF8; // Clear FB_DIR and VOUT_SET<1:0> (bits 2, 1, 0)
  reg04 |= (fb_dir << 2);
  reg04 |= lsb_val;
  reg04 |= (1 << 4); // Ensure FB_SEL = 1
  reg04 |= (1 << 3); // Ensure FB_ON = 1

  writeRegister(REG_VOUT_SET_MSB, msb);
  writeRegister(REG_VOUT_SET_LSB, reg04);
  updateLoad();

  return true;
}

bool SC8721::setCurrentLimit(uint8_t cso_set) {
  writeRegister(REG_CSO, cso_set);
  updateLoad();
  return true;
}

bool SC8721::setEnable(bool enable) {
  uint8_t reg05 = readRegister(REG_GLOBAL_CTRL);
  if (enable) {
    reg05 &= ~(1 << 0); // STANDBY = 0 (Enable)
  } else {
    reg05 |= (1 << 0); // STANDBY = 1 (Disable)
  }
  writeRegister(REG_GLOBAL_CTRL, reg05);
  return true;
}

bool SC8721::setFrequency(uint8_t freq_code) {
  if (freq_code > 3)
    return false;
  writeRegister(REG_FREQ_SET, freq_code & 0x03);
  updateLoad();
  return true;
}

uint8_t SC8721::getStatus1() { return readRegister(REG_STATUS_1); }

uint8_t SC8721::getStatus2() { return readRegister(REG_STATUS_2); }

bool SC8721::isOverCurrent() {
  return (getStatus1() & (1 << 3)); // OCP bit
}

bool SC8721::isShortCircuit() {
  return (getStatus1() & (1 << 2)); // SCP bit
}

bool SC8721::isOverVoltage() {
  return (getStatus1() & (1 << 1)); // OVP bit
}

void SC8721::writeRegister(uint8_t reg, uint8_t val) {
  _wire->beginTransmission(I2C_ADDRESS);
  _wire->write(reg);
  _wire->write(val);
  _wire->endTransmission();
}

uint8_t SC8721::readRegister(uint8_t reg) {
  _wire->beginTransmission(I2C_ADDRESS);
  _wire->write(reg);
  _wire->endTransmission(false);
  _wire->requestFrom(I2C_ADDRESS, (uint8_t)1);
  if (_wire->available()) {
    return _wire->read();
  }
  return 0;
}

void SC8721::updateLoad() {
  // Set LOAD bit (bit 1) in REG_GLOBAL_CTRL to apply settings
  uint8_t reg05 = readRegister(REG_GLOBAL_CTRL);
  reg05 |= (1 << 1);
  writeRegister(REG_GLOBAL_CTRL, reg05);
}
