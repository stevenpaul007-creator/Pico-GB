#pragma once
#include "baseservice.h"

#define BAT_CONV_FACTOR (3.3f / (1 << 12) * 3)

class BatteryService {
public:
  BatteryService();
  static void initBattery();
  static void measureBattery();
  static uint8_t get_VSYSPercent();

  /**
   * 0..3
   */
  static uint8_t getBatteryLevel();
  static float getVSYSVoltage();

private:
  static uint8_t _vsysPercent;
  static uint8_t _batteryLevel;
  static float _vsysVoltage;
};