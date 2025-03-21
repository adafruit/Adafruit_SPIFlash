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
//   close the serial monitor, press the board reset button,
//   wait a few seconds, then open the serial monitor again.
// - Type OK and enter to confirm the format when prompted,
//   the flash chip will be erased.
#include "SdFat_Adafruit_Fork.h"
#include <SPI.h>

#include <Adafruit_SPIFlash.h>

// for flashTransport definition
#include "flash_config.h"

Adafruit_SPIFlash flash(&flashTransport);

void setup() {
  // Initialize serial port and wait for it to open before continuing.
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  Serial.println("Adafruit SPI Flash Total Erase Example");

  // Initialize flash library and check its chip ID.
  if (!flash.begin()) {
    Serial.println("Error, failed to initialize flash chip!");
    while (1) {
    }
  }
  Serial.print("Flash chip JEDEC ID: 0x");
  Serial.println(flash.getJEDECID(), HEX);

  // Wait for user to send OK to continue.
  // Increase timeout to print message less frequently.
  Serial.setTimeout(30000);
  do {
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
                   "!!!!!!!!!!");
    Serial.println("This sketch will ERASE ALL DATA on the flash chip!");
    Serial.println("Type OK (all caps) and press enter to continue.");
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
                   "!!!!!!!!!!");
  } while (!Serial.find((char *)"OK"));

  Serial.println("Erasing flash chip in 10 seconds...");
  Serial.println(
      "Note you will see stat and other debug output printed repeatedly.");
  Serial.println(
      "Let it run for ~30 seconds until the flash erase is finished.");
  Serial.println("An error or success message will be printed when complete.");

  if (!flash.eraseChip()) {
    Serial.println("Failed to erase chip!");
  }

  flash.waitUntilReady();
  Serial.println("Successfully erased chip!");
}

void loop() {
  // Nothing to do in the loop.
  delay(100);
}
