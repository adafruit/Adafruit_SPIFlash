// The MIT License (MIT)
// Copyright (c) 2019 Ha Thach for Adafruit Industries

#include "SdFat.h"
#include "Adafruit_SPIFlash.h"

#if 1
// On-board external flash (QSPI or SPI) macros should already
// defined in your board variant if supported
// - EXTERNAL_FLASH_USE_QSPI
// - EXTERNAL_FLASH_USE_CS/EXTERNAL_FLASH_USE_SPI
#if defined(EXTERNAL_FLASH_USE_QSPI)
  Adafruit_FlashTransport_QSPI flashTransport;

#elif defined(EXTERNAL_FLASH_USE_SPI)
  Adafruit_FlashTransport_SPI flashTransport(EXTERNAL_FLASH_USE_CS, EXTERNAL_FLASH_USE_SPI);

#else
  #error No QSPI/SPI flash are defined on your board variant.h !
#endif

#else
  Adafruit_FlashTransport_SPI flashTransport(A5, SPI);

#endif

Adafruit_SPIFlash flash(&flashTransport);

#define BUFSIZE   4096

uint8_t bufwrite[BUFSIZE];
uint8_t bufread[BUFSIZE];

// the setup function runs once when you press reset or power the board
void setup()
{
  Serial.begin(115200);
  while ( !Serial ) delay(100);   // wait for native usb

  flash.begin();

  Serial.println("Adafruit Serial Flash Info example");
  Serial.print("JEDEC ID: "); Serial.println(flash.getJEDECID(), HEX);
  Serial.print("Flash size: "); Serial.println(flash.size());

  write_and_compare(0xAA);
}

void print_speed(const char* text, uint32_t count, uint32_t ms)
{
  Serial.print(text);
  Serial.print(count);
  Serial.print(" bytes in ");
  Serial.print(ms / 1000.0F, 2);
  Serial.println(" seconds.");

  Serial.print("Speed : ");
  Serial.print( (count / 1000.0F) / (ms / 1000.0F), 2);
  Serial.println(" KB/s.\r\n");
}

bool write_and_compare(uint8_t pattern)
{
  uint32_t ms;
  uint32_t const flash_sz = flash.size();

  Serial.println("Erase chip");
  Serial.flush();
  flash.eraseChip();

  // write all
  memset(bufwrite, (int) pattern, sizeof(bufwrite));
  Serial.printf("Write flash with 0x%02X\n", pattern);
  Serial.flush();
  ms = millis();

  for(uint32_t addr = 0; addr < flash_sz; addr += sizeof(bufwrite))
  {
    flash.writeBuffer(addr, bufwrite, sizeof(bufwrite));
  }

  uint32_t ms_write = millis() - ms;
  print_speed("Write ", flash_sz, ms_write);
  Serial.flush();

  // read and compare
  Serial.println("Read flash and compare");
  Serial.flush();
  ms = millis();
  for(uint32_t addr = 0; addr < flash_sz; addr += sizeof(bufread))
  {
    flash.readBuffer(addr, bufread, sizeof(bufread));

    if ( memcmp(bufwrite, bufread, BUFSIZE) )
    {
      Serial.printf("Error: flash contents mismatched at address 0x08X!!!", addr);
      return false;
    }
  }

  uint32_t ms_read = millis() - ms;
  print_speed("Read  ", flash_sz, ms_read);
  Serial.flush();

  return true;
}

void loop()
{
  // nothing to do
}
