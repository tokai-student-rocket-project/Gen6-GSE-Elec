#include "Lib_FRAM.hpp"

FRAM::FRAM(uint32_t cs) {
  _spi = &SPI;
  _cs = cs;
  _clk = -1;
  _mosi = -1;
  _miso = -1;
  _isSoftSPI = false;
  _setting = SPISettings(20000000, MSBFIRST, SPI_MODE0);
  pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH);
}

FRAM::FRAM(uint32_t cs, SPIClass *theSPI) {
  _spi = theSPI;
  _cs = cs;
  _clk = -1;
  _mosi = -1;
  _miso = -1;
  _isSoftSPI = false;
  _setting = SPISettings(20000000, MSBFIRST, SPI_MODE0);
  pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH);
}

FRAM::FRAM(uint32_t clk, uint32_t miso, uint32_t mosi, uint32_t cs) {
  _spi = NULL;
  _clk = clk;
  _miso = miso;
  _mosi = mosi;
  _cs = cs;
  _isSoftSPI = true;
  _setting = SPISettings(20000000, MSBFIRST, SPI_MODE0);

  pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH);
  pinMode(_clk, OUTPUT);
  digitalWrite(_clk, HIGH); // Mode 3 idle HIGH
  pinMode(_mosi, OUTPUT);
  pinMode(_miso, INPUT);
}

void FRAM::setWriteEnable() {
  if (_isSoftSPI)
    digitalWrite(_clk, HIGH);
  if (!_isSoftSPI)
    SPI.beginTransaction(_setting);
  digitalWrite(_cs, LOW);

  transfer(WREN);

  digitalWrite(_cs, HIGH);
  if (!_isSoftSPI)
    SPI.endTransaction();
}

void FRAM::getStatus(uint8_t *buffer) {
  if (_isSoftSPI)
    digitalWrite(_clk, HIGH);
  if (!_isSoftSPI)
    SPI.beginTransaction(_setting);
  digitalWrite(_cs, LOW);

  transfer(RDSR);
  buffer[0] = transfer(0xFF);

  digitalWrite(_cs, HIGH);
  if (!_isSoftSPI)
    SPI.endTransaction();
}

void FRAM::getId(uint8_t *buffer) {
  if (_isSoftSPI)
    digitalWrite(_clk, HIGH);
  if (!_isSoftSPI)
    SPI.beginTransaction(_setting);
  digitalWrite(_cs, LOW);

  transfer(RDID);
  buffer[0] = transfer(0xFF);
  buffer[1] = transfer(0xFF);
  buffer[2] = transfer(0xFF);
  buffer[3] = transfer(0xFF);

  digitalWrite(_cs, HIGH);
  if (!_isSoftSPI)
    SPI.endTransaction();
}

uint8_t FRAM::read(uint32_t address) {
  uint8_t addressPart[3];
  memcpy(addressPart, &address, 3);

  if (_isSoftSPI)
    digitalWrite(_clk, HIGH);
  if (!_isSoftSPI)
    SPI.beginTransaction(_setting);
  digitalWrite(_cs, LOW);

  transfer(READ);

  transfer(addressPart[2]);
  transfer(addressPart[1]);
  transfer(addressPart[0]);
  uint8_t data = transfer(0xFF);

  digitalWrite(_cs, HIGH);
  if (!_isSoftSPI)
    SPI.endTransaction();

  return data;
}

void FRAM::write(uint32_t address, uint8_t data) {
  uint8_t addressPart[3];
  memcpy(addressPart, &address, 3);

  if (_isSoftSPI)
    digitalWrite(_clk, HIGH);
  if (!_isSoftSPI)
    SPI.beginTransaction(_setting);
  digitalWrite(_cs, LOW);

  transfer(WRITE);

  transfer(addressPart[2]);
  transfer(addressPart[1]);
  transfer(addressPart[0]);
  transfer(data);

  digitalWrite(_cs, HIGH);
  if (!_isSoftSPI)
    SPI.endTransaction();
}

void FRAM::write(uint32_t address, const uint8_t *data, uint32_t size) {
  uint8_t addressPart[3];
  memcpy(addressPart, &address, 3);

  if (_isSoftSPI)
    digitalWrite(_clk, HIGH);
  if (!_isSoftSPI)
    SPI.beginTransaction(_setting);
  digitalWrite(_cs, LOW);

  transfer(WRITE);

  transfer(addressPart[2]);
  transfer(addressPart[1]);
  transfer(addressPart[0]);

  for (uint32_t i = 0; i < size; i++) {
    transfer(data[i]);
  }

  digitalWrite(_cs, HIGH);
  if (!_isSoftSPI)
    SPI.endTransaction();
}

void FRAM::clear() {
  for (uint32_t address = 0; address < LENGTH; address++) {
    write(address, 0x00);
  }
}

void FRAM::dump() {
  for (size_t address = 0; address < LENGTH; address++) {
    uint8_t data = read(address);

    Serial.print(data, HEX);

    if (data == 0) {
      Serial.println();
    } else {
      Serial.print(" ");
    }
  }
}

uint8_t FRAM::transfer(uint8_t data) {
  if (_isSoftSPI) {
    return spixfer(data);
  } else {
    return _spi->transfer(data);
  }
}

uint8_t FRAM::spixfer(uint8_t x) {
  uint8_t reply = 0;
  for (int i = 7; i >= 0; i--) {
    reply <<= 1;
    digitalWrite(_clk, LOW);
    digitalWrite(_mosi, x & (1 << i));
    digitalWrite(_clk, HIGH);
    if (digitalRead(_miso))
      reply |= 1;
  }
  return reply;
}
