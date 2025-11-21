#pragma once
#if ENABLE_SOUND
#include "baseservice.h"
#include "i2s-audio.h"
#include "minigb_apu.h"
// Project headers
#include "hedley.h"

class SoundService {
public:
  SoundService();
  void initSound();

  void handleSoundLoop();
  uint8_t getVolume();
  void increaseVolume();
  void decreaseVolume();

protected:
};

#endif