// Adafruit SPI Flash FatFs Format Example
// Author: Tony DiCola
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
// - Type OK and enter to confirm the format when prompted.
// - Partitioning and formatting will take about 30-60 seconds.
//   Once formatted a message will be printed to notify you that
//   it is finished.
//
#include <SPI.h>
#include <SdFat.h>
#include <Adafruit_SPIFlash.h>

#if defined(__SAMD51__) || defined(NRF52840_XXAA)
  Adafruit_FlashTransport_QSPI flashTransport(PIN_QSPI_SCK, PIN_QSPI_CS, PIN_QSPI_IO0, PIN_QSPI_IO1, PIN_QSPI_IO2, PIN_QSPI_IO3);
#else
  #if (SPI_INTERFACES_COUNT == 1)
    Adafruit_FlashTransport_SPI flashTransport(SS0, &SPI);
  #else
    Adafruit_FlashTransport_SPI flashTransport(SS1, &SPI1);
  #endif
#endif

Adafruit_SPIFlash flash(&flashTransport);

// file system object
FatFileSystem fatfs;


void setup() {
  // Initialize serial port and wait for it to open before continuing.
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  Serial.println("Adafruit SPI Flash FatFs Format Example");

  // Initialize flash library and check its chip ID.
  if (!flash.begin()) {
    Serial.println("Error, failed to initialize flash chip!");
    while(1);
  }
  Serial.print("Flash chip JEDEC ID: 0x"); Serial.println(flash.getJEDECID(), HEX);

  // Wait for user to send OK to continue.
  Serial.setTimeout(30000);  // Increase timeout to print message less frequently.
  do {
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    Serial.println("This sketch will ERASE ALL DATA on the flash chip and format it with a new filesystem!");
    Serial.println("Type OK (all caps) and press enter to continue.");
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
  }
  while (!Serial.find("OK"));

  // Call fatfs begin and passed flash object to initialize file system
  fatfs.begin(&flash);

  // Use wipe() for no dot progress indicator.
  if ( !fatfs.wipe(&Serial) )
  {
    Serial.println("Failed to wipe");
    while(1) delay(1);
  }

  // Must reinitialize after wipe.
  if (!fatfs.begin(&flash)) {
    Serial.println("Error, failed to mount newly formatted filesystem!");
    while(1) delay(1);
  }

  // Done!
  Serial.println("Flash chip successfully formatted with new empty filesystem!");
}

void loop() {
  // Nothing to be done in the main loop.
  delay(100);
}
