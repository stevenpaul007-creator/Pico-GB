# RP2040-GB for Pico-GB

This is a fork of [YouMakeTech's Pico-GB Game Boy emulator](https://github.com/YouMakeTech/Pico-GB) which again is a fork of the [Peanut-GB based RP2040-GB Game Boy (DMG) emulator from deltabeard](https://github.com/deltabeard/Peanut-GB).
In addition to YouMakeTech and Deltabeards amazing work, this fork adds some improvements to the emulator like
* support real time state save and load to SD card. Meaning you could continue your game after a cold power off and power on. THIS IS AMAZING.
* support for a larger variety of displays by using [Bodmer TFT_eSPI library](https://github.com/Bodmer/TFT_eSPI)
* scaling modes that can be toggled with `Select` + `B`. Note that no integer scaling is possible on this low resolution screens. The modes are:
  * full height with stretching to screen's width. Some columns/lines are doubled in this mode
  * full height with width scaled to original aspect ratio. Some columns/lines are doubled in this mode
  * original size (160x144 px, no scaling/stretching)
    <div style="display: flex; flex-direction: row; margin: 2em">
      <div style="width: 50%; text-align: center;">
        <div><img src="doc/mode-scaled-aspect.jpg" height="300" alt="Scaled with correct aspect ratio)"></div>
        <span>Scaled with correct aspect ratio</span>
      </div>
      <div style="width: 50%; text-align: center;">
        <div><img src="doc/mode-stretched.jpg" height="200" alt="Stretched"></div>
        <span>Stretched</span>
      </div>
      <div style="width: 50%; text-align: center;">
        <div><img src="doc/mode-original.jpg" height="200" alt="Original size (160x144 px, no scaling/stretching)"></div>
        <span>Original size (160x144 px, no scaling/stretching)</span>
      </div>
    </div>
* gamma correction (value hard-coded but changeable in source-code) for color correction of the used display
* support for Game Boy Color games, thanks to (froggestspirit's CGB branch of Peanut-GB)[https://github.com/deltabeard/Peanut-GB/tree/cgb] that unfortunately is not merged yet to Peanut-GB main branch
    <div style="width: 50%; text-align: center;">
      <div><img src="doc/color.jpg" height="200" alt="Color"></div>
    </div>
* migration to [PlatformIO](https://platformio.org/) for easier development and integration of third-party libraries
* support for I2C IO expanders in case you want to use a fast 16-bit LCD display
* double frame buffer costs about 300KB in ram (320*240*2 = 150KB each)

It also includes the changes done by [YouMakeTech](https://github.com/YouMakeTech/Pico-GB):
* push buttons support
* overclocking to 266MHz for more accurate framerate (~60 FPS)
* I2S sound support (44.1kHz 16 bits stereo audio)
* SD card support (store roms and save games) + game selection menu
* automatic color palette selection for some games (emulation of Game Boy Color Bootstrap ROM) + manual color palette selection
* PSRAM (APS6404L) is supported if your flash is smaller than your rom file. If you have on board psram, set ENABLE_RP2040_PSRAM=1 and select your board and maybe delete RP2350_PSRAM_CS in common.h. If you are willing to use an external psram chip, set ENABLE_EXT_PSRAM=1.

# Videos by YouMakeTech
* [Let's build a Game Boy Emulator on a Breadboard!](https://youtu.be/ThmwXpIsGWs)
* [Build the ULTIMATE GameBoy Emulator for Raspberry Pi Pico](https://youtu.be/yauNQSS6nC4)

# Hardware
## What you need
* (1x) Raspberry Pi Pico 2
  * Note: a Pico 1 should also work as long as you do not want to use GB Color games. As the price difference is so small, I would not recommend to use a Pico 1 for this project
* (1x) An LCD screen, e.g. an 2.8" 320x240 ILI9341-based LCD Display Module works nicely.
  * Note that SPI displays might be too slow at this size. You might be better off with an 8-bit or 16-bit parallel display, where 8-bit might be the perfect tradeoff between speed and number of pins used.
  * At least you will not be able to use a 16-bit parallel display without an I2C IO expander, e.g. PCF8574.
  * smaller LCDs like the 176x220 one used in YouMakeTech's original project also work, but I did not implement any scaling for it
  * larger ones like 480x320 screens should also work. They might be interesting as they allow integer scaling (2x native resolution: 320x288 pixels). No scaling mode was implemented for this so far.
    * Do yourself a favor and do not use one with an SPI controller as the framerate might be really low. I tried a 3.5" SPI one that was designed for the Raspberry Pi and only got 20 FSP out of it.
    * spi with dma should be fine and should easily give 60fps.
* (1x) SD card reader, like [this one](https://www.androegg.de/shop/esp8266-stm-32-arduino-spi-kartenleser-33v/) - if the LCD board does not already have one built in
* (1x) FAT 32 formatted Micro SD card with roms you legally own. Roms must have the .gb or .gbc extension and must be copied to the root folder.
* (1x) MAX98357A amplifier
* (1x) 2W 8ohms speaker
* (8x) Micro Push Button Switch, Momentary Tactile Tact Touch, 6x6x6 mm, 4 pins
  * (optional - 1x) I2C IO expander (PCF8574) for the buttons if you use a 16-bit parallel LCD
* (1x) PCB or Breadboard
* Dupont Wires Assorted Kit (Male to Female + Male to Male + Female to Female)
* Preformed Breadboard Jumper Wires
* (optional) A APS6404L is required if your flash is smaller than your rom file. Like my Pico 2 with a 2MB flash, I could only load a rom within 1MB. If you would like to load a bigger rom, you can add a APS6404 to SD card IOs with another CS pin. And modify ENABLE_EXT_PSRAM to 1, MAX_ROM_SIZE_MB to what ever depends on your flash size(1.5mb is default for 2mb).

Last but not least: you might want a shell.
* You might like the one from YouMakeTech: [Pico-GB 3d-printed Game Boy emulator handheld gaming console for Raspberry Pi Pico](https://www.youmaketech.com/pico-gb-gameboy-emulator-handheld-for-raspberry-pi-pico/) that ressembles to the original Nintendo Game Boy released in 1989.
  * See [Pico-GB assembly instructions, circuit diagrams, 3d printed files etc.](https://www.youmaketech.com/pico-gb-gameboy-emulator-handheld-for-raspberry-pi-pico/)
* Or just build your own like I did (I just modified a cheap retro arcade with "homebrew" games)

# Pinout
You must select your pinout via the tft-espi-config/tft_setup.h (for the display) and common.h (audio, input, sd-card) files.

* I2C IO Expander (PCF8574), NOT USED OR TESTED
  * SDA = NC
  * SCL = NC
  * UP / DOWN / LEFT / RIGHT / BUTTON A + B / SELECT / START = I2C IO Expander pins 0-7
* Buttons
  * UP = GP17
  * DOWN = GP19
  * LEFT = GP16
  * RIGHT = GP18
  * A = GP21
  * B = GP20
  * SELECT = GP22
  * START = GP26
* SD, using SPI1
  * CS = GP13
  * CSK = GP14
  * MOSI = GP15
  * MISO = GP12
* PSRAM(APS6404L), using SPI1, NOT ON BOARD PSARM
  * CS = GP0
  * CSK = GP14
  * MOSI = GP15
  * MISO = GP12
* On-board PSRAM
  * CS = GP0  See [CS (QMI CS1n) which can be used on these pins: GPIO 0, 8, 19, (47)](https://forums.raspberrypi.com/viewtopic.php?t=384076#p2295447)
* LCD, using SPI0
  * SCLK = GP2
  * MOSI = GP3
  * CS = GP4
  * DC/RS = GP7
  * RST = GP8
  * LED = not connected, connect to 3.3V instead (could be connected to GP25 for PWM, but not implemented yet)
* I2S Audio (MAX98357A), using PIO, by the way.
  * DIN = GP9
  * BCLK = GP10
  * LRC = GP11

As there is no pin left, reading the Display for VSYNC is not possible, so screen tearing might occur.

# Flashing the firmware
As the project has to be configured to the display you want to use, there is no precompiled binary or UF2. You have to config the project first and then build and flash the firmware on your own with PlatformIO. See the next section.

# Building from source
PlatformIO is required to build this project. It can be installed as an extension to Visual Studio Code.
When PlatformIO is installed, just open the project in Visual Studio Code. It should be detected as a PlatformIO project. Now select the "pico2" environment. Then build and flash it.

## pio env
* pico: for pico and rp2040 chip. not tested.
* pico2: for pico 2 and rp2350 chip. No psram. Copy rom file to flash directly.
* pico2-extpsram: for pico 2 and rp2350 chip. External psram attached to SPI1(see Pinout). Copy first 1.5MB of rom file to flash and the rest to psram. External psram seems a little slow for random read byte for rom.
* pico-psram: for pico 2 and rp2350 chip. On board psram is used. Copy whole rom file to psram(4MB max). 

# Preparing the SD card
The SD card is used to store game roms and save game progress. For this project, you will need a FAT 32 formatted Micro SD card with roms you legally own. Roms must have the .gb/.gbc extension.

* Insert your SD card in a Windows computer and format it as FAT 32
* Copy your .gb and/or .gbc files to the SD card root folder (subfolders are not supported at this time)
* Insert the SD card into the SD card slot

# Known issues and limitations
* No copyrighted games are included with Pico-GB / RP2040-GB. For this project, you will need a FAT 32 formatted Micro SD card with roms you legally own. Roms must have the .gb extension.
* The RP2040-GB emulator is able to run at full speed on the Pico, at the expense of emulation accuracy. Some games may not work as expected or may not work at all. RP2040-GB is still experimental and not all features are guaranteed to work.
* RP2040-GB is only compatible with [original Game Boy DMG games](https://en.wikipedia.org/wiki/List_of_Game_Boy_games) (not compatible with Game Boy Color or Game Boy Advance games)
* Repeatedly flashing your Pico will eventually wear out the flash memory (Pico is qualified for min. 100K flash/erase cycles)
* The emulator overclocks the Pico in order to get the emulator working fast enough. Overclocking can reduce the Pico’s lifespan.
* Use this software and instructions at your own risk! I will not be responsible in any way for any damage to your Pico and/or connected peripherals caused by using this software. I also do not take responsibility in any way when damage is caused to the Pico or display due to incorrect wiring or voltages.

# In-game key combos
* select + up = volume up
* select + down = volume down
* select + left = next palette
* select + right = prev palette
* select + start = restart (to rom list)
* select + B = screen scale mode
* select + A = frame skip mode and turn off sound
* start + left = save ram (to /SAVES/)
* start + right = load ram
* left + start = in-game reset
* left + select = save state. To ram and SD card. Need /rtsav/ folder created first.
* right + select = load state. From ram first or from SD card.
* hold select + power on = flash mode
Willing change to in-game menu.

# License
MIT
