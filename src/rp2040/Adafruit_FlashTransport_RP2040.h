/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 hathach for Adafruit Industries
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

#ifndef ADAFRUIT_FLASHTRANSPORT_RP2040_H_
#define ADAFRUIT_FLASHTRANSPORT_RP2040_H_

#include "Arduino.h"
#include "SPI.h"
#include "flash_devices.h"

/// Flash transport for a region of the RP2040's own flash, accessed through
/// the Pico SDK flash API
class Adafruit_FlashTransport_RP2040 : public Adafruit_FlashTransport {
protected:
  uint32_t _start_addr;         ///< Region offset from the start of flash
  uint32_t _size;               ///< Region size in bytes
  SPIFlash_Device_t _flash_dev; ///< Flash device describing the region

  /// Check whether an access ending at a region-relative offset stays within
  /// the region; the caller's addr + len is not checked for wrap
  /// @param addr exclusive end offset of the access
  /// @return true if valid
  bool check_addr(uint32_t addr) { return addr <= _size; }

  /// Flag (set via constructor) indicates whether other core must pause when
  /// writing or erasing flash. Default state is true, pause other core. VERY
  /// rare that this needs changed, REQUIRES SPECIAL LINKER CONFIG to locate
  /// ALL functions of other core entirely in RAM, else hard crash and flash
  /// filesystem corruption. PicoDVI is a rare use case.
  bool _idle_other_core_on_write;

public:
  /// Start of the CircuitPython partition: 1 MB, 1.5 MB on Pico W
  static const uint32_t CPY_START_ADDR;
  /// Size of the CircuitPython partition, from CPY_START_ADDR to the end of
  /// flash
  static const uint32_t CPY_SIZE;

  /// Generic constructor with address and size. If start_address and size are
  /// 0, value that matches filesystem setting in 'Tools->Flash Size' menu
  /// selection will be used.
  ///
  /// To be compatible with CircuitPython partition scheme (start_address =
  /// CPY_START_ADDR, size = CPY_SIZE) use
  ///   Adafruit_FlashTransport_RP2040(CPY_START_ADDR, CPY_SIZE)
  /// @param start_addr region offset from the start of flash, 0 for the menu
  ///                   filesystem start
  /// @param size       region size in bytes, 0 for the menu filesystem size
  /// @param idle       pause the other core while writing or erasing flash
  Adafruit_FlashTransport_RP2040(uint32_t start_addr = 0, uint32_t size = 0,
                                 bool idle = true);

  virtual void begin(void);
  virtual void end(void);

  virtual bool supportQuadMode(void) { return false; }

  virtual void setClockSpeed(uint32_t write_hz, uint32_t read_hz);

  virtual bool runCommand(uint8_t command);
  virtual bool readCommand(uint8_t command, uint8_t *response, uint32_t len);
  virtual bool writeCommand(uint8_t command, uint8_t const *data, uint32_t len);
  virtual bool eraseCommand(uint8_t command, uint32_t addr);

  virtual bool readMemory(uint32_t addr, uint8_t *data, uint32_t len);
  virtual bool writeMemory(uint32_t addr, uint8_t const *data, uint32_t len);

  /// Get the flash device already detected and configured, so begin() can
  /// skip the detection sequence
  /// @return device with the region size and the JEDEC ID read at begin()
  SPIFlash_Device_t *getFlashDevice(void);
};

/// RP2040 flash transport for the CircuitPython partition (CPY_START_ADDR,
/// CPY_SIZE)
class Adafruit_FlashTransport_RP2040_CPY
    : public Adafruit_FlashTransport_RP2040 {

public:
  /// Create a transport for the CircuitPython partition
  /// @param idle pause the other core while writing or erasing flash
  Adafruit_FlashTransport_RP2040_CPY(bool idle = true)
      : Adafruit_FlashTransport_RP2040(CPY_START_ADDR, CPY_SIZE, idle) {}
};

#endif
