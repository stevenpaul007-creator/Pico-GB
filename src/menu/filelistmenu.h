#pragma once
#include "menu.h"

#include <stdint.h>

class FileListMenu : public Menu {
public:
  FileListMenu();
  void setOnNextPageCallback(std::function<bool()> onNextPageCallback);
  void setOnPrevPageCallback(std::function<bool()> onPrevPageCallback);
  void setAfterFileSelectedCallback(std::function<void()> afterFileSelectedCallback);
  void setOnSelectKeyPressedCallback(std::function<void()> onSelectKeyPressedCallback);
  /**
   * true: break loop
   */
  bool onKeyDown() override;
  void openMenu() override;
  void setGameType(GameType gametype);
  void onCloseMenu() override;

private:
  std::function<bool()> _onNextPageCallback;
  std::function<bool()> _onPrevPageCallback;
  std::function<void()> _afterFileSelectedCallback;
  std::function<void()> _onSelectKeyPressedCallback;
  void makeTitle();

protected:
  uint8_t _currentPage = 1;
  GameType _gameType = GameType_GB;
};