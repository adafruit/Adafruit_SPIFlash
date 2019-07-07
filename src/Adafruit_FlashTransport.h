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

#ifndef ADAFRUIT_FLASHTRANSPORT_H_
#define ADAFRUIT_FLASHTRANSPORT_H_

#include <stdint.h>
#include <stdbool.h>

class Adafruit_FlashTransport
{
  public:
    virtual void begin(void);

    virtual bool supportQuadMode(void);

    /// Set clock speed in hertz
    /// @param clock_hz clock speed in hertz
    virtual void setClockSpeed(uint32_t clock_hz) = 0;

    /// Execute a single byte command e.g Reset, Write Enable
    /// @param command command code
    /// @return true if success
    virtual bool runCommand(uint8_t command) = 0;

    /// Execute a command with response data e.g Read Status, Read JEDEC
    /// @param command    command code
    /// @param response   buffer to hold data
    /// @param len        number of bytes to read
    /// @return true if success
    virtual bool readCommand(uint8_t command, uint8_t* response, uint32_t len) = 0;

    /// Execute a command with data e.g Write Status,
    /// @param command    command code
    /// @param data       writing data
    /// @param len        number of bytes to read
    /// @return true if success
    virtual bool writeCommand(uint8_t command, uint8_t const* data, uint32_t len) = 0;

    /// Erase external flash by address
    /// @param command  can be sector erase (0x20) or block erase 0xD8
    /// @param address  adddress to be erased
    /// @return true if success
    virtual bool eraseCommand(uint8_t command, uint32_t address) = 0;

    /// Read data from external flash contents. Typically it is implemented by quad read command 0x6B
    /// @param addr       address to read
    /// @param buffer     buffer to hold data
    /// @param len        number of byte to read
    /// @return true if success
    virtual bool readMemory(uint32_t addr, uint8_t *buffer, uint32_t len) = 0;

    /// Write data to external flash contents, flash sector must be previously erased first.
    /// Typically it uses quad write command 0x32
    /// @param addr       address to read
    /// @param data       writing data
    /// @param len        number of byte to read
    /// @return true if success
    virtual bool writeMemory(uint32_t addr, uint8_t const *data, uint32_t len) = 0;
};

#include "spi/Adafruit_FlashTransport_SPI.h"
#include "qspi/Adafruit_FlashTransport_QSPI.h"

#endif /* ADAFRUIT_FLASHTRANSPORT_H_ */
