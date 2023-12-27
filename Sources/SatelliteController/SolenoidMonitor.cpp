#include "SolenoidMonitor.hpp"


SolenoidMonitor::SolenoidMonitor(uint8_t cs) {
  _setting = SPISettings(1600000, MSBFIRST, SPI_MODE0);
  _mcp = new MCP3208(3300, cs);

  pinMode(cs, OUTPUT);
  digitalWrite(cs, HIGH);
}


void SolenoidMonitor::setDividerResistance(float upperResistance, float lowerResistance) {
  _segmentFactor = (lowerResistance + upperResistance) / lowerResistance;
}


float SolenoidMonitor::getVoltage(Solenoid solenoid) {
  MCP320xTypes::MCP3208::Channel channel = static_cast<MCP320xTypes::MCP3208::Channel>(solenoid);

  SPI.beginTransaction(_setting);
  float segmentVoltage = (float)_mcp->toAnalog(_mcp->read(channel)) / 1000.0;
  SPI.endTransaction();

  return segmentVoltage * _segmentFactor;
}
