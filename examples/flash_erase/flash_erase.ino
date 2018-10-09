// Adafruit SPI Flash Total Erase
// Author: Tony DiCola
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
// - Open the serial monitor at 115200 baud.  You should see a
//   prompt to confirm formatting.  If you don't see the prompt
//   close the serial monitor, press the board reset buttton,
//   wait a few seconds, then open the serial monitor again.
// - Type OK and enter to confirm the format when prompted,
//   the flash chip will be erased.
#include <SPI.h>
#include <Adafruit_SPIFlash.h>

// Configuration of the flash chip pins and flash fatfs object.
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


void setup() {
  // Initialize serial port and wait for it to open before continuing.
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  Serial.println("Adafruit SPI Flash Total Erase Example");

  // Initialize flash library and check its chip ID.
  if (!flash.begin(FLASH_TYPE)) {
    Serial.println("Error, failed to initialize flash chip!");
    while(1);
  }
  Serial.print("Flash chip JEDEC ID: 0x"); Serial.println(flash.GetJEDECID(), HEX);

  // Wait for user to send OK to continue.
  Serial.setTimeout(30000);  // Increase timeout to print message less frequently.
  do {
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    Serial.println("This sketch will ERASE ALL DATA on the flash chip!");
    Serial.println("Type OK (all caps) and press enter to continue.");
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
  }
  while (!Serial.find("OK"));

  Serial.println("Erasing flash chip in 10 seconds...");
  Serial.println("Note you will see stat and other debug output printed repeatedly.");
  Serial.println("Let it run for ~30 seconds until the flash erase is finished.");
  Serial.println("An error or success message will be printed when complete.");
  delay(10000L);
  if (!flash.EraseChip()) {
    Serial.println("Failed to erase chip!");
  }
  else {
    Serial.println("Successfully erased chip!");
  }
}

void loop() {
  // Nothing to do in the loop.
  delay(100);
}
