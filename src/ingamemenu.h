#include <stdint.h>

// 菜单系统相关定义
#define MENU_ITEMS 6
#define MENU_WIDTH 200
#define MENU_HEIGHT 180
#define MENU_X 20
#define MENU_Y 30

// 菜单项枚举
typedef enum {
  MENU_VOLUME = 0,
  MENU_SAVE,
  MENU_LOAD,
  MENU_COLOR_SCHEME,
  MENU_BACK_TO_MAIN,
  MENU_REBOOT
} menu_item_t;

// 全局变量
static uint8_t current_menu_selection = 0;
static bool menu_active = false;
static uint8_t volume_level = 8; // 默认音量
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
  snprintf(vol_text, sizeof(vol_text), "Volume: %2d/15", volume_level);
  draw_menu_item(vol_text, MENU_VOLUME, current_menu_selection == MENU_VOLUME);
}

// 显示配色方案
void draw_color_scheme_value() {
  const char* color_names[] = {"Default", "Blue/Yellow", "Green", "Cyan/Grey", "Orange/Navy"};
  char color_text[30];
  snprintf(color_text, sizeof(color_text), "Color: %s", color_names[color_scheme]);
  draw_menu_item(color_text, MENU_COLOR_SCHEME, current_menu_selection == MENU_COLOR_SCHEME);
}

// 绘制完整菜单
void draw_menu() {
  draw_menu_background();
  
  // 绘制各个菜单项
  draw_volume_value();
  draw_menu_item("Save Game (A)", MENU_SAVE, current_menu_selection == MENU_SAVE);
  draw_menu_item("Load Game (A)", MENU_LOAD, current_menu_selection == MENU_LOAD);
  draw_color_scheme_value();
  draw_menu_item("Back to Main", MENU_BACK_TO_MAIN, current_menu_selection == MENU_BACK_TO_MAIN);
  draw_menu_item("Reboot System", MENU_REBOOT, current_menu_selection == MENU_REBOOT);
  
  // 绘制操作提示
  tft.setTextColor(TFT_CYAN, TFT_DARKGREY);
  tft.drawString("Up/Down:Navigate A:Select B:Back", MENU_X + 5, MENU_Y + MENU_HEIGHT - 15, FONT_ID);
}

// 处理菜单输入
void handle_menu_input(uint8_t button) {
  switch(button) {
    case 'U': // 上键
      current_menu_selection = (current_menu_selection == 0) ? MENU_ITEMS - 1 : current_menu_selection - 1;
      draw_menu();
      break;
      
    case 'D': // 下键
      current_menu_selection = (current_menu_selection == MENU_ITEMS - 1) ? 0 : current_menu_selection + 1;
      draw_menu();
      break;
      
    case 'L': // 左键
      if (current_menu_selection == MENU_VOLUME && volume_level > 0) {
        volume_level--;
        draw_volume_value();
        // 这里可以添加音量改变的实际操作
      } else if (current_menu_selection == MENU_COLOR_SCHEME) {
        color_scheme = (color_scheme == 0) ? COLOR_SCHEME_COUNT - 1 : color_scheme - 1;
        draw_color_scheme_value();
        // 应用新的配色方案
        apply_color_scheme();
      }
      break;
      
    case 'R': // 右键
      if (current_menu_selection == MENU_VOLUME && volume_level < 15) {
        volume_level++;
        draw_volume_value();
        // 这里可以添加音量改变的实际操作
      } else if (current_menu_selection == MENU_COLOR_SCHEME) {
        color_scheme = (color_scheme == COLOR_SCHEME_COUNT - 1) ? 0 : color_scheme + 1;
        draw_color_scheme_value();
        // 应用新的配色方案
        apply_color_scheme();
      }
      break;
      
    case 'A': // A键确认
      handle_menu_selection();
      break;
      
    case 'B': // B键退出菜单
      close_menu();
      break;
  }
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
      save_game();
      break;
      
    case MENU_LOAD:
      // 执行读取游戏操作  
      load_game();
      break;
      
    case MENU_BACK_TO_MAIN:
      // 返回主菜单
      return_to_main_menu();
      break;
      
    case MENU_REBOOT:
      // 系统重启
      reboot_system();
      break;
      
    default:
      // 其他项不需要特殊处理
      break;
  }
}

// 保存游戏（需要您实现具体逻辑）
void save_game() {
  // 显示保存提示
  tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
  tft.drawString("Game Saved!", MENU_X + 60, MENU_Y + MENU_HEIGHT - 30, FONT_ID);
  delay(1000);
  draw_menu(); // 重新绘制菜单清除提示
}

// 读取游戏（需要您实现具体逻辑）
void load_game() {
  // 显示读取提示
  tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
  tft.drawString("Game Loaded!", MENU_X + 60, MENU_Y + MENU_HEIGHT - 30, FONT_ID);
  delay(1000);
  draw_menu(); // 重新绘制菜单清除提示
}

// 返回主菜单（需要您实现具体逻辑）
void return_to_main_menu() {
  close_menu();
  // 这里添加返回主菜单的代码
}

// 系统重启（需要您实现具体逻辑）
void reboot_system() {
  tft.setTextColor(TFT_RED, TFT_DARKGREY);
  tft.drawString("Rebooting...", MENU_X + 60, MENU_Y + MENU_HEIGHT - 30, FONT_ID);
  delay(1000);
  // 实际的重启代码
  // rp2350_restart(); 或类似的函数
}

// 打开菜单
void open_menu() {
  menu_active = true;
  current_menu_selection = 0;
  draw_menu();
}

// 关闭菜单
void close_menu() {
  menu_active = false;
  // 这里需要恢复游戏画面
  // restore_game_display(); 
}

// 检查菜单是否激活
bool is_menu_active() {
  return menu_active;
}

// 在主循环中调用这个函数来处理菜单
void update_menu_system(uint8_t button_press) {
  if (menu_active) {
    handle_menu_input(button_press);
  }
}