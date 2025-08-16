#include "common.h"
#include "input.h"
#include "i2s-audio.h"

#include "gb.h"

extern i2s_config_t i2s_config;

static struct
{
  unsigned a : 1;
  unsigned b : 1;
  unsigned select : 1;
  unsigned start : 1;
  unsigned right : 1;
  unsigned left : 1;
  unsigned up : 1;
  unsigned down : 1;
} prev_joypad_bits;

#ifndef USE_PAD_GPIO
static PCF8574 pcf8574(0x20, 16, 17);
#endif


#ifdef USE_PAD_GPIO
void initJoypad() {
  gpio_set_function(GPIO_UP, GPIO_FUNC_SIO);
  gpio_set_function(GPIO_DOWN, GPIO_FUNC_SIO);
  gpio_set_function(GPIO_LEFT, GPIO_FUNC_SIO);
  gpio_set_function(GPIO_RIGHT, GPIO_FUNC_SIO);
  gpio_set_function(GPIO_A, GPIO_FUNC_SIO);
  gpio_set_function(GPIO_B, GPIO_FUNC_SIO);
  gpio_set_function(GPIO_SELECT, GPIO_FUNC_SIO);
  gpio_set_function(GPIO_START, GPIO_FUNC_SIO);

  gpio_set_dir(GPIO_UP, false);
  gpio_set_dir(GPIO_DOWN, false);
  gpio_set_dir(GPIO_LEFT, false);
  gpio_set_dir(GPIO_RIGHT, false);
  gpio_set_dir(GPIO_A, false);
  gpio_set_dir(GPIO_B, false);
  gpio_set_dir(GPIO_SELECT, false);
  gpio_set_dir(GPIO_START, false);

  gpio_pull_up(GPIO_UP);
  gpio_pull_up(GPIO_DOWN);
  gpio_pull_up(GPIO_LEFT);
  gpio_pull_up(GPIO_RIGHT);
  gpio_pull_up(GPIO_A);
  gpio_pull_up(GPIO_B);
  gpio_pull_up(GPIO_SELECT);
  gpio_pull_up(GPIO_START);
}
#else
void initJoypad() {
  for (int pin = 0; pin < 8; ++pin) {
    pcf8574.pinMode(pin, INPUT_PULLUP);
  }

  pcf8574.setLatency(5);
  
  if (pcf8574.begin()){
		Serial.println("PCF8574 initialized");
	}else{
		Serial.println("PCF8574 initialization failed");
    while (true) ;
	}
}
#endif

void handleSerial() {
  static uint64_t start_time = time_us_64();

  /* Serial monitor commands */
  int input = Serial.read();
  if (input <= 0) {
    return;
  }

  switch (input) {
  case 'i':
    gb.direct.interlace = !gb.direct.interlace;
    break;

  case 'f':
    gb.direct.frame_skip = !gb.direct.frame_skip;
    break;

  case 'b': {
    uint64_t end_time;
    uint32_t diff;
    uint32_t fps;

    end_time = time_us_64();
    diff = end_time - start_time;
    fps = ((uint64_t)frames * 1000 * 1000) / diff;
    Serial.printf("Frames: %u\n"
                  "Time: %lu us\n"
                  "FPS: %lu\n",
        frames, diff, fps);
    Serial.flush();
    frames = 0;
    start_time = time_us_64();
    break;
  }

  case '\n':
  case '\r': {
    gb.direct.joypad_bits.start = 0;
    break;
  }

  case '\b': {
    gb.direct.joypad_bits.select = 0;
    break;
  }

  case '8': {
    gb.direct.joypad_bits.up = 0;
    break;
  }

  case '2': {
    gb.direct.joypad_bits.down = 0;
    break;
  }

  case '4': {
    gb.direct.joypad_bits.left = 0;
    break;
  }

  case '6': {
    gb.direct.joypad_bits.right = 0;
    break;
  }

  case 'z':
  case 'w': {
    gb.direct.joypad_bits.a = 0;
    break;
  }

  case 'x': {
    gb.direct.joypad_bits.b = 0;
    break;
  }

  case 'q':
    reset();

  case 'p':
    nextPalette();

  default:
    break;
  }
}

void handlePad() {
  /* Update buttons state */
  prev_joypad_bits.up = gb.direct.joypad_bits.up;
  prev_joypad_bits.down = gb.direct.joypad_bits.down;
  prev_joypad_bits.left = gb.direct.joypad_bits.left;
  prev_joypad_bits.right = gb.direct.joypad_bits.right;
  prev_joypad_bits.a = gb.direct.joypad_bits.a;
  prev_joypad_bits.b = gb.direct.joypad_bits.b;
  prev_joypad_bits.select = gb.direct.joypad_bits.select;
  prev_joypad_bits.start = gb.direct.joypad_bits.start;

#ifdef USE_PAD_GPIO
  gb.direct.joypad_bits.up = gpio_get(PIN_UP);
  gb.direct.joypad_bits.down = gpio_get(PIN_DOWN);
  gb.direct.joypad_bits.left = gpio_get(PIN_LEFT);
  gb.direct.joypad_bits.right = gpio_get(PIN_RIGHT);
  gb.direct.joypad_bits.a = gpio_get(PIN_A);
  gb.direct.joypad_bits.b = gpio_get(PIN_B);
  gb.direct.joypad_bits.select = gpio_get(PIN_SELECT);
  gb.direct.joypad_bits.start = gpio_get(PIN_START);
#else
  #if 0
  Serial.printf("pins:\n");
  for (int i = 0; i < 8; ++i) {
    int in = pcf8574.digitalRead(i);
    Serial.printf(" %d", in);
  }
  Serial.printf("\n");
  #endif

  gb.direct.joypad_bits.up = pcf8574.digitalRead(PIN_UP);
  gb.direct.joypad_bits.down = pcf8574.digitalRead(PIN_DOWN);
  gb.direct.joypad_bits.left = pcf8574.digitalRead(PIN_LEFT);
  gb.direct.joypad_bits.right = pcf8574.digitalRead(PIN_RIGHT);
  gb.direct.joypad_bits.a = pcf8574.digitalRead(PIN_A);
  gb.direct.joypad_bits.b = pcf8574.digitalRead(PIN_B);
  gb.direct.joypad_bits.select = pcf8574.digitalRead(PIN_SELECT);
  gb.direct.joypad_bits.start = pcf8574.digitalRead(PIN_START);
#endif

  /* hotkeys (select + * combo)*/
  if (!gb.direct.joypad_bits.select) {
#if ENABLE_SOUND
    if (!gb.direct.joypad_bits.up && prev_joypad_bits.up) {
      /* select + up: increase sound volume */
      i2s_increase_volume(&i2s_config);
    }
    if (!gb.direct.joypad_bits.down && prev_joypad_bits.down) {
      /* select + down: decrease sound volume */
      i2s_decrease_volume(&i2s_config);
    }
#endif
    if (!gb.direct.joypad_bits.right && prev_joypad_bits.right) {
      /* select + right: select the next manual color palette */
      nextPalette();
    }
    if (!gb.direct.joypad_bits.left && prev_joypad_bits.left) {
      /* select + left: select the previous manual color palette */
      prevPalette();
    }
    if (!gb.direct.joypad_bits.start && prev_joypad_bits.start) {
      /* select + start: save ram and resets to the game selection menu */
#if ENABLE_SDCARD
      write_cart_ram_file(&gb);
#endif
      reset();
    }
    if (!gb.direct.joypad_bits.a && prev_joypad_bits.a) {
      /* select + A: enable/disable frame-skip => fast-forward */
      gb.direct.frame_skip = !gb.direct.frame_skip;
      Serial.printf("I gb.direct.frame_skip = %d\n", gb.direct.frame_skip);
    }
    if (!gb.direct.joypad_bits.b && prev_joypad_bits.b) {
      /* select + B: change scaling mode */
      scalingMode = (ScalingMode)(((int) scalingMode + 1) % ((int) ScalingMode::COUNT));
      union core_cmd cmd;
      cmd.cmd = CORE_CMD_IDLE_SET;
      multicore_fifo_push_blocking(cmd.full);
      Serial.printf("I Scaling mode: = %d\n", scalingMode);
    }
  }
}
