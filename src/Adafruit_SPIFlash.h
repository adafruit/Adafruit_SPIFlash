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

#ifndef ADAFRUIT_SPIFLASH_H_
#define ADAFRUIT_SPIFLASH_H_

#include "Adafruit_FlashCache.h"
#include "Adafruit_SPIFlashBase.h"

// implement SdFat Block Driver
#include "SdFat_Adafruit_Fork.h"

#if SD_FAT_VERSION >= 20000

#if USE_BLOCK_DEVICE_INTERFACE == 0
#error USE_BLOCK_DEVICE_INTERFACE must be defined to 1 in SdFatConfig.h. Make sure you use the Adafruit Fork at 'https://github.com/adafruit/SdFat'
#endif

#else

#if ENABLE_EXTENDED_TRANSFER_CLASS == 0
#error ENABLE_EXTENDED_TRANSFER_CLASS must be set to 1 in SdFatConfig.h. Make sure you use the Adafruit Fork at 'https://github.com/adafruit/SdFat'
#endif

// Try our best to be forward-compatible with v2
#define FsBlockDeviceInterface BaseBlockDriver
#define FatVolume FatFileSystem
#define File32 File

#endif // SD_FAT_VERSION

#if FAT12_SUPPORT == 0
#error FAT12_SUPPORT must be set to 1 in SdFat SdFatConfig.h. Make sure you use the Adafruit Fork at 'https://github.com/adafruit/SdFat'
#endif

/// This class extends Adafruit_SPIFlashBase by adding support for the
/// BaseBlockDriver interface. This allows it to be used with SdFat's
/// FatFileSystem class.
///
/// Instances of this class will use 4kB of RAM as a block cache.
class Adafruit_SPIFlash : public FsBlockDeviceInterface,
                          public Adafruit_SPIFlashBase {
public:
  /// Create a flash block device with no transport
  Adafruit_SPIFlash();

  /// Create a flash block device on a transport
  /// @param transport bus used to talk to the flash
  /// @param useCache  use a 4KB sector cache for writes (not used for FRAM)
  Adafruit_SPIFlash(Adafruit_FlashTransport *transport, bool useCache = true);
  ~Adafruit_SPIFlash() {}

  /// Initialize the flash, then allocate the sector cache if enabled
  /// @param flash_devs list of possible flash devices, NULL for the built-in
  ///                   list (ignored on ESP32 and RP2040)
  /// @param count      number of entries in flash_devs
  /// @return true if the flash device was identified
  bool begin(SPIFlash_Device_t const *flash_devs = NULL, size_t count = 1);

  /// Deinitialize the flash and free the sector cache
  void end(void);

  /// Check whether writes go through the sector cache
  /// @return true if the cache is enabled and allocated
  bool isCached(void) { return _cache_en && (_cache != NULL); }

  //------------- SdFat v2 FsBlockDeviceInterface API -------------//
  /// Check whether a write or erase is in progress
  /// @return true if busy
  virtual bool isBusy();

  /// Get the number of 512 byte sectors
  /// @return sector count
  virtual uint32_t sectorCount();

  /// Write any cached data to flash
  /// @return true if success
  virtual bool syncDevice();

  /// Read a 512 byte sector
  /// @param block sector number
  /// @param dst   buffer to hold the data
  /// @return true if success
  virtual bool readSector(uint32_t block, uint8_t *dst);

  /// Read consecutive 512 byte sectors
  /// @param block first sector number
  /// @param dst   buffer to hold the data
  /// @param ns    number of sectors
  /// @return true if success
  virtual bool readSectors(uint32_t block, uint8_t *dst, size_t ns);

  /// Write a 512 byte sector
  /// @param block sector number
  /// @param src   data to write
  /// @return true if success
  virtual bool writeSector(uint32_t block, const uint8_t *src);

  /// Write consecutive 512 byte sectors
  /// @param block first sector number
  /// @param src   data to write
  /// @param ns    number of sectors
  /// @return true if success
  virtual bool writeSectors(uint32_t block, const uint8_t *src, size_t ns);

  // SdFat v1 BaseBlockDRiver API for backward-compatible
  /// Write any cached data to flash (SdFat v1 name for syncDevice())
  /// @return true if success
  virtual bool syncBlocks() { return syncDevice(); }

  /// Read a 512 byte block (SdFat v1 name for readSector())
  /// @param block block number
  /// @param dst   buffer to hold the data
  /// @return true if success
  virtual bool readBlock(uint32_t block, uint8_t *dst) {
    return readSector(block, dst);
  }

  /// Read consecutive 512 byte blocks (SdFat v1 name for readSectors())
  /// @param block first block number
  /// @param dst   buffer to hold the data
  /// @param nb    number of blocks
  /// @return true if success
  virtual bool readBlocks(uint32_t block, uint8_t *dst, size_t nb) {
    return readSectors(block, dst, nb);
  }

  /// Write a 512 byte block (SdFat v1 name for writeSector())
  /// @param block block number
  /// @param src   data to write
  /// @return true if success
  virtual bool writeBlock(uint32_t block, const uint8_t *src) {
    return writeSector(block, src);
  }

  /// Write consecutive 512 byte blocks (SdFat v1 name for writeSectors())
  /// @param block first block number
  /// @param src   data to write
  /// @param nb    number of blocks
  /// @return true if success
  virtual bool writeBlocks(uint32_t block, const uint8_t *src, size_t nb) {
    return writeSectors(block, src, nb);
  }

protected:
  bool _cache_en;              ///< Sector cache requested
  Adafruit_FlashCache *_cache; ///< Sector cache, NULL until begin()
};

#endif /* ADAFRUIT_SPIFLASH_H_ */
