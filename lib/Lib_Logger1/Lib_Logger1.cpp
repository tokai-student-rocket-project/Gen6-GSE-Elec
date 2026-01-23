#include "Lib_Logger1.hpp"

Logger::Logger(uint32_t csFram0)
{
  _fram0 = new FRAM(csFram0);
}

void Logger::reset()
{
  _offset = 0;
}

void Logger::dump()
{
  _fram0->dump();
}

void Logger::clear()
{
  _fram0->setWriteEnable();
  _fram0->clear();
}

uint32_t Logger::write(const uint8_t *data, uint32_t size)
{
  if (_offset + size >= FRAM::LENGTH)
  {
    return size;
  }
  else
  {
    uint32_t writeAddress = _offset;
    _fram0->setWriteEnable();
    _fram0->write(writeAddress, data, size);
  }

  _offset += size;

  ////////////////////////////////////////////////////////////
  Serial.print("[");
  Serial.print(getUsage(), 0);
  Serial.print("% | ");
  Serial.print(_offset);
  Serial.print("b / ");
  Serial.print(FRAM::LENGTH * 1);
  Serial.print("b] ");

  for (uint32_t i = 0; i < size; i++)
  {
    Serial.print(data[i], HEX);
    Serial.print((data[i] == 0x00) ? "\n" : " ");
  }
  ////////////////////////////////////////////////////////////

  return size;
}

uint32_t Logger::getOffset()
{
  return _offset;
}

float Logger::getUsage()
{
  return ((float)_offset / (float)(FRAM::LENGTH * 1)) * 100.0;
}

uint8_t Logger::framNumber()
{
  if (_offset >= FRAM::LENGTH)
  {
    return -1;
  }
  else
  {
    return 0;
  }
}
