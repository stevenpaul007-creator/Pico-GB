#include "gbinput.h"

#include "allmenus.h"
#include "allservices.h"

#define PALETTE_COUNT 15

GBInput::GBInput() {
}
void GBInput::afterHandleJoypadCallback() {
  gb.direct.joypad_bits.up = !PRESSED_KEY(ButtonID::BTN_UP);
  gb.direct.joypad_bits.down = !PRESSED_KEY(ButtonID::BTN_DOWN);
  gb.direct.joypad_bits.left = !PRESSED_KEY(ButtonID::BTN_LEFT);
  gb.direct.joypad_bits.right = !PRESSED_KEY(ButtonID::BTN_RIGHT);
  gb.direct.joypad_bits.a = !PRESSED_KEY(ButtonID::BTN_A);
  gb.direct.joypad_bits.b = !PRESSED_KEY(ButtonID::BTN_B);
  gb.direct.joypad_bits.select = !PRESSED_KEY(ButtonID::BTN_SELECT);
  gb.direct.joypad_bits.start = !PRESSED_KEY(ButtonID::BTN_START);

  if (PRESSED_KEY(ButtonID ::BTN_SELECT)) {
    if (srv.inputService.isButtonPressedFirstTime(ButtonID::BTN_START)) {
      gameMenu.openMenu();
    }
    if (srv.inputService.isButtonPressedFirstTime(ButtonID::BTN_B)) {
      /* select + B: change scaling mode */
      scalingMode = (ScalingMode)(((int)scalingMode + 1) % ((int)ScalingMode::COUNT));
      union core_cmd cmd;
      cmd.cmd = CORE_CMD_IDLE_SET;
      multicore_fifo_push_blocking(cmd.full);
      Serial.printf("I Scaling mode: = %d\n", scalingMode);
    }
  }
}
void GBInput::initJoypad() {
  srv.inputService.setAfterHandleJoypadCallback(
      std::bind(&GBInput::afterHandleJoypadCallback, this));

  gameMenu.setApplyColorSchemeCallback(std::bind(&GBInput::applyColorSchemeCallback, this));
  gameMenu.setSaveRealtimeGameCallback(std::bind(&GBInput::saveRealtimeGameCallback, this));
  gameMenu.setLoadRealtimeGameCallback(std::bind(&GBInput::loadRealtimeGameCallback, this));
  gameMenu.setSaveRamCallback(std::bind(&GBInput::saveRamCallback, this));
  gameMenu.setLoadRamCallback(std::bind(&GBInput::loadRamCallback, this));
  gameMenu.setRestartGameCallback(std::bind(&GBInput::restartGameCallback, this));
}

void GBInput::updatePalette() {
  if (palette_selected == 0) {
    char rom_title[16];
    auto_assign_palette(palette, gb_colour_hash(&gb), gb_get_rom_name(&gb, rom_title));
  } else {
    manual_assign_palette(palette, palette_selected - 1);
  }
}

void GBInput::nextPalette() {
  palette_selected = (palette_selected + 1) % PALETTE_COUNT;
  updatePalette();
}

void GBInput::shutdown() {
  srv.inputService.unsetAfterHandleJoypadCallback();
  gameMenu.setApplyColorSchemeCallback(nullptr);
  gameMenu.setSaveRealtimeGameCallback(nullptr);
  gameMenu.setLoadRealtimeGameCallback(nullptr);
  gameMenu.setSaveRamCallback(nullptr);
  gameMenu.setLoadRamCallback(nullptr);
  gameMenu.setRestartGameCallback(nullptr);
}

void GBInput::prevPalette() {
  palette_selected = (palette_selected + PALETTE_COUNT - 1) % PALETTE_COUNT;
  updatePalette();
}

void GBInput::handleSerial() {
  srv.inputService.handleSerial();
}

void GBInput::handleJoypad() {
  srv.inputService.handleJoypad();
}

void GBInput::applyColorSchemeCallback() {
  nextPalette();
}
void GBInput::saveRealtimeGameCallback() {
  srv.cardService.save_state(&gb);
}
void GBInput::loadRealtimeGameCallback() {
  srv.cardService.load_state(&gb);
}
void GBInput::saveRamCallback() {
  srv.cardService.write_cart_ram_file(&gb);
}
void GBInput::loadRamCallback() {
  srv.cardService.read_cart_ram_file(&gb);
}
void GBInput::restartGameCallback() {
  gb_reset();
}
