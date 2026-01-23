#pragma once

#include <Arduino.h>

class Var
{
public:
  enum class FlightMode : uint8_t
  {
    STANDBY,
    READY_TO_FLY,
    POWERED_CLIMB,
    FREE_CLIMB,
    FREE_DESCENT,
    DROGUE_CHUTE_DESCENT,
    MAIN_CHUTE_DESCENT,
    LANDED,
    SHUTDOWN,
    DATA_PROTECTION
  };

  enum class Label : uint32_t
  {
    FLIGHT_DATA,
    TRAJECTORY_DATA,
    GSE_SIGNAL,
    VALVE_MODE,
    DYNAMICS_DATA,
    VALVE_DATA_PART_1 = 0x10B,
    VALVE_DATA_PART_2,
    VALVE_DATA_PART_3,
    VALVE_DATA_PART_4,
    MONITOR_BUS,
    MONITOR_BATTERY,
    MONITOR_EXTERNAL,
  };

  enum class ValveMode : uint8_t
  {
    WAITING,
    LAUNCH
  };

  enum class GseSignal : uint8_t
  {
    IGNITION_OFF,
    IGNITION_ON
  };

  enum class Timer : uint8_t
  {
    SEPARATION_1_PROTECTION_TIME,
    SEPARATION_1_FORCE_TIME,
    SEPARATION_2_PROTECTION_TIME,
    SEPARATION_2_FORCE_TIME,
    LANDING_TIME
  };
};
