#include "Lib_ADXL375.hpp"

/**************************************************************************/
/*!
    @brief  I2C Constructor
*/
/**************************************************************************/
ADXL375::ADXL375(int32_t sensorID)
{
  _sensorID = sensorID;
  _i2c = true;
  _wire = &Wire;
  _i2caddr = ADXL375_ADDRESS;
}

/**************************************************************************/
/*!
    @brief  Hardware SPI Constructor
*/
/**************************************************************************/
ADXL375::ADXL375(uint8_t cs, SPIClass *theSPI, int32_t sensorID)
{
  _sensorID = sensorID;
  _i2c = false;
  _spi = theSPI;
  _cs = cs;
  _clk = -1;
  _mosi = -1;
  _miso = -1;
}

/**************************************************************************/
/*!
    @brief  Software SPI Constructor
*/
/**************************************************************************/
ADXL375::ADXL375(uint8_t clock, uint8_t miso, uint8_t mosi, uint8_t cs,
                 int32_t sensorID)
{
  _sensorID = sensorID;
  _i2c = false;
  _spi = NULL;
  _cs = cs;
  _clk = clock;
  _mosi = mosi;
  _miso = miso;
}

/**************************************************************************/
/*!
    @brief  Initialize the sensor
*/
/**************************************************************************/
bool ADXL375::begin(uint8_t i2caddr)
{
  _i2caddr = i2caddr;

  if (_i2c)
  {
    _wire->begin();
  }
  else
  {
    pinMode(_cs, OUTPUT);
    digitalWrite(_cs, HIGH);
    if (_spi)
    {
      _spi->begin();
    }
    else
    {
      pinMode(_clk, OUTPUT);
      digitalWrite(_clk, HIGH);
      pinMode(_mosi, OUTPUT);
      pinMode(_miso, INPUT);
    }
  }

  /* Check connection by reading Device ID */
  uint8_t deviceid = readRegister(ADXL375_REG_DEVID);
  if (deviceid != 0xE5)
  {
    /* No ADXL375 detected ... return false */
    // Serial.print("Device ID: "); Serial.println(deviceid, HEX);
    return false;
  }

  // Stop measurement logic
  writeRegister(ADXL375_REG_POWER_CTL, 0x00);

  // Set Data Format: Full Resolution, +/- 200g (Fixed for ADXL375),
  // Right-justified D3 (Full_Res) = 0 (ignored for ADXL375, always fixed?)
  // Wait, ADXL375 datasheet says: D1, D0 set range. But for ADXL375 fixed at
  // 200g. D3 is NOT used in the same way as ADXL343. Checking ADXL375
  // Datasheet: "The DATA_FORMAT register controls the presentation of data to
  // Register 0x32 through Register 0x37. All data, except that for the ±200 g
  // range, must be clipped (trimmed) to avoid rollover." "The D3 bit is always
  // 0." "D1 and D0 are ignored." We should set D3 to 1 to enable FULL_RES?
  // Actually Adafruit ADXL375 driver inherits ADXL343 and sets range.
  // Let's stick to simple default 0x0B (Binary: 0000 1011) like Adafruit
  // typical setup or just 0x00? Adafruit ADXL375 library doesn't explicitly
  // override setRange, but the hardware is fixed. Let's set 0x0B (Full Res +
  // INT_INVERT? No, just 0x08 for Full Res if applicable, or 0x00). Let's just
  // write 0x0B (Full Res just in case + Justify). Actually, simply 0x00 is
  // often enough for defaults, but let's try 0x08 (Full_Res) if it matters?
  // ADXL375 Datasheet: "User access to the D3 (FULL_RES) bit is disabled."
  // So writing to it does nothing.
  // However, we want "Justify" to be 0 (Right justified).
  // So 0x00 is fine.

  // Set Data Format: FULL_RES (Bit 3) + Range (Bits 1-0)
  // 試行: 0x00 (2x data issue) -> 0x0B (Full Res, +/-16g if ADXL345, Fixed if
  // 375)
  writeRegister(ADXL375_REG_DATA_FORMAT, 0x0B);

  // Set Bandwidth 100Hz (Default 0x0A)
  writeRegister(ADXL375_REG_BW_RATE, 0x0C); // Bandwidth 400 Hz

  // Enable Measurement
  writeRegister(ADXL375_REG_POWER_CTL, 0x08); // Measure bit

  return true;
}

/**************************************************************************/
/*!
    @brief  Read data and convert to m/s^2
*/
/**************************************************************************/
void ADXL375::getAcceleration(float *x, float *y, float *z)
{
  int16_t xRaw, yRaw, zRaw;
  getXYZ(&xRaw, &yRaw, &zRaw);

  *x = xRaw * ADXL375_MG2G_MULTIPLIER * 9.80665F;
  *y = yRaw * ADXL375_MG2G_MULTIPLIER * 9.80665F;
  *z = zRaw * ADXL375_MG2G_MULTIPLIER * 9.80665F;
}

void ADXL375::getXYZ(int16_t *x, int16_t *y, int16_t *z)
{
  uint8_t buffer[6];
  readLen(ADXL375_REG_DATAX0, buffer, 6);

  *x = ((int16_t)buffer[1] << 8) | buffer[0];
  *y = ((int16_t)buffer[3] << 8) | buffer[2];
  *z = ((int16_t)buffer[5] << 8) | buffer[4];
}

int16_t ADXL375::getX() { return read16(ADXL375_REG_DATAX0); }

int16_t ADXL375::getY() { return read16(ADXL375_REG_DATAY0); }

int16_t ADXL375::getZ() { return read16(ADXL375_REG_DATAZ0); }

/**************************************************************************/
/*!
    @brief  Set Trim Offsets
*/
/**************************************************************************/
void ADXL375::setTrimOffsets(int8_t x, int8_t y, int8_t z)
{
  writeRegister(ADXL375_REG_OFSX, x);
  writeRegister(ADXL375_REG_OFSY, y);
  writeRegister(ADXL375_REG_OFSZ, z);
}

/**************************************************************************/
/*!
    @brief  Get Trim Offsets
*/
/**************************************************************************/
void ADXL375::getTrimOffsets(int8_t *x, int8_t *y, int8_t *z)
{
  *x = readRegister(ADXL375_REG_OFSX);
  *y = readRegister(ADXL375_REG_OFSY);
  *z = readRegister(ADXL375_REG_OFSZ);
}

/**************************************************************************/
/*!
    @brief  Low level Write
*/
/**************************************************************************/
void ADXL375::writeRegister(uint8_t reg, uint8_t value)
{
  if (_i2c)
  {
    _wire->beginTransmission(_i2caddr);
    _wire->write(reg);
    _wire->write(value);
    _wire->endTransmission();
  }
  else
  {
    if (_spi)
    {
      _spi->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE3));
      digitalWrite(_cs, LOW);
      _spi->transfer(reg);
      _spi->transfer(value);
      digitalWrite(_cs, HIGH);
      _spi->endTransaction();
    }
    else
    {
      digitalWrite(_cs, LOW);
      spixfer(reg);
      spixfer(value);
      digitalWrite(_cs, HIGH);
    }
  }
}

/**************************************************************************/
/*!
    @brief  Low level Read
*/
/**************************************************************************/
uint8_t ADXL375::readRegister(uint8_t reg)
{
  if (_i2c)
  {
    _wire->beginTransmission(_i2caddr);
    _wire->write(reg);
    _wire->endTransmission();
    _wire->requestFrom(_i2caddr, (uint8_t)1);
    return _wire->read();
  }
  else
  {
    reg |= 0x80; // Read bit
    uint8_t value;
    if (_spi)
    {
      _spi->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE3));
      digitalWrite(_cs, LOW);
      _spi->transfer(reg);
      value = _spi->transfer(0x00);
      digitalWrite(_cs, HIGH);
      _spi->endTransaction();
    }
    else
    {
      digitalWrite(_cs, LOW);
      spixfer(reg);
      value = spixfer(0x00);
      digitalWrite(_cs, HIGH);
    }
    return value;
  }
}

/**************************************************************************/
/*!
    @brief  Read 16 bits
*/
/**************************************************************************/
int16_t ADXL375::read16(uint8_t reg)
{
  if (_i2c)
  {
    _wire->beginTransmission(_i2caddr);
    _wire->write(reg);
    _wire->endTransmission();
    _wire->requestFrom(_i2caddr, (uint8_t)2);
    return (int16_t)(_wire->read() | (_wire->read() << 8));
  }
  else
  {
    reg |= 0x80 | 0x40; // Read bit | Multibyte bit
    int16_t value;
    if (_spi)
    {
      _spi->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE3));
      digitalWrite(_cs, LOW);
      _spi->transfer(reg);
      value = _spi->transfer(0x00);
      value |= (_spi->transfer(0x00) << 8);
      digitalWrite(_cs, HIGH);
      _spi->endTransaction();
    }
    else
    {
      digitalWrite(_cs, LOW);
      spixfer(reg);
      value = spixfer(0x00);
      value |= (spixfer(0x00) << 8);
      digitalWrite(_cs, HIGH);
    }
    return value;
  }
}

/**************************************************************************/
/*!
    @brief  Read Length
*/
/**************************************************************************/
void ADXL375::readLen(uint8_t reg, uint8_t *buffer, uint8_t len)
{
  if (_i2c)
  {
    _wire->beginTransmission(_i2caddr);
    _wire->write(reg);
    _wire->endTransmission();
    _wire->requestFrom(_i2caddr, len);
    for (uint8_t i = 0; i < len; i++)
    {
      buffer[i] = _wire->read();
    }
  }
  else
  {
    reg |= 0x80 | 0x40; // Read bit | Multibyte bit
    if (_spi)
    {
      _spi->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE3));
      digitalWrite(_cs, LOW);
      _spi->transfer(reg);
      for (uint8_t i = 0; i < len; i++)
      {
        buffer[i] = _spi->transfer(0x00);
      }
      digitalWrite(_cs, HIGH);
      _spi->endTransaction();
    }
    else
    {
      digitalWrite(_cs, LOW);
      spixfer(reg);
      for (uint8_t i = 0; i < len; i++)
      {
        buffer[i] = spixfer(0x00);
      }
      digitalWrite(_cs, HIGH);
    }
  }
}

/**************************************************************************/
/*!
    @brief  Software SPI Transfer
*/
/**************************************************************************/
uint8_t ADXL375::spixfer(uint8_t x)
{
  uint8_t reply = 0;
  for (int i = 7; i >= 0; i--)
  {
    reply <<= 1;
    digitalWrite(_clk, LOW);
    digitalWrite(_mosi, x & (1 << i));
    digitalWrite(_clk, HIGH);
    if (digitalRead(_miso))
      reply |= 1;
  }
  return reply;
}
