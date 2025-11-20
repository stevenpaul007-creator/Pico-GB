#pragma once
#include "menu.h"

#include <stdint.h>

class GameMenu : public Menu {
public:
  GameMenu();
  /**
   * true: break loop
   */
  bool onKeyDown() override;
  void onCloseMenu() override;
  void openMenu() override;
  void handleMenuSelection() override;

private:
  // 私有辅助函数
  void setVolumeItem();
  void applyColorScheme();
  void saveRealtimeGame();
  void loadRealtimeGame();
  void saveRam();
  void loadRam();
  void restartGame();
  void returnToMainMenu();
  void rebootSystem();
};