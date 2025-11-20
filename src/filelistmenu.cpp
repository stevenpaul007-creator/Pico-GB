#include "filelistmenu.h"

#include "card_loader.h"
#include "common.h"
#include "gb.h"
#include "input.h"

#include <stdint.h>
#if ENABLE_SOUND
#include "i2s-audio.h"
extern i2s_config_t i2s_config;
#endif

FileListMenu::FileListMenu() : Menu() {
  setWidth(DISPLAY_WIDTH);
  setHeight(DISPLAY_HEIGHT);
}

void FileListMenu::setOnNextPage(bool (*onNextPage)()) {
  _onNextPage = onNextPage;
}

void FileListMenu::setOnPrevPage(bool (*onPrevPage)()) {
  _onPrevPage = onPrevPage;
}

void FileListMenu::setAfterFileSelected(void (*afterFileSelected)()) {
  _afterFileSelected = afterFileSelected;
}

void FileListMenu::openMenu() {
  menuActive = true;
  setTitle("GAME LIST");
  tft.fillScreen(TFT_BLACK);
  Menu::openMenu();
}

bool FileListMenu::onKeyDown() {
  if (!select && !b) {
    rp2040.rebootToBootloader();
  }
  // up
  if (!up) {
    currentMenuSelection = (currentMenuSelection == 0) ? _menuCount - 1 : currentMenuSelection - 1;
    drawMenuItems();
  }
  if (!down) {
    currentMenuSelection = (currentMenuSelection == _menuCount - 1) ? 0 : currentMenuSelection + 1;
    drawMenuItems();
  }
  if (!left) {
    if (_onPrevPage) {
      if (_onPrevPage()) {
        currentMenuSelection = 0;
        drawMenuBackground();
        drawMenuItems();
      }
    }
  }
  if (!right) {
    if (_onNextPage) {
      if (_onNextPage()) {
        currentMenuSelection = 0;
        drawMenuBackground();
        drawMenuItems();
      }
    }
  }
  if (!a || !b || !start) {
    if (_afterFileSelected) {
      _afterFileSelected();
    }
    return true;
  }
  return false;
}
