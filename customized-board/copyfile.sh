rm -fr ~/.platformio/packages/framework-arduinopico/variants/generic_rp2350_2M_PSRAM_CS0
cp ./customized-board/pin_arduino.h ~/.platformio/packages/framework-arduinopico/variants/generic_rp2350_2M_PSRAM_CS0/
cp ./customized-board/generic_rp2350_2M_PSRAM_CS0.json ~/.platformio/platforms/raspberrypi@src-330cdbbd6fda0e3245392129791aab62/boards/
