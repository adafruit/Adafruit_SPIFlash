/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach and Dean Miller for Adafruit Industries LLC
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

#ifndef ADAFRUIT_SPIFLASHBASE_H_
#define ADAFRUIT_SPIFLASHBASE_H_

#include "Adafruit_FlashTransport.h"
#include "flash_devices.h"

// for debugging
#define SPIFLASH_DEBUG 0

/// An easy to use interface for working with Flash memory.
///
/// If you are managing allocation of the Flash space yourself, this is the
/// class to use as it take very little RAM.
class Adafruit_SPIFlashBase {
public:
  /// Create a flash device with no transport
  Adafruit_SPIFlashBase();

  /// Create a flash device on a transport
  /// @param transport bus used to talk to the flash
  Adafruit_SPIFlashBase(Adafruit_FlashTransport *transport);
  ~Adafruit_SPIFlashBase() {}

  /// Initialize the transport and identify the flash chip
  /// @param flash_devs list of possible flash devices, NULL for the built-in
  ///                   list (ignored on ESP32 and RP2040)
  /// @param count      number of entries in flash_devs
  /// @return true if the flash device was identified
  bool begin(SPIFlash_Device_t const *flash_devs = NULL, size_t count = 1);

  /// Deinitialize the transport and forget the flash device
  void end(void);

  /// Set a pin that is driven while the flash is being accessed, e.g. an LED
  /// @param pin      pin number, -1 to disable
  /// @param state_on pin level when active
  void setIndicator(int pin, bool state_on = true);

  /// Get the number of pages in the flash
  /// @return number of pages, 0 if no flash device
  uint32_t numPages(void);

  /// Get the flash page size
  /// @return page size in bytes
  uint16_t pageSize(void);

  /// Get the flash size
  /// @return size in bytes, 0 if no flash device
  uint32_t size(void);

  /// Read the first status register
  /// @return status register 1 value
  uint8_t readStatus(void);

  /// Read the second status register
  /// @return status register 2 value
  uint8_t readStatus2(void);

  /// Block until no write or erase is in progress
  void waitUntilReady(void);

  /// Set the write enable latch
  /// @return true if the command was sent
  bool writeEnable(void);

  /// Clear the write enable latch
  /// @return true if the command was sent
  bool writeDisable(void);

  /// Check whether the flash is idle (both WIP and WREN are clear)
  /// @return true if ready
  bool isReady(void);

  /// Get the JEDEC ID of the flash
  /// @return manufacturer ID, memory type and capacity as 0xMMTTCC, or
  ///         0xFFFFFF if no flash device
  uint32_t getJEDECID(void);

  /// Read data from flash
  /// @param address flash address to read from
  /// @param buffer  buffer to hold the data
  /// @param len     number of bytes to read
  /// @return number of bytes read, 0 on failure
  uint32_t readBuffer(uint32_t address, uint8_t *buffer, uint32_t len);

  /// Write data to flash, which must already be erased
  /// @param address flash address to write to
  /// @param buffer  data to write
  /// @param len     number of bytes to write
  /// @return number of bytes written
  uint32_t writeBuffer(uint32_t address, uint8_t const *buffer, uint32_t len);

  /// Erase a 256 byte page
  /// @param pageNumber page to erase
  /// @return true if success
  bool erasePage(uint32_t pageNumber);

  /// Erase a 4KB sector
  /// @param sectorNumber sector to erase
  /// @return true if success
  bool eraseSector(uint32_t sectorNumber);

  /// Erase a 64KB block
  /// @param blockNumber block to erase
  /// @return true if success
  bool eraseBlock(uint32_t blockNumber);

  /// Erase the whole chip
  /// @return true if success
  bool eraseChip(void);

  // Helper
  /// Read one byte from flash
  /// @param addr flash address to read from
  /// @return byte read, 0xff on failure
  uint8_t read8(uint32_t addr);

  /// Read a 16-bit value from flash
  /// @param addr flash address to read from
  /// @return value read, 0xffff on failure
  uint16_t read16(uint32_t addr);

  /// Read a 32-bit value from flash
  /// @param addr flash address to read from
  /// @return value read, 0xffffffff on failure
  uint32_t read32(uint32_t addr);

protected:
  Adafruit_FlashTransport *_trans;     ///< Bus used to talk to the flash
  SPIFlash_Device_t const *_flash_dev; ///< Identified flash device, or NULL

  int _ind_pin;     ///< Activity indicator pin, -1 if none
  bool _ind_active; ///< Activity indicator pin level when active

  /// Drive the activity indicator pin to its active level, if set
  void _indicator_on(void) {
    if (_ind_pin >= 0) {
      digitalWrite(_ind_pin, _ind_active ? HIGH : LOW);
    }
  }

  /// Drive the activity indicator pin to its inactive level, if set
  void _indicator_off(void) {
    if (_ind_pin >= 0) {
      digitalWrite(_ind_pin, _ind_active ? LOW : HIGH);
    }
  }
};

#endif /* ADAFRUIT_SPIFLASHBASE_H_ */
