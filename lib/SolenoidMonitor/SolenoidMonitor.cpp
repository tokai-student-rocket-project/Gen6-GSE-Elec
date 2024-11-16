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


uint16_t SolenoidMonitor::getVoltage_mV(Solenoid solenoid) {
  MCP320xTypes::MCP3208::Channel channel = static_cast<MCP320xTypes::MCP3208::Channel>(solenoid);

  SPI.beginTransaction(_setting);
  uint16_t segmentVoltage = _mcp->toAnalog(_mcp->read(channel));
  SPI.endTransaction();

  return segmentVoltage * _segmentFactor;
}


SolenoidMonitor::Status SolenoidMonitor::getStatus(Solenoid solenoid) {
  uint16_t voltage_mV = getVoltage_mV(solenoid);

  if (voltage_mV < 8) return SolenoidMonitor::Status::OPEN_FAILURE;
  if (voltage_mV >= 8 && voltage_mV < 100) return SolenoidMonitor::Status::ON;
  if (voltage_mV >= 100 && voltage_mV < 7000) return SolenoidMonitor::Status::OFF;
  return SolenoidMonitor::Status::CLOSE_FAILURE;
}
