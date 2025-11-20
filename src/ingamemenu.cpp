#include "ingamemenu.h"

#include "card_loader.h"
#include "common.h"
#include "gb.h"
#include "input.h"

#include <stdint.h>
#if ENABLE_SOUND
#include "i2s-audio.h"
extern i2s_config_t i2s_config;
#endif

GameMenu::GameMenu() : Menu() {
  setWidth(DISPLAY_WIDTH);
  setHeight(DISPLAY_HEIGHT);
}

// 菜单项枚举
enum MenuItem {
  MENU_VOLUME = 0,
  MENU_SAVE,
  MENU_LOAD,
  MENU_SAVERAM,
  MENU_LOADRAM,
  MENU_COLOR_SCHEME,
  MENU_BACK_TO_GAME_LIST,
  MENU_RESTARTGAME,
  COUNT // 用于方便计算菜单项数量
};

uint8_t MENU_ITEMS = static_cast<int>(MenuItem::COUNT);
// 显示音量值
void GameMenu::setVolumeItem() {
  char vol_text[20];
  if (i2s_config.volume == 16) {
    snprintf(vol_text, sizeof(vol_text), " Volume:    OFF ");
  } else {
    snprintf(vol_text, sizeof(vol_text), " Volume: %2d/16 ", 16 - i2s_config.volume);
  }
  setTextAtIndex(vol_text, MENU_VOLUME);
}

// 应用配色方案
void GameMenu::applyColorScheme() {
  nextPalette();
}

// 处理菜单项选择
void GameMenu::handleMenuSelection() {
  switch (currentMenuSelection) {
  case MENU_COLOR_SCHEME:
    applyColorScheme();
    break;

  case MENU_SAVE:
    // 执行保存游戏操作
    saveRealtimeGame();
    break;

  case MENU_LOAD:
    // 执行读取游戏操作
    loadRealtimeGame();
    break;

  case MENU_SAVERAM:
    saveRam();
    break;

  case MENU_LOADRAM:
    loadRam();
    break;

  case MENU_BACK_TO_GAME_LIST:
    // 系统重启
    rebootSystem();
    break;

  case MENU_RESTARTGAME:
    // 系统重启
    restartGame();
    return;
    break;

  default:
    // 其他项不需要特殊处理
    break;
  }
}

// 保存游戏（需要您实现具体逻辑）
void GameMenu::saveRealtimeGame() {
  // 显示保存提示
  save_state(&gb);
  tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
  tft.drawString("Game Saved!", _menuX + 45, _menuY + _menuHeight - 50, FONT_ID);
  delay(1000);
  // drawMenuItems(); // 重新绘制菜单清除提示
}

// 读取游戏（需要您实现具体逻辑）
void GameMenu::loadRealtimeGame() {
  // 显示读取提示
  load_state(&gb);
  tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
  tft.drawString("Game Loaded!", _menuX + 45, _menuY + _menuHeight - 50, FONT_ID);
  delay(1000);
  // drawMenuItems(); // 重新绘制菜单清除提示
}

void GameMenu::saveRam() {
  write_cart_ram_file(&gb);
  tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
  tft.drawString("RAM Saved!", _menuX + 45, _menuY + _menuHeight - 50, FONT_ID);
  delay(1000);
  // drawMenuItems(); // 重新绘制菜单清除提示
}
void GameMenu::loadRam() {
  read_cart_ram_file(&gb);
  tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
  tft.drawString("RAM Loaded!", _menuX + 45, _menuY + _menuHeight - 50, FONT_ID);
  delay(1000);
  // drawMenuItems(); // 重新绘制菜单清除提示
}

// 返回主菜单（需要您实现具体逻辑）
void GameMenu::returnToMainMenu() {
  // closeMenu();
  //  这里添加返回主菜单的代码
}
void GameMenu::restartGame() {
  gb_reset();
}

// 系统重启（需要您实现具体逻辑）
void GameMenu::rebootSystem() {
  rp2040.reboot();
}

// 关闭菜单
void GameMenu::onCloseMenu() {
  menuActive = false;
  delay(400);
  tft.setRotation(2);
}

void GameMenu::openMenu() {
  menuActive = true;
  setTitle("IN GAME MENU");
  setVolumeItem();
  setTextAtIndex(" Save Realtime Game ", MENU_SAVE);
  setTextAtIndex(" Load Realtime Game ", MENU_LOAD);
  setTextAtIndex(" Save RAM ", MENU_SAVERAM);
  setTextAtIndex(" Load RAM ", MENU_LOADRAM);
  setTextAtIndex(" Next Color Palette ", MENU_COLOR_SCHEME);
  setTextAtIndex(" Back to Game List ", MENU_BACK_TO_GAME_LIST);
  setTextAtIndex(" Restart Game ", MENU_RESTARTGAME);

  Menu::openMenu();
}

bool GameMenu::onKeyDown() {
  if (!select && !b) {
    rp2040.rebootToBootloader();
  }
  // up
  if (!up) {
    currentMenuSelection = (currentMenuSelection == 0) ? MENU_ITEMS - 1 : currentMenuSelection - 1;
    drawMenuItems();
  }
  if (!down) {
    currentMenuSelection = (currentMenuSelection == MENU_ITEMS - 1) ? 0 : currentMenuSelection + 1;
    drawMenuItems();
  }
  if (!left) {
    if (currentMenuSelection == MENU_VOLUME) {
      i2s_decrease_volume(&i2s_config);
      setVolumeItem();
      drawMenuItem(_lines[MENU_VOLUME], MENU_VOLUME);
    }
  }
  if (!right) {
    if (currentMenuSelection == MENU_VOLUME) {
      i2s_increase_volume(&i2s_config);
      setVolumeItem();
      drawMenuItem(_lines[MENU_VOLUME], MENU_VOLUME);
    }
  }
  if (!a) {
    handleMenuSelection();
    return true;
  }
  if (!b) {
    return true;
  }
  return false;
}
