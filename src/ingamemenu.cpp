
#include <stdint.h>
#include "ingamemenu.h"
#include "common.h"
#include "input.h"
#include "gb.h"
#include "card_loader.h"
#if ENABLE_SOUND
#include "i2s-audio.h"
extern i2s_config_t i2s_config;
#endif

// 全局变量
static uint8_t current_menu_selection = 0;
static bool menu_active = false;
static uint8_t color_scheme = 0; // 当前配色方案


// 绘制弹出菜单背景
void draw_menu_background() {
  // 保存当前游戏区域（简单实现，实际可能需要更复杂的保存机制）
  // 这里绘制半透明背景来模拟弹出效果
  tft.fillRect(MENU_X, MENU_Y, MENU_WIDTH, MENU_HEIGHT, TFT_DARKGREY);
  tft.drawRect(MENU_X, MENU_Y, MENU_WIDTH, MENU_HEIGHT, TFT_WHITE);
  
  // 菜单标题
  tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);
  tft.drawString("SYSTEM MENU", MENU_X + 10, MENU_Y + 5, FONT_ID);
  tft.drawLine(MENU_X, MENU_Y + 25, MENU_X + MENU_WIDTH, MENU_Y + 25, TFT_WHITE);
}

// 绘制菜单项（复用您的样式）
void draw_menu_item(const char* text, uint8_t index, bool selected) {
  uint16_t y_pos = MENU_Y + 30 + index * FONT_HEIGHT;
  
  tft.setTextColor(selected ? TFT_BLACK : TFT_WHITE, 
                   selected ? TFT_RED : TFT_DARKGREY);
  tft.drawString(text, MENU_X + 10, y_pos, FONT_ID);
}

// 显示音量值
void draw_volume_value() {
  char vol_text[20];
  snprintf(vol_text, sizeof(vol_text), "Volume: %2d/15", 15 - i2s_config.volume);
  draw_menu_item(vol_text, MENU_VOLUME, current_menu_selection == MENU_VOLUME);
}

// 显示配色方案
void draw_color_scheme_value() {
  char color_text[30];
  snprintf(color_text, sizeof(color_text), "Change Color");
  draw_menu_item(color_text, MENU_COLOR_SCHEME, current_menu_selection == MENU_COLOR_SCHEME);
}

// 绘制完整菜单
void draw_menu() {
  
  // 绘制各个菜单项
  draw_volume_value();
  draw_menu_item("Save Realtime Game", MENU_SAVE, current_menu_selection == MENU_SAVE);
  draw_menu_item("Load Realtime Game", MENU_LOAD, current_menu_selection == MENU_LOAD);
  draw_menu_item("Save RAM", MENU_SAVERAM, current_menu_selection == MENU_SAVERAM);
  draw_menu_item("Load RAM", MENU_LOADRAM, current_menu_selection == MENU_LOADRAM);
  draw_color_scheme_value();
  draw_menu_item("Back to Game List", MENU_BACK_TO_GAME_LIST, current_menu_selection == MENU_BACK_TO_GAME_LIST);
  draw_menu_item("Restart Game", MENU_BACK_TO_GAME_LIST, current_menu_selection == MENU_BACK_TO_GAME_LIST);
  
  // 绘制操作提示
  tft.setTextColor(TFT_CYAN, TFT_DARKGREY);
  tft.drawString("Up/Down:Navigate A:Select B:Back", MENU_X + 5, MENU_Y + MENU_HEIGHT - 15, FONT_ID);
}


// 应用配色方案
void apply_color_scheme() {
  // 这里设置全局的颜色方案
  // 例如：tft.setTextColor(color_schemes[color_scheme][0], color_schemes[color_scheme][1]);
  // 您需要根据实际需求调整这个函数
}

// 处理菜单项选择
void handle_menu_selection() {
  switch(current_menu_selection) {
    case MENU_SAVE:
      // 执行保存游戏操作
      save_realtime_game();
      break;
      
    case MENU_LOAD:
      // 执行读取游戏操作  
      load_realtime_game();
      break;
      
    case MENU_SAVERAM:
      save_ram();
      break;
      
    case MENU_LOADRAM:
      load_ram();
      break;
      
    case MENU_BACK_TO_GAME_LIST:
      // 系统重启
      reboot_system();
      break;
      
    case MENU_RESTART_GAME:
      // 系统重启
      restart_game();
      return;
      break;
      
    default:
      // 其他项不需要特殊处理
      break;
  }
}

// 保存游戏（需要您实现具体逻辑）
void save_realtime_game() {
  // 显示保存提示
  save_state(&gb);
  tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
  tft.drawString("Game Saved!", MENU_X + 60, MENU_Y + MENU_HEIGHT - 30, FONT_ID);
  delay(1000);
  draw_menu(); // 重新绘制菜单清除提示
}

// 读取游戏（需要您实现具体逻辑）
void load_realtime_game() {
  // 显示读取提示
  load_state(&gb);
  tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
  tft.drawString("Game Loaded!", MENU_X + 60, MENU_Y + MENU_HEIGHT - 30, FONT_ID);
  delay(1000);
  draw_menu(); // 重新绘制菜单清除提示
}

void save_ram(){
  write_cart_ram_file(&gb);
  tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
  tft.drawString("RAM Saved!", MENU_X + 60, MENU_Y + MENU_HEIGHT - 30, FONT_ID);
  delay(1000);
  draw_menu(); // 重新绘制菜单清除提示
}
void load_ram(){
  read_cart_ram_file(&gb);
  tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
  tft.drawString("RAM Loaded!", MENU_X + 60, MENU_Y + MENU_HEIGHT - 30, FONT_ID);
  delay(1000);
  draw_menu(); // 重新绘制菜单清除提示
}

// 返回主菜单（需要您实现具体逻辑）
void return_to_main_menu() {
  //close_menu();
  // 这里添加返回主菜单的代码
}
void restart_game(){
  gb_reset();
}

// 系统重启（需要您实现具体逻辑）
void reboot_system() {
  reset();
}

// 打开菜单
void open_menu() {
  menu_active = true;
  current_menu_selection = 0;
  Serial.println("I open in game menu.");
  tft.setRotation(3);
  delay(100);
  draw_menu_background();
  draw_menu();
  handle_menu_input(0);// this is a loop
  close_menu();
}

// 关闭菜单
void close_menu() {
  menu_active = false;
  // 这里需要恢复游戏画面
  // restore_game_display(); 
  tft.setRotation(2);
  delay(200);
}

// 检查菜单是否激活
bool is_menu_active() {
  return menu_active;
}

// 处理菜单输入
void handle_menu_input(uint8_t button) {
  
  bool up, down, left, right, a, b, select, start;
  while (true) {
    up = readJoypad(PIN_UP);
    down = readJoypad(PIN_DOWN);
    left = readJoypad(PIN_LEFT);
    right = readJoypad(PIN_RIGHT);
    a = readJoypad(PIN_A);
    b = readJoypad(PIN_B);
    select = readJoypad(PIN_SELECT);
    start = readJoypad(PIN_START);

    //up
    if (!up) {
      current_menu_selection = (current_menu_selection == 0) ? MENU_ITEMS - 1 : current_menu_selection - 1;
      draw_menu();
    }
    if (!down) {
      current_menu_selection = (current_menu_selection == MENU_ITEMS - 1) ? 0 : current_menu_selection + 1;
      draw_menu();
    }
    if (!left) {
      if (current_menu_selection == MENU_VOLUME) {
        i2s_decrease_volume(&i2s_config);
        draw_volume_value();
        // 这里可以添加音量改变的实际操作
      } else if (current_menu_selection == MENU_COLOR_SCHEME) {
        draw_color_scheme_value();
        // 应用新的配色方案
        apply_color_scheme();
      }
    }
    if (!right) {
      if (current_menu_selection == MENU_VOLUME) {
        i2s_increase_volume(&i2s_config);
        draw_volume_value();
        // 这里可以添加音量改变的实际操作
      } else if (current_menu_selection == MENU_COLOR_SCHEME) {
        draw_color_scheme_value();
        // 应用新的配色方案
        apply_color_scheme();
      }
    }
    if(!a){
      handle_menu_selection();
    }
    if(!b){
      return;
    }
    sleep_ms(200);
  }
}
// 在主循环中调用这个函数来处理菜单
void update_menu_system(uint8_t button_press) {
  if (menu_active) {
    handle_menu_input(button_press);
  }
}