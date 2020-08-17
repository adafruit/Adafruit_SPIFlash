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

#include "Adafruit_FlashTransport.h"
#include "Arduino.h"

Adafruit_FlashTransport_SPI::Adafruit_FlashTransport_SPI(
    uint8_t ss, SPIClass *spiinterface) {
  _cmd_read = SFLASH_CMD_READ;
  _ss = ss;
  _spi = spiinterface;
  _setting = SPISettings();
}

Adafruit_FlashTransport_SPI::Adafruit_FlashTransport_SPI(uint8_t ss,
                                                         SPIClass &spiinterface)
    : Adafruit_FlashTransport_SPI(ss, &spiinterface) {}

void Adafruit_FlashTransport_SPI::begin(void) {
  pinMode(_ss, OUTPUT);
  digitalWrite(_ss, HIGH);

  _spi->begin();
}

void Adafruit_FlashTransport_SPI::setClockSpeed(uint32_t clock_hz) {
  _setting = SPISettings(clock_hz, MSBFIRST, SPI_MODE0);
}

bool Adafruit_FlashTransport_SPI::runCommand(uint8_t command) {
  digitalWrite(_ss, LOW);
  _spi->beginTransaction(_setting);

  _spi->transfer(command);

  _spi->endTransaction();
  digitalWrite(_ss, HIGH);

  return true;
}

bool Adafruit_FlashTransport_SPI::readCommand(uint8_t command,
                                              uint8_t *response, uint32_t len) {
  digitalWrite(_ss, LOW);
  _spi->beginTransaction(_setting);

  _spi->transfer(command);
  while (len--) {
    *response++ = _spi->transfer(0xFF);
  }

  _spi->endTransaction();
  digitalWrite(_ss, HIGH);

  return true;
}

bool Adafruit_FlashTransport_SPI::writeCommand(uint8_t command,
                                               uint8_t const *data,
                                               uint32_t len) {
  digitalWrite(_ss, LOW);
  _spi->beginTransaction(_setting);

  _spi->transfer(command);
  while (len--) {
    (void)_spi->transfer(*data++);
  }

  _spi->endTransaction();
  digitalWrite(_ss, HIGH);

  return true;
}

bool Adafruit_FlashTransport_SPI::eraseCommand(uint8_t command,
                                               uint32_t address) {
  digitalWrite(_ss, LOW);
  _spi->beginTransaction(_setting);

  uint8_t cmd_with_addr[] = { command, (address >> 16) & 0xFF, (address >> 8) & 0xFF, address & 0xFF };

  _spi->transfer(cmd_with_addr, 4);

  _spi->endTransaction();
  digitalWrite(_ss, HIGH);

  return true;
}

bool Adafruit_FlashTransport_SPI::readMemory(uint32_t addr, uint8_t *data,
                                             uint32_t len) {
  digitalWrite(_ss, LOW);
  _spi->beginTransaction(_setting);

  uint8_t cmd_with_addr[5] = { _cmd_read, (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF, 0xFF };

  // Fast Read has 1 extra dummy byte
  _spi->transfer(cmd_with_addr, SFLASH_CMD_FAST_READ == _cmd_read ? 5 : 4);

  // Use SPI DMA if available for best performance
#if defined(ARDUINO_NRF52_ADAFRUIT) && defined(NRF52840_XXAA)
  _spi->transfer(NULL, data, len);
#elif defined(ARDUINO_ARCH_SAMD) && defined(_ADAFRUIT_ZERODMA_H_)
  _spi->transfer(NULL, data, len, true);
#else
  while (len--) {
    *data++ = _spi->transfer(0xFF);
  }
#endif

  _spi->endTransaction();
  digitalWrite(_ss, HIGH);

  return true;
}

bool Adafruit_FlashTransport_SPI::writeMemory(uint32_t addr,
                                              uint8_t const *data,
                                              uint32_t len) {
  digitalWrite(_ss, LOW);
  _spi->beginTransaction(_setting);

  uint8_t cmd_with_addr[] = { SFLASH_CMD_PAGE_PROGRAM, (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF };

  _spi->transfer(cmd_with_addr, 4);

  // Use SPI DMA if available for best performance
#if defined(ARDUINO_NRF52_ADAFRUIT) && defined(NRF52840_XXAA)
  _spi->transfer(data, NULL, len);
#elif defined(ARDUINO_ARCH_SAMD) && defined(_ADAFRUIT_ZERODMA_H_) && !defined(ADAFRUIT_FEATHER_M0_EXPRESS)
  // TODO Out of all M0 boards, Feather M0 seems to have issue with SPI DMA writing, other M0 board seems to be fine
  _spi->transfer(data, NULL, len, true);
#else
  while (len--) {
    _spi->transfer(*data++);
  }
#endif

  _spi->endTransaction();
  digitalWrite(_ss, HIGH);

  return true;
}
