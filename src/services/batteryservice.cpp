#include "batteryservice.h"

uint8_t BatteryService::_vsysPercent;
uint8_t BatteryService::_batteryLevel;
float BatteryService::_vsysVoltage;

BatteryService::BatteryService() {
}
void BatteryService::initBattery() {
  measureBattery();
}
void BatteryService::measureBattery() {
  analogReadResolution(12);
  uint16_t rawADC = analogRead(A3); // Read from ADC3 (GPIO29 is mapped to A3 in Arduino)
  delay(15);
  rawADC = analogRead(A3);
  delay(15);
  float vsysVoltage = rawADC * BAT_CONV_FACTOR;
  uint8_t vsysPercent = map(vsysVoltage, 2.0f, 4.2f, 0, 100);
  uint8_t batteryLevel = map(vsysPercent, 0, 100, 0, 3);
  _vsysVoltage = vsysVoltage;
  _vsysPercent = vsysPercent;
  _batteryLevel = batteryLevel;
  Serial.printf("I Battary = %0.2fv %d%% level=%d\r\n", _vsysVoltage, vsysPercent, batteryLevel);
}

uint8_t BatteryService::get_VSYSPercent() {
  return _vsysPercent;
}

uint8_t BatteryService::getBatteryLevel() {
  return _batteryLevel;
}

float BatteryService::getVSYSVoltage() {
  return _vsysVoltage;
}
