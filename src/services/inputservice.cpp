#include "inputservice.h"

InputService::InputService() {
}

void InputService::buttonServiceInit() {
  Serial.println("I init button service");
#if ENABLE_INPUT == INPUT_PCF8574
  initJoypadI2CIoExpander();
#elif ENABLE_INPUT == INPUT_GPIO
  initJoypadGpios();
#endif
  Serial.println("I init button service end");
}

bool InputService::isButtonPressed(ButtonID button) {
  return (g_button_state.current_state >> button) & 1U;
}

bool InputService::isButtonPressedFirstTime(ButtonID button) {
  return (g_button_state.current_state >> button) & 1U
      && (g_button_state.last_state >> button) & 1U;
}

bool InputService::hasButtonPressed() {
  return g_button_state.current_state != 0;
}

#if ENABLE_INPUT == INPUT_PCF8574
PCF8574 pcf8574(PCF8574_ADDR, PCF8574_SDA, PCF8574_SCL);

void InputService::initJoypadI2CIoExpander() {
  for (int pin = 0; pin < 8; ++pin) {
    pcf8574.pinMode(pin, INPUT_PULLUP);
  }

  pcf8574.setLatency(5);

  if (!pcf8574.begin()) {
    Serial.println("PCF8574 initialization failed");
    reset();
  }
}

bool InputService::readJoypad(uint8_t pin) {
  return pcf8574.digitalRead(pin);
}

#elif ENABLE_INPUT == INPUT_GPIO
void InputService::initJoypadGpios() {
  for (int i = 0; i < BTN_COUNT; i++) {
    // gpio_set_function(button_pins[i], GPIO_FUNC_SIO);
    gpio_init(button_pins[i]);
    gpio_set_dir(button_pins[i], GPIO_IN);
    gpio_pull_up(button_pins[i]); // 假设按键按下时连接到 GND
  }
}

bool InputService::readJoypad(ButtonID button) {
  return gpio_get(button_pins[button]);
}

void InputService::handleSerial() {
  static uint64_t start_time = time_us_64();

  /* Serial monitor commands */
  int input = Serial.read();
  if (input <= 0) {
    return;
  }

  switch (input) {
  case 'b': {
    uint64_t end_time;
    uint32_t diff;
    uint32_t fps;

    end_time = time_us_64();
    diff = end_time - start_time;
    fps = ((uint64_t)frames * 1000 * 1000) / diff;
    Serial.printf("Frames: %u\tTime: %lu us\tFPS: %lu\r\n",
        frames, diff, fps);
    Serial.flush();
    frames = 0;
    start_time = time_us_64();
    break;
  }

  case '\n':
  case '\r': {
    setButtonPressed(ButtonID::BTN_START);
    break;
  }

  case 'w': {
    setButtonPressed(ButtonID::BTN_SELECT);
    break;
  }

  case '8': {
    setButtonPressed(ButtonID::BTN_UP);
    break;
  }

  case '2': {
    setButtonPressed(ButtonID::BTN_DOWN);
    break;
  }

  case '4': {
    setButtonPressed(ButtonID::BTN_LEFT);
    break;
  }

  case '6': {
    setButtonPressed(ButtonID::BTN_RIGHT);
    break;
  }

  case 'z': {
    setButtonPressed(ButtonID::BTN_A);
    break;
  }

  case 'x': {
    setButtonPressed(ButtonID::BTN_B);
    break;
  }

  case 'c': {
    Serial.println(g_button_state.current_state, BIN);
    break;
  }

  case 'q':
    reset();

  default:
    break;
  }
  if (_afterHandleJoypadCallback) {
    _afterHandleJoypadCallback();
  }
}

void InputService::handleJoypad() {
  g_button_state.last_state = g_button_state.current_state;
  uint16_t new_state = 0;

  for (int i = 0; i < BTN_COUNT; i++) {
    if (!gpio_get(button_pins[i])) {
      new_state |= (1U << i); // 设置对应的位为1（按下）
    }
  }
  // 更新全局状态，如果需要多线程保护，可以加锁或禁用中断，但通常轮询不需要
  g_button_state.current_state = new_state;

  if (_afterHandleJoypadCallback) {
    _afterHandleJoypadCallback();
  }
}
void InputService::setAfterHandleJoypadCallback(std::function<void()> afterHandleJoypadCallback) {
  if (!afterHandleJoypadCallback) {
    return;
  }
  if (_afterHandleJoypadCallback) {
    _prevAfterHandleJoypadCallback = _afterHandleJoypadCallback;
  }
  _afterHandleJoypadCallback = afterHandleJoypadCallback;
}
void InputService::unsetAfterHandleJoypadCallback() {
  if (_prevAfterHandleJoypadCallback) {
    _afterHandleJoypadCallback = _prevAfterHandleJoypadCallback;
  }
}
void InputService::setButtonPressed(ButtonID button) {
  g_button_state.current_state |= (1U << button);
}

#endif
