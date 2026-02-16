#include "Lib_ICM42688.hpp"

ICM42688::ICM42688(TwoWire &wire) : _wire(&wire), _isSPI(false) {}

ICM42688::ICM42688(uint8_t cs, SPIClass *spi)
    : _spi(spi), _cs(cs), _isSPI(true), _isSoftSPI(false) {}

ICM42688::ICM42688(uint8_t sck, uint8_t miso, uint8_t mosi, uint8_t cs)
    : _sck(sck), _miso(miso), _mosi(mosi), _cs(cs), _isSPI(true),
      _isSoftSPI(true) {}

bool ICM42688::begin(uint8_t i2caddr) {
  _i2caddr = i2caddr;

  if (_isSPI) {
    pinMode(_cs, OUTPUT);
    digitalWrite(_cs, HIGH);
    if (_isSoftSPI) {
      pinMode(_sck, OUTPUT);
      digitalWrite(_sck, HIGH);
      pinMode(_mosi, OUTPUT);
      pinMode(_miso, INPUT);
    } else {
      _spi->begin();
    }
  } else {
    _wire->begin();
  }

  // Software Reset
  writeRegister(ICM42688_REG_DEVICE_CONFIG, 0x01);
  delay(10); // Wait for reset to complete

  // Check WHO_AM_I
  uint8_t whoami = readRegister(ICM42688_REG_WHO_AM_I);
  if (whoami != ICM42688_WHO_AM_I_VALUE) {
    return false;
  }

  // Set Bank 0
  writeRegister(ICM42688_REG_REG_BANK_SEL, 0x00);

  // Initial configuration: 16G, 2000DPS, 1kHz ODR
  setAccelFS(ACCEL_FS_16G);
  setGyroFS(GYRO_FS_2000DPS);
  setAccelODR(ODR_1KHZ);
  setGyroODR(ODR_1KHZ);

  // Power management: Turn on Accel and Gyro in Low Noise mode
  // PWR_MGMT0: [7:6] Reserved, [5] Temp Dis, [4] Idle, [3:2] Gyro Mode, [1:0]
  // Accel Mode Modes: 00:OFF, 01:OFF, 10:Standby, 11:Low Noise
  writeRegister(ICM42688_REG_PWR_MGMT0, 0x0F);
  delay(50); // Wait for sensors to stabilize

  return true;
}

void ICM42688::readSensor() {
  uint8_t buffer[14];
  // Start reading from TEMP_DATA1
  readRegisters(ICM42688_REG_TEMP_DATA1, buffer, 14);

  int16_t rawTemp = (int16_t)((buffer[0] << 8) | buffer[1]);
  int16_t rawAx = (int16_t)((buffer[2] << 8) | buffer[3]);
  int16_t rawAy = (int16_t)((buffer[4] << 8) | buffer[5]);
  int16_t rawAz = (int16_t)((buffer[6] << 8) | buffer[7]);
  int16_t rawGx = (int16_t)((buffer[8] << 8) | buffer[9]);
  int16_t rawGy = (int16_t)((buffer[10] << 8) | buffer[11]);
  int16_t rawGz = (int16_t)((buffer[12] << 8) | buffer[13]);

  // Scaling
  // Temp: (raw / 132.48) + 25
  _temp = (float)rawTemp / 132.48f + 25.0f;

  _ax = (float)rawAx * _accelRes;
  _ay = (float)rawAy * _accelRes;
  _az = (float)rawAz * _accelRes;

  _gx = (float)rawGx * _gyroRes;
  _gy = (float)rawGy * _gyroRes;
  _gz = (float)rawGz * _gyroRes;
}

void ICM42688::setAccelFS(AccelFS fs) {
  _accelFS = fs;
  uint8_t current = readRegister(ICM42688_REG_ACCEL_CONFIG0);
  current &= 0x1F; // Clear bits 7:5
  current |= (fs << 5);
  writeRegister(ICM42688_REG_ACCEL_CONFIG0, current);
  updateRes();
}

void ICM42688::setGyroFS(GyroFS fs) {
  _gyroFS = fs;
  uint8_t current = readRegister(ICM42688_REG_GYRO_CONFIG0);
  current &= 0x1F; // Clear bits 7:5
  current |= (fs << 5);
  writeRegister(ICM42688_REG_GYRO_CONFIG0, current);
  updateRes();
}

void ICM42688::setAccelODR(ODR odr) {
  uint8_t current = readRegister(ICM42688_REG_ACCEL_CONFIG0);
  current &= 0xF0; // Clear bits 3:0
  current |= (uint8_t)odr;
  writeRegister(ICM42688_REG_ACCEL_CONFIG0, current);
}

void ICM42688::setGyroODR(ODR odr) {
  uint8_t current = readRegister(ICM42688_REG_GYRO_CONFIG0);
  current &= 0xF0; // Clear bits 3:0
  current |= (uint8_t)odr;
  writeRegister(ICM42688_REG_GYRO_CONFIG0, current);
}

void ICM42688::updateRes() {
  switch (_accelFS) {
  case ACCEL_FS_16G:
    _accelRes = 16.0f / 32768.0f;
    break;
  case ACCEL_FS_8G:
    _accelRes = 8.0f / 32768.0f;
    break;
  case ACCEL_FS_4G:
    _accelRes = 4.0f / 32768.0f;
    break;
  case ACCEL_FS_2G:
    _accelRes = 2.0f / 32768.0f;
    break;
  }

  switch (_gyroFS) {
  case GYRO_FS_2000DPS:
    _gyroRes = 2000.0f / 32768.0f;
    break;
  case GYRO_FS_1000DPS:
    _gyroRes = 1000.0f / 32768.0f;
    break;
  case GYRO_FS_500DPS:
    _gyroRes = 500.0f / 32768.0f;
    break;
  case GYRO_FS_250DPS:
    _gyroRes = 250.0f / 32768.0f;
    break;
  case GYRO_FS_125DPS:
    _gyroRes = 125.0f / 32768.0f;
    break;
  case GYRO_FS_62_5DPS:
    _gyroRes = 62.5f / 32768.0f;
    break;
  case GYRO_FS_31_25DPS:
    _gyroRes = 31.25f / 32768.0f;
    break;
  case GYRO_FS_15_625DPS:
    _gyroRes = 15.625f / 32768.0f;
    break;
  }
}

void ICM42688::writeRegister(uint8_t reg, uint8_t val) {
  if (_isSPI) {
    if (_isSoftSPI) {
      digitalWrite(_cs, LOW);
      spixfer(reg); // Write bit is 0
      spixfer(val);
      digitalWrite(_cs, HIGH);
    } else {
      _spi->beginTransaction(SPISettings(12000000, MSBFIRST, SPI_MODE0));
      digitalWrite(_cs, LOW);
      _spi->transfer(reg); // Write bit is 0
      _spi->transfer(val);
      digitalWrite(_cs, HIGH);
      _spi->endTransaction();
    }
  } else {
    _wire->beginTransmission(_i2caddr);
    _wire->write(reg);
    _wire->write(val);
    _wire->endTransmission();
  }
}

uint8_t ICM42688::readRegister(uint8_t reg) {
  uint8_t val;
  if (_isSPI) {
    if (_isSoftSPI) {
      digitalWrite(_cs, LOW);
      spixfer(reg | 0x80); // Read bit is 1
      val = spixfer(0x00);
      digitalWrite(_cs, HIGH);
    } else {
      _spi->beginTransaction(SPISettings(12000000, MSBFIRST, SPI_MODE0));
      digitalWrite(_cs, LOW);
      _spi->transfer(reg | 0x80); // Read bit is 1
      val = _spi->transfer(0x00);
      digitalWrite(_cs, HIGH);
      _spi->endTransaction();
    }
  } else {
    _wire->beginTransmission(_i2caddr);
    _wire->write(reg);
    _wire->endTransmission(false);
    _wire->requestFrom(_i2caddr, (uint8_t)1);
    val = _wire->read();
  }
  return val;
}

void ICM42688::readRegisters(uint8_t reg, uint8_t *buffer, uint8_t len) {
  if (_isSPI) {
    if (_isSoftSPI) {
      digitalWrite(_cs, LOW);
      spixfer(reg | 0x80);
      for (uint8_t i = 0; i < len; i++) {
        buffer[i] = spixfer(0x00);
      }
      digitalWrite(_cs, HIGH);
    } else {
      _spi->beginTransaction(SPISettings(12000000, MSBFIRST, SPI_MODE0));
      digitalWrite(_cs, LOW);
      _spi->transfer(reg | 0x80);
      for (uint8_t i = 0; i < len; i++) {
        buffer[i] = _spi->transfer(0x00);
      }
      digitalWrite(_cs, HIGH);
      _spi->endTransaction();
    }
  } else {
    _wire->beginTransmission(_i2caddr);
    _wire->write(reg);
    _wire->endTransmission(false);
    _wire->requestFrom(_i2caddr, len);
    for (uint8_t i = 0; i < len; i++) {
      buffer[i] = _wire->read();
    }
  }
}

uint8_t ICM42688::spixfer(uint8_t x) {
  uint8_t reply = 0;
  for (int i = 7; i >= 0; i--) {
    reply <<= 1;
    digitalWrite(_sck, LOW);
    digitalWrite(_mosi, x & (1 << i));
    digitalWrite(_sck, HIGH);
    if (digitalRead(_miso))
      reply |= 1;
  }
  return reply;
}
