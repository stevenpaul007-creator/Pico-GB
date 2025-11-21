#pragma once

#include "baseservice.h"
#define BAT_CONV_FACTOR (3.3f / (1 << 10) * 3)

class BatteryService {
public:
  BatteryService();
  void initBattery();
  void measureBattery();
  uint8_t get_VSYSPercent();

  /**
   * 0..3
   */
  uint8_t getBatteryLevel();
  float getVSYSVoltage();

private:
  uint8_t _vsysPercent;
  uint8_t _batteryLevel;
  float _vsysVoltage;
};