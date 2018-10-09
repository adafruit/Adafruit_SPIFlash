// Adafruit SPI Flash Total Erase
// Authors: Tony DiCola, Dan Halbert
//
// This example will perform a complete erase of ALL data on the SPI
// flash.  This is handy to reset the flash into a known empty state
// and fix potential filesystem or other corruption issues.
//
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!  NOTE: YOU WILL ERASE ALL DATA BY RUNNING THIS SKETCH!  !!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//
// Usage:
// - Modify the pins and type of fatfs object in the config
//   section below if necessary (usually not necessary).
// - Upload this sketch to your M0 express board.
// - Upon starting the board Neopixel will be blue
// - About 13 seconds later, the Neopixel should starting flashing
//   green once per second. This indicates the SPI flash has been
//   erased and all is well.
// - If the Nexopixel starts flashing red two or three times a second,
//   an error has occurred.

#include <SPI.h>
#include <Adafruit_SPIFlash.h>
#include <Adafruit_SPIFlash_FatFs.h>
#include <Adafruit_NeoPixel.h>

// Configuration of the flash chip pins.
// You don't normally need to change these if using a Feather/Metro
// M0 express board.
#define FLASH_TYPE     SPIFLASHTYPE_W25Q16BV  // Flash chip type.
                                              // If you change this be
                                              // sure to change the fatfs
                                              // object type below to match.

#if !defined(SS1)
  #define FLASH_SS       SS                    // Flash chip SS pin.
  #define FLASH_SPI_PORT SPI                   // What SPI port is Flash on?
  #define NEOPIN         8
#else
  #define FLASH_SS       SS1                    // Flash chip SS pin.
  #define FLASH_SPI_PORT SPI1                   // What SPI port is Flash on?
  #define NEOPIN         40
#endif

Adafruit_SPIFlash flash(FLASH_SS, &FLASH_SPI_PORT);     // Use hardware SPI
// Alternatively you can define and use non-SPI pins!
//Adafruit_SPIFlash flash(SCK1, MISO1, MOSI1, FLASH_SS);

// On-board status Neopixel.
Adafruit_NeoPixel pixel = Adafruit_NeoPixel(1, NEOPIN, NEO_GRB + NEO_KHZ800);
uint32_t BLUE = pixel.Color(0, 0, 100);
uint32_t GREEN = pixel.Color(0, 100, 0);
uint32_t RED = pixel.Color(100, 0, 0);
uint32_t OFF = pixel.Color(0, 0, 0);

void setup() {
  // Start with a blue pixel.
  pixel.begin();
  pixel.setBrightness(30);
  pixel.setPixelColor(0, BLUE);
  pixel.show();

  // Initialize flash library and check its chip ID.
  if (!flash.begin(FLASH_TYPE)) {
    // blink red
    blink(2, RED);
  }
  if (!flash.EraseChip()) {
    blink(3, RED);
  }
  blink(1, GREEN);
}

void loop() {
  // Nothing to do in the loop.
  delay(100);
}

void blink(int times, uint32_t color) {
  while (1) {
    for (int i = 0; i < times; i++) {
      pixel.setPixelColor(0, color);
      pixel.show();
      delay(100);
      pixel.setPixelColor(0, OFF);
      pixel.show();
      delay(100);
    }
    delay(1000);
  }

}

