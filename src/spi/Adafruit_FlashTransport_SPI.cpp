/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 hathach for Adafruit Industries
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */


#include "Adafruit_SPIFlash.h"

Adafruit_FlashTransport_SPI::Adafruit_FlashTransport_SPI(uint8_t ss, SPIClass *spiinterface)
{
  _ss = ss;
  _spi = spiinterface;
  _setting = SPISettings();
}

void Adafruit_FlashTransport_SPI::begin(void)
{
  pinMode(_ss, OUTPUT);
  digitalWrite(_ss, HIGH);

  _spi->begin();
}

void Adafruit_FlashTransport_SPI::setClockSpeed(uint32_t clock_hz)
{
  _setting = SPISettings(clock_hz, MSBFIRST, SPI_MODE0);
}

bool Adafruit_FlashTransport_SPI::runCommand(uint8_t command)
{
  digitalWrite(_ss, LOW);
  _spi->beginTransaction(_setting);

  _spi->transfer(command);

  _spi->endTransaction();
  digitalWrite(_ss, HIGH);

  return true;
}

bool Adafruit_FlashTransport_SPI::readCommand(uint8_t command, uint8_t* response, uint32_t len)
{
  digitalWrite(_ss, LOW);
  _spi->beginTransaction(_setting);

  _spi->transfer(command);
  while(len--)
  {
    *response++ = _spi->transfer(0xFF);
  }

  _spi->endTransaction();
  digitalWrite(_ss, HIGH);

  return true;
}

bool Adafruit_FlashTransport_SPI::writeCommand(uint8_t command, uint8_t const* data, uint32_t len)
{
  digitalWrite(_ss, LOW);
  _spi->beginTransaction(_setting);

  _spi->transfer(command);
  while(len--)
  {
    (void) _spi->transfer(*data++);
  }

  _spi->endTransaction();
  digitalWrite(_ss, HIGH);

  return true;
}

bool Adafruit_FlashTransport_SPI::eraseCommand(uint8_t command, uint32_t address)
{
  digitalWrite(_ss, LOW);
  _spi->beginTransaction(_setting);

  _spi->transfer(command);

  // 24-bit address MSB first
  _spi->transfer((address >> 16) & 0xFF);
  _spi->transfer((address >> 8) & 0xFF);
  _spi->transfer(address & 0xFF);

  _spi->endTransaction();
  digitalWrite(_ss, HIGH);

  return true;
}

bool Adafruit_FlashTransport_SPI::readMemory(uint32_t addr, uint8_t *data, uint32_t len)
{
  digitalWrite(_ss, LOW);
  _spi->beginTransaction(_setting);

  _spi->transfer(SFLASH_CMD_READ);

  // 24-bit address MSB first
  _spi->transfer((addr >> 16) & 0xFF);
  _spi->transfer((addr >> 8) & 0xFF);
  _spi->transfer(addr & 0xFF);

  while(len--)
  {
    *data++ = _spi->transfer(0xFF);
  }

  _spi->endTransaction();
  digitalWrite(_ss, HIGH);

  return true;
}

bool Adafruit_FlashTransport_SPI::writeMemory(uint32_t addr, uint8_t const *data, uint32_t len)
{
  digitalWrite(_ss, LOW);
  _spi->beginTransaction(_setting);

  _spi->transfer(SFLASH_CMD_PAGE_PROGRAM);

  // 24-bit address MSB first
  _spi->transfer((addr >> 16) & 0xFF);
  _spi->transfer((addr >> 8) & 0xFF);
  _spi->transfer(addr & 0xFF);

  while(len--)
  {
    _spi->transfer(*data++);
  }

  _spi->endTransaction();
  digitalWrite(_ss, HIGH);

  return true;
}
