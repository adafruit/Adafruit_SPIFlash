/*
 * Print size, modify date/time, and name for all files in root.
 */
#include <SPI.h>
#include "SdFat.h"
#include "Adafruit_SPIFlash.h"

#if defined(__SAMD51__) || defined(NRF52840_XXAA)
  Adafruit_FlashTransport_QSPI flashTransport(PIN_QSPI_SCK, PIN_QSPI_CS, PIN_QSPI_IO0, PIN_QSPI_IO1, PIN_QSPI_IO2, PIN_QSPI_IO3);
#else
  #if (SPI_INTERFACES_COUNT == 1)
    Adafruit_FlashTransport_SPI flashTransport(SS, &SPI);
  #else
    Adafruit_FlashTransport_SPI flashTransport(SS1, &SPI1);
  #endif
#endif

Adafruit_SPIFlash flash(&flashTransport);

// file system object from SdFat
FatFileSystem fatfs;

FatFile root;
FatFile file;

//------------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // Init external flash
  flash.begin();

  // Init file system on the flash
  fatfs.begin(&flash);
  
  // Wait for USB Serial 
  while (!Serial) {
    SysCall::yield();
  }
  
  if (!root.open("/")) {
    Serial.println("open root failed");
  }
  // Open next file in root.
  // Warning, openNext starts at the current directory position
  // so a rewind of the directory may be required.
  while (file.openNext(&root, O_RDONLY)) {
    file.printFileSize(&Serial);
    Serial.write(' ');
    file.printModifyDateTime(&Serial);
    Serial.write(' ');
    file.printName(&Serial);
    if (file.isDir()) {
      // Indicate a directory.
      Serial.write('/');
    }
    Serial.println();
    file.close();
  }
  
  if (root.getError()) {
    Serial.println("openNext failed");
  } else {
    Serial.println("Done!");
  }
}
//------------------------------------------------------------------------------
void loop() {}
