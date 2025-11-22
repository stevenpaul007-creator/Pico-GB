#pragma once
#include <stdint.h>

class GBInput {
public:
  GBInput();
  void initJoypad();
  void handleJoypad();
  void handleSerial();
  void nextPalette();
  void shutdown();

private:
  void afterHandleJoypadCallback();
  void updatePalette();
  void prevPalette();
  
  void applyColorSchemeCallback();
  void saveRealtimeGameCallback();
  void loadRealtimeGameCallback();
  void saveRamCallback();
  void loadRamCallback();
  void restartGameCallback();


protected:
  uint8_t palette_selected = 0;
};
