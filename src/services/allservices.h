#pragma once
#include "inputservice.h"
#include "tfcardservice.h"
#include "soundservice.h"

#define PRESSED_KEY(x) (srv.inputService.isButtonPressed(x))

class Services {
public:
  Services();
  void initAll();

  InputService inputService;
  CardService cardService;
  SoundService soundService;
};

extern Services srv;
