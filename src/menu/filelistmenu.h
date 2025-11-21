#pragma once
#include "menu.h"

#include <stdint.h>

class FileListMenu : public Menu {
public:
  FileListMenu();
  void setOnNextPageCallback(std::function<bool()> onNextPageCallback);
  void setOnPrevPageCallback(std::function<bool()> onPrevPageCallback);
  void setAfterFileSelectedCallback(std::function<void()> afterFileSelectedCallback);
  /**
   * true: break loop
   */
  bool onKeyDown() override;
  void openMenu() override;

private:
  std::function<bool()> _onNextPageCallback;
  std::function<bool()> _onPrevPageCallback;
  std::function<void()> _afterFileSelectedCallback;
  void makeTitle();

protected:
  uint8_t _currentPage = 1;
};