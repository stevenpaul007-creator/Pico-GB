#pragma once

// 菜单系统相关定义
#define MENU_ITEMS 9
#define MENU_WIDTH TFT_HEIGHT
#define MENU_HEIGHT TFT_WIDTH
#define MENU_X 0
#define MENU_Y 0

// 菜单项枚举
typedef enum {
  MENU_VOLUME = 0,
  MENU_SAVE,
  MENU_LOAD,
  MENU_SAVERAM,
  MENU_LOADRAM,
  MENU_COLOR_SCHEME,
  MENU_BACK_TO_GAME_LIST,
  MENU_RESTART_GAME
} menu_item_t;

void open_menu();
void close_menu();
bool is_menu_active();
void handle_menu_selection();
void apply_color_scheme();
void handle_menu_input(uint8_t button);
void update_menu_system(uint8_t button_press);

void save_realtime_game();
void load_realtime_game();
void save_ram();
void load_ram();
void restart_game();


void return_to_main_menu();

void reboot_system();
