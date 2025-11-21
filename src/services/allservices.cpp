#include "allservices.h"

Services::Services() {
}

void Services::initAll() {
  inputService.buttonServiceInit();
#if ENABLE_SDCARD
  cardService.initSDCard();
#endif
#if ENABLE_SOUND
  soundService.initSound();
#endif
}

Services srv;