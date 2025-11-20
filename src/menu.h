#pragma once
#include <stdint.h>

class Menu {
public:
  Menu();
  void setWidth(uint16_t width);
  void setHeight(uint16_t height);
  bool isMenuActive() const { return menuActive; }
  void setTitle(const char* title);
  const char* getTitle();

  uint8_t getSelectedIndex() const { return currentMenuSelection; };
  char* getSelectedText();
  void setTextAtIndex(const char* text, uint8_t index);
  void drawMenuItem(const char* text, uint8_t index);
  void setMenuCount(uint8_t count);
  void clearMenu();
  uint8_t getMenuCount() const { return _menuCount; };
  void drawMenuItems();
  /**
   * true: break loop
   */
  virtual bool onKeyDown();
  virtual void openMenu();
  virtual void handleMenuSelection();
  virtual void drawMenuBackground();
  virtual void onCloseMenu();

private:
  void measureBattery();
  void drawBatteryIcon();
  void loopInput();

protected:
  uint8_t currentMenuSelection = 0;
  bool up, down, left, right, a, b, select, start;
  bool menuActive = false;
  uint8_t vsysPercent = 0;
  uint8_t batteryLevel = 0;

  uint16_t _menuWidth = 0;
  uint16_t _menuHeight = 0;
  uint16_t _menuX = 0;
  uint16_t _menuY = 0;
  uint8_t _menuCount = 0;
  const char* _title;
  char* _lines[14];
};