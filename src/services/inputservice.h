#pragma once
#include "baseservice.h"

#if ENABLE_INPUT == INPUT_PCF8574
#include <PCF8574.h>
#endif

// 定义按键枚举
typedef enum {
  BTN_UP,
  BTN_DOWN,
  BTN_LEFT,
  BTN_RIGHT,
  BTN_A,
  BTN_B,
  BTN_SELECT,
  BTN_START,
  BTN_COUNT // 用于计算按键总数
} ButtonID;

class InputService {
public:
  InputService();
  // 初始化按键服务的函数
  void buttonServiceInit();
  // 检查特定按键是否按下的函数
  bool isButtonPressed(ButtonID button);
  // 检查特定按键是否第一次按下的函数
  bool isButtonPressedFirstTime(ButtonID button);
  // 检查有按键按下
  bool hasButtonPressed();
  bool readJoypad(ButtonID button);

  void handleSerial();
  void handleJoypad();

  void setAfterHandleJoypadCallback(std::function<void()> afterHandleJoypadCallback);

  void unsetAfterHandleJoypadCallback();

  // 存储当前按键状态的结构体
  typedef struct {
    uint16_t current_state; // 使用位掩码存储8个按键的状态
    uint16_t last_state; // 使用位掩码存储8个按键的状态
  } ButtonState;

  // 声明全局可访问的状态结构体（需要在某个 .cpp 文件中定义）
  ButtonState g_button_state;

private:
  std::function<void()> _afterHandleJoypadCallback;
  std::function<void()> _prevAfterHandleJoypadCallback;
  void setButtonPressed(ButtonID button);
#if ENABLE_INPUT == INPUT_PCF8574
  void initJoypadI2CIoExpander();
#elif ENABLE_INPUT == INPUT_GPIO
  void initJoypadGpios();

  const uint8_t button_pins[BTN_COUNT] = {
      PIN_UP, PIN_DOWN, PIN_LEFT, PIN_RIGHT, PIN_A, PIN_B, PIN_SELECT, PIN_START};

#endif
};