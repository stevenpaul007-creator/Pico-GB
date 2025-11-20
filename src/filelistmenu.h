#pragma once
#include "menu.h"

#include <stdint.h>

class FileListMenu : public Menu {
public:
  FileListMenu();
  void setOnNextPage(bool (*onNextPage)());
  void setOnPrevPage(bool (*onPrevPage)());
  void setAfterFileSelected(void (*afterFileSelected)());
  /**
   * true: break loop
   */
  bool onKeyDown() override;
  void openMenu() override;

private:
  bool (*_onNextPage)();
  bool (*_onPrevPage)();
  void (*_afterFileSelected)();
  void makeTitle();

protected:
  uint8_t _currentPage = 1;
};