#include "batteryservice.h"

BatteryService::BatteryService() {
}
void BatteryService::initBattery() {
  this->measureBattery();
}
void BatteryService::measureBattery() {
  analogReadResolution(10);
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
  Serial.printf("I Battary = %0.2fv %d%% level=%d\r\n", this->_vsysVoltage, this->_vsysPercent, this->_batteryLevel);
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
