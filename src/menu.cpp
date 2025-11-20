#include "menu.h"

#include "common.h"

#include <cstdlib> // For malloc and free
#include <cstring> // For strlen and memcpy

#define BAT_CONV_FACTOR (3.3f / (1 << 10) * 3)
Menu::Menu() {
}
void Menu::setWidth(uint16_t width) {
  _menuWidth = width;
}
void Menu::setHeight(uint16_t height) {
  _menuHeight = height;
}
// 打开菜单
void Menu::openMenu() {
  menuActive = true;
  currentMenuSelection = 0;
  tft.setRotation(3);
  measureBattery();
  drawMenuBackground();
  drawMenuItems();
  loopInput();
  onCloseMenu();
}

// 关闭菜单
void Menu::onCloseMenu() {
  menuActive = false;
}
void Menu::handleMenuSelection() {
}

bool Menu::onKeyDown() {
  return true;
}

void Menu::setTitle(const char* title) {
  // 1. 释放旧的标题内存（防止内存泄漏）
  if (_title != nullptr) {
    free(_title);
    _title = nullptr;
  }

  // 2. 使用 strdup 复制新的字符串内容
  // strdup 会在堆上分配内存并复制字符串
  _title = strdup(title);
}
const char* Menu::getTitle() {
  return _title;
}
char* Menu::getSelectedText() {
  return _lines[currentMenuSelection];
}
void Menu::setTextAtIndex(const char* text, uint8_t index) {
  if (index < 0 || index > sizeof(_lines)) {
    return;
  }

  if (index + 1 > _menuCount) {
    _menuCount = index + 1;
  }
  // 1. 如果已经有旧字符串，释放它
  if (_lines[index] != nullptr) {
    free((void*)_lines[index]);
    _lines[index] = nullptr;
  }

  // 2. 复制传入的字符串到新的堆内存中
  _lines[index] = strdup(text);
}
void Menu::drawMenuItems() {
  for (uint8_t index = 0; index < _menuCount; index++) {
    drawMenuItem(_lines[index], index);
#if 0
#define MAX_CHARS_PER_LINE 45
    if (strlen(_lines[index]) < MAX_CHARS_PER_LINE) {
      drawMenuItem(_lines[index], index);
    } else {
      char* text = _lines[index];
      size_t len_to_copy = MAX_CHARS_PER_LINE;
      // 2. 分配新内存 (+1 字节用于空终止符 '\0')
      char* new_str = (char*)malloc(len_to_copy + 1);
      if (new_str == nullptr) {
        // 内存分配失败处理
        return;
      }
      // 3. 复制数据并手动添加空终止符
      memcpy(new_str, text, len_to_copy);
      new_str[len_to_copy] = '\0'; // 确保字符串以空字符结尾
      drawMenuItem(new_str, index);
    }
#endif
  }
}
void Menu::drawMenuItem(const char* text, uint8_t index) {
  bool selected = currentMenuSelection == index;
  uint16_t y_pos = _menuY + 30 + index * FONT_HEIGHT;

  tft.setTextColor(selected ? TFT_BLACK : TFT_WHITE,
      selected ? TFT_RED : TFT_DARKGREY, true);
  tft.drawString(text, _menuX + 10, y_pos, FONT_ID);
}

void Menu::setMenuCount(uint8_t count) {
  _menuCount = count;
}

void Menu::clearMenu() {
  _menuCount = 0;
}

void Menu::drawMenuBackground() {
  tft.fillRect(_menuX, _menuY, _menuWidth, _menuHeight, TFT_DARKGREY);
  tft.drawRect(_menuX, _menuY, _menuWidth, _menuHeight, TFT_WHITE);
  tft.drawRect(_menuX + 1, _menuY + 1, _menuWidth - 1, _menuHeight - 1, TFT_WHITE);

  // 菜单标题
  tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);
  tft.drawString(getTitle(), _menuX + 10, _menuY + 5, FONT_ID);
  tft.drawLine(_menuX, _menuY + 25, _menuX + _menuWidth, _menuY + 25, TFT_WHITE);
  drawBatteryIcon();
}

void Menu::measureBattery() {
  analogReadResolution(10);
  uint16_t rawADC = analogRead(A3); // Read from ADC3 (GPIO29 is mapped to A3 in Arduino)
  delay(5);
  rawADC = analogRead(A3);
  delay(5);
  float vsysVoltage = rawADC * BAT_CONV_FACTOR;
  uint8_t vsysPercent = map(vsysVoltage, 2.0f, 4.2f, 0, 100);
  batteryLevel = map(vsysPercent, 0, 100, 0, 3);
  Serial.printf("I Battary = %0.2fv %d%% level=%d\r\n", vsysVoltage, vsysPercent, batteryLevel);
}

// 绘制电池图标的函数
void Menu::drawBatteryIcon() {
  int x = tft.width() - 30; // 放置在右上角，距离右边缘 30 像素
  int y = 5; // 距离顶边缘 5 像素
  int w = 24; // 电池宽度
  int h = 12; // 电池高度

  // 1. 清除旧图标区域（如果需要，或者直接覆盖）
  tft.fillRect(x, y, w, h, TFT_BLACK);

  // 2. 绘制电池外壳
  tft.drawRect(x, y, w, h, TFT_WHITE);

  // 3. 填充电量格
  int numBars = batteryLevel; // batteryLevel 应为 0, 1, 2 或 3
  int barSpacing = 2;
  int barWidth = (w - (4 * barSpacing)) / 3; // 三格的宽度

  for (int i = 0; i < numBars; i++) {
    int barX = x + barSpacing + i * (barWidth + barSpacing);
    int barY = y + barSpacing;
    int barH = h - (2 * barSpacing);

    // 使用绿色表示电量充足，低电量时可以使用红色
    uint16_t barColor = (batteryLevel <= 1) ? TFT_RED : TFT_GREEN;
    tft.fillRect(barX, barY, barWidth, barH, barColor);
  }
}

// 处理菜单输入
void Menu::loopInput() {
  while (true) {
    up = gpio_get(PIN_UP);
    down = gpio_get(PIN_DOWN);
    left = gpio_get(PIN_LEFT);
    right = gpio_get(PIN_RIGHT);
    a = gpio_get(PIN_A);
    b = gpio_get(PIN_B);
    select = gpio_get(PIN_SELECT);
    start = gpio_get(PIN_START);

    if (!up || !down || !left || !right || !a || !b || !start || !select) {
      if (onKeyDown())
        return;
    }
    if (!up || !down || !left || !right) {
      delay(150);
    }
    delay(50);
  }
}