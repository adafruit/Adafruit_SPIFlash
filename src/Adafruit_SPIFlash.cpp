#include <SPI.h>
#include "Adafruit_SPIFlash.h"
#include "pins_arduino.h"
#include "wiring_private.h"

#include "flash_devices.h"


#define SPIFLASH_DEBUG 0

#if SPIFLASH_DEBUG
  #define SPIFLASH_LOG(_sector, _count)   do { \
        Serial.print(__FUNCTION__); Serial.print(": sector = "); Serial.print(_sector);\
        Serial.print(" count = "); Serial.println(_count); \
      } while (0)
#else
  #define SPIFLASH_LOG(_sector, _count)
#endif

/// List of all possible flash devices used by Adafruit boards
static const external_flash_device possible_devices[] =
{
  GD25Q16C, GD25Q64C,    // Main devices current Adafruit
  S25FL116K, S25FL216K,
  W25Q16FW, W25Q64JV_IQ, // Only a handful of production run

  MX25R6435F,            // Nordic PCA10056
};

/// Flash device list count
enum
{
  EXTERNAL_FLASH_DEVICE_COUNT = sizeof(possible_devices)/sizeof(possible_devices[0])
};


Adafruit_SPIFlash::Adafruit_SPIFlash(Adafruit_FlashTransport* transport)
  : _cache()
{
  _trans = transport;
  _flash_dev = NULL;
}

bool Adafruit_SPIFlash::begin(void)
{
  _trans->begin();

  //------------- flash detection -------------//
	uint8_t jedec_ids[3];
	_trans->readCommand(SFLASH_CMD_READ_JEDEC_ID, jedec_ids, 3);

	for (uint8_t i = 0; i < EXTERNAL_FLASH_DEVICE_COUNT; i++) {
	  const external_flash_device* possible_device = &possible_devices[i];
	  if (jedec_ids[0] == possible_device->manufacturer_id &&
	      jedec_ids[1] == possible_device->memory_type &&
	      jedec_ids[2] == possible_device->capacity) {
	    _flash_dev = possible_device;
	    break;
	  }
	}

	if (_flash_dev == NULL) return false;

  // We don't know what state the flash is in so wait for any remaining writes and then reset.

  // The write in progress bit should be low.
  while ( readStatus() & 0x01 ) {}

  // The suspended write/erase bit should be low.
  while ( readStatus2() & 0x80 ) {}

  _trans->runCommand(SFLASH_CMD_ENABLE_RESET);
  _trans->runCommand(SFLASH_CMD_RESET);

  // Wait 30us for the reset
  delayMicroseconds(30);

  // Speed up to max device frequency
  _trans->setClockSpeed(_flash_dev->max_clock_speed_mhz*1000000UL);

  // Enable Quad Mode if available
  if ( _trans->supportQuadMode() && (_flash_dev->quad_enable_bit_mask) )
  {
    // Verify that QSPI mode is enabled.
    uint8_t status = _flash_dev->single_status_byte ? readStatus() : readStatus2();

    // Check the quad enable bit.
    if ((status & _flash_dev->quad_enable_bit_mask) == 0)
    {
        writeEnable();

        uint8_t full_status[2] = {0x00, _flash_dev->quad_enable_bit_mask};

        if (_flash_dev->write_status_register_split) {
            _trans->writeCommand(SFLASH_CMD_WRITE_STATUS2, full_status + 1, 1);
        } else if (_flash_dev->single_status_byte) {
            _trans->writeCommand(SFLASH_CMD_WRITE_STATUS, full_status + 1, 1);
        } else {
            _trans->writeCommand(SFLASH_CMD_WRITE_STATUS, full_status, 2);
        }
    }
  }

  // Turn off sector protection if needed
//  if (_flash_dev->has_sector_protection)
//  {
//    writeEnable();
//
//    uint8_t data[1] = {0x00};
//    QSPI0.writeCommand(QSPI_CMD_WRITE_STATUS, data, 1);
//  }

  // Turn off writes in case this is a microcontroller only reset.
  _trans->runCommand(SFLASH_CMD_WRITE_DISABLE);

  waitUntilReady();

  return true;
}

uint32_t Adafruit_SPIFlash::size(void)
{
  return _flash_dev ? _flash_dev->total_size : 0;
}

uint16_t Adafruit_SPIFlash::numPages(void)
{
  return _flash_dev ? _flash_dev->total_size/pageSize() : 0;
}

uint16_t Adafruit_SPIFlash::pageSize(void)
{
  return SFLASH_PAGE_SIZE;
}
 
uint32_t Adafruit_SPIFlash::getJEDECID (void)
{
	return (_flash_dev->manufacturer_id << 16) | (_flash_dev->memory_type << 8) | _flash_dev->capacity;
}

uint8_t Adafruit_SPIFlash::readStatus()
{
  uint8_t status;
  _trans->readCommand(SFLASH_CMD_READ_STATUS, &status, 1);
  return status;
}

uint8_t Adafruit_SPIFlash::readStatus2(void)
{
  uint8_t status;
  _trans->readCommand(SFLASH_CMD_READ_STATUS2, &status, 1);
  return status;
}

void Adafruit_SPIFlash::waitUntilReady(void)
{
  // both WIP and WREN bit should be clear
  while ( readStatus() & 0x03 ) {}
}

bool Adafruit_SPIFlash::writeEnable(void)
{
  return _trans->runCommand(SFLASH_CMD_WRITE_ENABLE);
}

bool Adafruit_SPIFlash::eraseSector(uint32_t sectorNumber)
{
  if (!_flash_dev) return false;

  // Before we erase the sector we need to wait for any writes to finish
  waitUntilReady();
  writeEnable();

	return _trans->eraseCommand(SFLASH_CMD_ERASE_SECTOR, sectorNumber * SFLASH_SECTOR_SIZE);
}

bool Adafruit_SPIFlash::eraseBlock (uint32_t blockNumber)
{
  if (!_flash_dev) return false;

  // Before we erase the sector we need to wait for any writes to finish
  waitUntilReady();
  writeEnable();

  return _trans->eraseCommand(SFLASH_CMD_ERASE_BLOCK, blockNumber * SFLASH_BLOCK_SIZE);
}

bool Adafruit_SPIFlash::eraseChip  (void)
{
  if (!_flash_dev) return false;

  // We need to wait for any writes to finish
  waitUntilReady();
	writeEnable();

	return _trans->runCommand(SFLASH_CMD_ERASE_CHIP);
}

uint32_t Adafruit_SPIFlash::readBuffer  (uint32_t address, uint8_t *buffer, uint32_t len)
{
  if (!_flash_dev) return 0;
  waitUntilReady();

  SPIFLASH_LOG(address/512, len/512);

  return _trans->readMemory(address, buffer, len) ? len : 0;
}

uint8_t Adafruit_SPIFlash::read8(uint32_t addr)
{
	uint8_t ret;
	return readBuffer(addr, &ret, sizeof(ret)) ? ret : 0xff;
}

uint16_t Adafruit_SPIFlash::read16(uint32_t addr)
{
	uint16_t ret;
	return readBuffer(addr, (uint8_t*) &ret, sizeof(ret)) ? ret : 0xffff;
}

uint32_t Adafruit_SPIFlash::read32(uint32_t addr)
{
	uint32_t ret;
	return readBuffer(addr, (uint8_t*) &ret, sizeof(ret)) ? ret : 0xffffffff;
}

uint32_t Adafruit_SPIFlash::writeBuffer (uint32_t address, uint8_t const *buffer, uint32_t len)
{
  if (!_flash_dev) return 0;

  SPIFLASH_LOG(address/512, len/512);

  uint32_t remain = len;

	// write one page at a time
	while(remain)
	{
	  waitUntilReady();
	  writeEnable();

	  uint16_t const toWrite = min(remain, (uint32_t)SFLASH_PAGE_SIZE);

		if ( !_trans->writeMemory(address, buffer, toWrite) ) break;

		remain -= toWrite;
		buffer += toWrite;
		address += toWrite;
	}

	return len - remain;
}

//--------------------------------------------------------------------+
// SdFat BaseBlockDRiver API
// A block is 512 bytes
//--------------------------------------------------------------------+
bool Adafruit_SPIFlash::readBlock(uint32_t block, uint8_t* dst)
{
  SPIFLASH_LOG(block, 1);
  return _cache.read(this, block*512, dst, 512);
}

bool Adafruit_SPIFlash::syncBlocks()
{
  SPIFLASH_LOG(0, 0);
  return _cache.sync(this);
}

bool Adafruit_SPIFlash::writeBlock(uint32_t block, const uint8_t* src)
{
  SPIFLASH_LOG(block, 1);
  return _cache.write(this, block*512, src, 512);
}

bool Adafruit_SPIFlash::readBlocks(uint32_t block, uint8_t* dst, size_t nb)
{
  SPIFLASH_LOG(block, nb);
  return _cache.read(this, block*512, dst, 512*nb);
}

bool Adafruit_SPIFlash::writeBlocks(uint32_t block, const uint8_t* src, size_t nb)
{
  SPIFLASH_LOG(block, nb);
  return _cache.write(this, block*512, src, 512*nb);
}
