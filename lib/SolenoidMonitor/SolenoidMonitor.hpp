#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <Mcp320x.h>


class SolenoidMonitor {
public:
  enum class Solenoid : uint8_t {
    SHIFT = MCP320xTypes::MCP3208::Channel::SINGLE_0,
    FILL = MCP320xTypes::MCP3208::Channel::SINGLE_1,
    DUMP = MCP320xTypes::MCP3208::Channel::SINGLE_2,
    OXYGEN = MCP320xTypes::MCP3208::Channel::SINGLE_3,
    PURGE = MCP320xTypes::MCP3208::Channel::SINGLE_4,
    CLOSE = MCP320xTypes::MCP3208::Channel::SINGLE_5,
    OPEN = MCP320xTypes::MCP3208::Channel::SINGLE_6,
    IGNITER = MCP320xTypes::MCP3208::Channel::SINGLE_7
  };

  enum class Status : uint8_t {
    OPEN_FAILURE,
    CLOSE_FAILURE,
    ON,
    OFF,
  };

  SolenoidMonitor(uint8_t cs);

  void setDividerResistance(float upperResistance, float lowerResistance);

  uint16_t getVoltage_mV(Solenoid solenoid);
  SolenoidMonitor::Status getStatus(Solenoid solenoid);

private:
  SPISettings _setting;
  MCP3208* _mcp;
  float _segmentFactor = 1.0;
};
