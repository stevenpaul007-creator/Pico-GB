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
  void setAudioCallback(std::function<void(void *userdata, int16_t *stream, size_t len)> audioCallback);

private:

  std::function<void(void *userdata, int16_t *stream, size_t len)> _audioCallback;
protected:
};

#endif