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

  SolenoidMonitor(uint8_t cs);

  void setDividerResistance(float upperResistance, float lowerResistance);

  float getVoltage(Solenoid solenoid);

private:
  SPISettings _setting;
  MCP3208* _mcp;
  float _segmentFactor = 1.0;
};
