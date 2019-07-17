// The MIT License (MIT)
// Copyright (c) 2019 Ha Thach for Adafruit Industries

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

// the setup function runs once when you press reset or power the board
void setup()
{
  Serial.begin(115200);
  while ( !Serial ) delay(10);   // wait for native usb

  flash.begin();
  
  Serial.println("Adafruit Serial Flash Sector Dump example");
  Serial.print("JEDEC ID: "); Serial.println(flash.getJEDECID(), HEX);
  Serial.print("Flash size: "); Serial.println(flash.size());
}

void dump_sector(uint32_t sector)
{
  uint8_t buf[512];
  flash.readBuffer(sector*512, buf, 512);

  for(uint32_t row=0; row<32; row++)
  {
    if ( row == 0 ) Serial.print("0");
    if ( row < 16 ) Serial.print("0");
    Serial.print(row*16, HEX);
    Serial.print(" : ");

    for(uint32_t col=0; col<16; col++)
    {
      uint8_t val = buf[row*16 + col];

      if ( val < 16 ) Serial.print("0");
      Serial.print(val, HEX);

      Serial.print(" ");
    }

    Serial.println();
  }
}

void loop()
{
  Serial.print("Enter the sector number to dump: ");
  while( !Serial.available() ) delay(10);

  int sector = Serial.parseInt();

  Serial.println(sector); // echo

  if ( sector < flash.size()/512 )
  {
    dump_sector(sector);
  }else
  {
    Serial.println("Invalid sector number");
  }

  Serial.println();
  delay(10); // a bit of delay
}
