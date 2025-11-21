#include "filelistmenu.h"

#include "common.h"

#include <stdint.h>

FileListMenu::FileListMenu() : Menu() {
  setWidth(DISPLAY_WIDTH);
  setHeight(DISPLAY_HEIGHT);
}

void FileListMenu::setOnNextPageCallback(std::function<bool()> onNextPageCallback) {
  _onNextPageCallback = onNextPageCallback;
}

void FileListMenu::setOnPrevPageCallback(std::function<bool()> onPrevPageCallback) {
  _onPrevPageCallback = onPrevPageCallback;
}

void FileListMenu::setAfterFileSelectedCallback(std::function<void()> afterFileSelectedCallback) {
  _afterFileSelectedCallback = afterFileSelectedCallback;
}

void FileListMenu::setOnSelectKeyPressedCallback(std::function<void()> onSelectKeyPressedCallback) {
  _onSelectKeyPressedCallback = onSelectKeyPressedCallback;
}

void FileListMenu::openMenu() {
  menuActive = true;
  _currentPage = 1;
  makeTitle();
  tft.fillScreen(TFT_BLACK);
  Menu::openMenu();
}

void FileListMenu::setGameType(GameType gametype) {
  _gameType = gametype;
}

void FileListMenu::makeTitle() {
  char title_text[25];
  snprintf(title_text, sizeof(title_text), "%sGAME LIST  PAGE %d", _gameType == GameType_GB ? "GB " : "NES ", _currentPage);
  setTitle(title_text);
}

bool FileListMenu::onKeyDown() {
  if (PRESSED_KEY(ButtonID::BTN_SELECT)) {
    if (_onSelectKeyPressedCallback) {
      _onSelectKeyPressedCallback();
      makeTitle();
      currentMenuSelection = 0;
      _currentPage = 1;
      drawMenuBackground();
      drawMenuItems();
    }
  }
  if (PRESSED_KEY(ButtonID::BTN_SELECT) && PRESSED_KEY(ButtonID::BTN_B)) {
    rp2040.rebootToBootloader();
  }
  // up
  if (PRESSED_KEY(ButtonID::BTN_UP)) {
    currentMenuSelection = (currentMenuSelection == 0) ? _menuCount - 1 : currentMenuSelection - 1;
    drawMenuItems();
  }
  if (PRESSED_KEY(ButtonID::BTN_DOWN)) {
    currentMenuSelection = (currentMenuSelection == _menuCount - 1) ? 0 : currentMenuSelection + 1;
    drawMenuItems();
  }
  if (PRESSED_KEY(ButtonID::BTN_LEFT)) {
    if (_onPrevPageCallback) {
      if (_onPrevPageCallback()) {
        _currentPage--;
        makeTitle();
        currentMenuSelection = 0;
        drawMenuBackground();
        drawMenuItems();
      }
    }
  }
  if (PRESSED_KEY(ButtonID::BTN_RIGHT)) {
    if (_onNextPageCallback) {
      if (_onNextPageCallback()) {
        _currentPage++;
        makeTitle();
        currentMenuSelection = 0;
        drawMenuBackground();
        drawMenuItems();
      }
    }
  }
  if (PRESSED_KEY(ButtonID::BTN_A) || PRESSED_KEY(ButtonID::BTN_B)) {
    if (_afterFileSelectedCallback) {
      _afterFileSelectedCallback();
    }
    return true;
  }
  return false;
}
