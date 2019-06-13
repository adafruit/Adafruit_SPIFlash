// The MIT License (MIT)
// Copyright (c) 2019 Ha Thach for Adafruit Industries

#include "Adafruit_SPIFlash.h"

int chipSelect = 10;

Adafruit_Flash_Transport_SPI spiTransport(SS1, &SPI1);

Adafruit_SPIFlash flash(&spiTransport);

// the setup function runs once when you press reset or power the board
void setup()
{
  Serial.begin(115200);
  while ( !Serial ) delay(10);   // wait for native usb

  flash.begin();
  
  Serial.println("Adafruit Serial Flash Info example");
  Serial.print("JEDEC ID: "); Serial.println(flash.getJEDECID(), HEX);
  Serial.print("Flash size: "); Serial.println(flash.size());
}

void loop()
{
  // nothing to do
}
