/**
 * @file Adafruit_QSPI_Flash.h
 *
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

#include "Adafruit_FlashTransport.h"
#include "Adafruit_FlashCache.h"
#include "flash_devices.h"

// implement SdFat Block Driver
#include "SdFat.h"
#include "SdFatConfig.h"

#if ENABLE_EXTENDED_TRANSFER_CLASS == 0
  #error ENABLE_EXTENDED_TRANSFER_CLASS must be set to 1 in SdFat SdFatConfig.h
#endif

#if FAT12_SUPPORT == 0
  #error FAT12_SUPPORT must be set to 1 in SdFat SdFatConfig.h
#endif

enum
{
  SFLASH_CMD_READ              = 0x03, // Single Read
  SFLASH_CMD_QUAD_READ         = 0x6B, // 1 line address, 4 line data

  SFLASH_CMD_READ_JEDEC_ID     = 0x9f,

  SFLASH_CMD_PAGE_PROGRAM      = 0x02,
  SFLASH_CMD_QUAD_PAGE_PROGRAM = 0x32, // 1 line address, 4 line data

  SFLASH_CMD_READ_STATUS       = 0x05,
  SFLASH_CMD_READ_STATUS2      = 0x35,

  SFLASH_CMD_WRITE_STATUS      = 0x01,
  SFLASH_CMD_WRITE_STATUS2     = 0x31,

  SFLASH_CMD_ENABLE_RESET      = 0x66,
  SFLASH_CMD_RESET             = 0x99,

  SFLASH_CMD_WRITE_ENABLE      = 0x06,
  SFLASH_CMD_WRITE_DISABLE     = 0x04,

  SFLASH_CMD_ERASE_SECTOR      = 0x20,
  SFLASH_CMD_ERASE_BLOCK       = 0xD8,
  SFLASH_CMD_ERASE_CHIP        = 0xC7,
};

/// Constant that is (mostly) true to all external flash devices
enum {
  SFLASH_BLOCK_SIZE  = 64*1024,
  SFLASH_SECTOR_SIZE = 4*1024,
  SFLASH_PAGE_SIZE   = 256,
};

class Adafruit_SPIFlash : public BaseBlockDriver
{
public:
	Adafruit_SPIFlash(Adafruit_FlashTransport* transport);
	~Adafruit_SPIFlash() {}

	bool begin(void);
	bool end(void);

  uint16_t numPages(void);
  uint16_t pageSize(void);

  uint32_t size(void);

	uint8_t readStatus(void);
	uint8_t readStatus2(void);
	void waitUntilReady(void);
	bool writeEnable(void);

	uint32_t getJEDECID (void);
	
	uint32_t readBuffer  (uint32_t address, uint8_t *buffer, uint32_t len);
	uint32_t writeBuffer (uint32_t address, uint8_t const *buffer, uint32_t len);

	bool eraseSector(uint32_t sectorNumber);
	bool eraseBlock (uint32_t blockNumber);
	bool eraseChip  (void);

	// Helper
	uint8_t  read8(uint32_t addr);
	uint16_t read16(uint32_t addr);
	uint32_t read32(uint32_t addr);

	//------------- SdFat BaseBlockDRiver API -------------//
	virtual bool readBlock(uint32_t block, uint8_t* dst);
	virtual bool syncBlocks();
	virtual bool writeBlock(uint32_t block, const uint8_t* src);
	virtual bool readBlocks(uint32_t block, uint8_t* dst, size_t nb);
	virtual bool writeBlocks(uint32_t block, const uint8_t* src, size_t nb);

private:
	Adafruit_FlashTransport* _trans;
	external_flash_device const * _flash_dev;

	Adafruit_FlashCache _cache;


};

#endif /* ADAFRUIT_SPIFLASH_H_ */
