# Guition JC8048W550

5" 800x480 RGB panel board, marked **JC8048W550** (ESP32-S3, 16 MB flash,
8 MB PSRAM, CH340 serial bridge).

| Block | Part | Pins |
|---|---|---|
| Display | 800x480 RGB565, no DISP pin | DE 40, VSYNC 41, HSYNC 39, PCLK 42 (16 MHz, falling edge) |
| Display data | B0-B4 / G0-G5 / R0-R4 | B 8,3,46,9,1  G 5,6,7,15,16,4  R 45,48,47,21,14 |
| Backlight | LEDC PWM | 2 |
| Touch | GT911, I2C0 | SDA 19, SCL 20, RST 38, INT not wired |
| Speaker | I2S amplifier on the speaker connector | BCLK 0, LRC 18, DOUT 17 |
| SD card (unused) | SPI | CS 10, MOSI 11, SCK 12, MISO 13 |

## Where the pins come from

The vendor demo package (Arduino demos for the panel, GT911 and audio) and the
community ESPHome definition of this board. There is no schematic, so **none of
it has been measured on hardware**. Two things to check first:

- Colours: the ESPHome definition inverts colours, the vendor Arduino demo does
  not. `esp_lcd` RGB panels cannot invert, so this build does not.
- PSRAM: the build uses the project default (octal, 80 MHz). If the boot log
  reports a PSRAM failure, the module has a different PSRAM.

## Verified on hardware

Display, backlight and speaker work. Two things were needed to get a picture:

- hsync/vsync idle low (the vendor demo's polarity 0).
- Backlight PWM at 1220 Hz. The stock 25 kHz default left the screen dark.

Touch initialises (GT911 at 0x5D); its coordinates and the microphone path are
not checked.

## Build

```sh
python scripts/build.py guition/jc8048w550
```

## Audio

**No microphone.** The vendor pin tables define none, so the board only plays
sound; voice input does not work until one is added. The microphone pins in
`config.h` are `GPIO_NUM_NC`. The free pins are the SD-card lines (IO10-13, if
the card slot stays empty).

The vendor `Audio_test` demo puts the speaker bit clock on IO19, but IO19 is the
touch SDA line. The demo that runs both together uses IO0, which this board
definition uses. IO0 is also the BOOT switch, so there is no button input.

## Starting a conversation

A tap anywhere on the screen toggles listening, as on the JC4827W543.

## Flashing

The CH340 on this board uses the normal auto-reset: flash over the serial port
(COM port on Windows) with `idf.py -p <port> flash`.
