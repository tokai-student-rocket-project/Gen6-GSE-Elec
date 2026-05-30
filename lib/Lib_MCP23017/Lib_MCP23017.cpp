#include "Lib_MCP23017.hpp"

Lib_MCP23017::Lib_MCP23017(uint8_t address, TwoWire &wire)
    : _address(address), _wire(wire), _outputLatchB(0x00)
{
}

bool Lib_MCP23017::begin()
{
    // デバイスが応答するか確認
    _wire.beginTransmission(_address);
    uint8_t error = _wire.endTransmission();
    return (error == 0);
}

void Lib_MCP23017::setPinMode(Port port, uint8_t direction)
{
    uint8_t reg = (port == Port::A) ? REG_IODIRA : REG_IODIRB;
    writeRegister(reg, direction);
}

void Lib_MCP23017::setPullup(Port port, uint8_t pullup)
{
    uint8_t reg = (port == Port::A) ? REG_GPPUA : REG_GPPUB;
    writeRegister(reg, pullup);
}

uint8_t Lib_MCP23017::readPort(Port port)
{
    uint8_t reg = (port == Port::A) ? REG_GPIOA : REG_GPIOB;
    return readRegister(reg);
}

void Lib_MCP23017::writePort(Port port, uint8_t value)
{
    uint8_t reg = (port == Port::A) ? REG_OLATA : REG_OLATB;
    if (port == Port::B) {
        _outputLatchB = value;
    }
    writeRegister(reg, value);
}

bool Lib_MCP23017::readPin(Port port, uint8_t pin)
{
    if (pin > 7) return false;
    return (readPort(port) >> pin) & 0x01;
}

void Lib_MCP23017::writePin(Port port, uint8_t pin, bool value)
{
    if (pin > 7) return;

    uint8_t reg = (port == Port::A) ? REG_OLATA : REG_OLATB;

    // 現在のラッチ値を読み出して対象ビットだけ変更
    uint8_t current = readRegister(reg);
    if (value) {
        current |=  (1 << pin);
    } else {
        current &= ~(1 << pin);
    }
    writeRegister(reg, current);

    if (port == Port::B) {
        _outputLatchB = current;
    }
}

// ---- 低レベル I2C 関数 ----

void Lib_MCP23017::writeRegister(uint8_t reg, uint8_t value)
{
    _wire.beginTransmission(_address);
    _wire.write(reg);
    _wire.write(value);
    _wire.endTransmission();
}

uint8_t Lib_MCP23017::readRegister(uint8_t reg)
{
    _wire.beginTransmission(_address);
    _wire.write(reg);
    _wire.endTransmission(false); // Repeated START でアドレスポインタを保持

    _wire.requestFrom(_address, (uint8_t)1);
    if (_wire.available()) {
        return _wire.read();
    }
    return 0x00;
}
