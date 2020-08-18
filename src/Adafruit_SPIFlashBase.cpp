#include "Adafruit_SPIFlashBase.h"
#include "pins_arduino.h"
#include "wiring_private.h"
#include <SPI.h>

#include "flash_devices.h"

#if SPIFLASH_DEBUG
  #define SPIFLASH_LOG(_address, _count)                                         \
    do {                                                                         \
      Serial.print(__FUNCTION__);                                                \
      Serial.print(": adddress = ");                                             \
      Serial.print(_address, HEX);                                               \
      if (_count) {                                                              \
        Serial.print(" count = ");                                               \
        Serial.print(_count);                                                    \
      }                                                                          \
      Serial.println();                                                          \
    } while (0)

#else
  #define SPIFLASH_LOG(_sector, _count)

#endif

/// List of all possible flash devices used by Adafruit boards
static const SPIFlash_Device_t possible_devices[] = {
    // Main devices used in current Adafruit products
    GD25Q16C,
    GD25Q64C,
    S25FL116K,
    S25FL216K,

    // Only a handful of production run
    W25Q16FW,
    W25Q64JV_IQ,

    // Fujitsu FRAM 128/256/512 KBs
    MB85RS1MT,
    MB85RS2MTA,
    MB85RS4MT,

    // Nordic PCA10056
    MX25R6435F,

    // Other common flash devices
    W25Q16JV_IQ,
};

/// Flash device list count
enum {
  EXTERNAL_FLASH_DEVICE_COUNT =
      sizeof(possible_devices) / sizeof(possible_devices[0])
};

Adafruit_SPIFlashBase::Adafruit_SPIFlashBase() {
  _trans = NULL;
  _flash_dev = NULL;
  _ind_pin = -1;
  _ind_active = true;
}

Adafruit_SPIFlashBase::Adafruit_SPIFlashBase(
    Adafruit_FlashTransport *transport) {
  _trans = transport;
  _flash_dev = NULL;
  _ind_pin = -1;
  _ind_active = true;
}

static SPIFlash_Device_t const *findDevice(SPIFlash_Device_t const *device_list,
                                           int count,
                                           uint8_t const (&jedec_ids)[3]) {
  for (uint8_t i = 0; i < count; i++) {
    const SPIFlash_Device_t *dev = &device_list[i];
    if (jedec_ids[0] == dev->manufacturer_id &&
        jedec_ids[1] == dev->memory_type && // comment to appease format check
        jedec_ids[2] == dev->capacity) {
      return dev;
    }
  }
  return NULL;
}

bool Adafruit_SPIFlashBase::begin(SPIFlash_Device_t const *flash_devs,
                                  size_t count) {
  if (_trans == NULL)
    return false;

  _trans->begin();

  //------------- flash detection -------------//
  uint8_t jedec_ids[3];
  _trans->readCommand(SFLASH_CMD_READ_JEDEC_ID, jedec_ids, 3);

  // Check for device in supplied list, if any.
  if (flash_devs != NULL) {
    _flash_dev = findDevice(flash_devs, count, jedec_ids);
  }

  // If not found, check for device in standard list.
  if (_flash_dev == NULL) {
    _flash_dev =
        findDevice(possible_devices, EXTERNAL_FLASH_DEVICE_COUNT, jedec_ids);
  }

  if (_flash_dev == NULL) {
    Serial.print("Unknown flash device 0x");
    Serial.println(jedec_ids[0] << 16 | jedec_ids[1] << 8 | jedec_ids[2], HEX);
    return false;
  }

  // We don't know what state the flash is in so wait for any remaining writes
  // and then reset. Skip this procedure for FRAM since it does not support Reset command
  if ( !_flash_dev->is_fram ) {

    // The write in progress bit should be low.
    while (readStatus() & 0x01) {
    }

    // The suspended write/erase bit should be low.
    if ( !_flash_dev->single_status_byte ) {
      while ( readStatus2() & 0x80 ) {
      }
    }

    _trans->runCommand(SFLASH_CMD_ENABLE_RESET);
    _trans->runCommand(SFLASH_CMD_RESET);

    // Wait 30us for the reset
    delayMicroseconds(30);
  }

  // Speed up to max device frequency, or as high as possible
  uint32_t clock_speed = _flash_dev->max_clock_speed_mhz * 1000000U;
  uint32_t max_speed = F_CPU;

  if (_flash_dev->is_fram) {
    // Initial testing on breadboard, max clock speed to work reliably with FRAM is slower than supported specs
    // - nRF52840: 16 Mhz  with DMA write/read ~ 2000 KB/s
    // - SAMD M4 : 24 Mhz, no   DMA write/read ~ 1300 KB/s
    // - SAMD M0 : 12 Mhz, no   DMA write/read ~  500 KB/s
#if defined ARDUINO_NRF52_ADAFRUIT
    max_speed = 16000000;
#elif defined ARDUINO_ARCH_SAMD
  #ifdef __SAMD51__
    max_speed = 24000000;
  #else
    max_speed = 12000000;
  #endif
#endif
  }

  clock_speed = min(clock_speed, max_speed);
  //PRINT_INT(clock_speed);
  _trans->setClockSpeed(clock_speed);

  // Enable Quad Mode if available
  if (_trans->supportQuadMode() && _flash_dev->supports_qspi) {
    // Verify that QSPI mode is enabled.
    uint8_t status =
        _flash_dev->single_status_byte ? readStatus() : readStatus2();

    // Check the quad enable bit.
    if ((status & _flash_dev->quad_enable_bit_mask) == 0) {
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
  }else {
    // Single mode, use fast read if supported
    if ( _flash_dev->supports_fast_read ) {
      _trans->setReadCommand(SFLASH_CMD_FAST_READ);
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

  writeDisable();
  waitUntilReady();

  return true;
}

void Adafruit_SPIFlashBase::setIndicator(int pin, bool state_on)
{
  _ind_pin = pin;
  _ind_active = state_on;
}

uint32_t Adafruit_SPIFlashBase::size(void) {
  return _flash_dev ? _flash_dev->total_size : 0;
}

uint16_t Adafruit_SPIFlashBase::numPages(void) {
  return _flash_dev ? _flash_dev->total_size / pageSize() : 0;
}

uint16_t Adafruit_SPIFlashBase::pageSize(void) { return SFLASH_PAGE_SIZE; }

uint32_t Adafruit_SPIFlashBase::getJEDECID(void) {
  if (!_flash_dev) {
    return 0xFFFFFF;
  }else {
    return (_flash_dev->manufacturer_id << 16) | (_flash_dev->memory_type << 8) |
        _flash_dev->capacity;
  }
}

uint8_t Adafruit_SPIFlashBase::readStatus() {
  uint8_t status;
  _trans->readCommand(SFLASH_CMD_READ_STATUS, &status, 1);
  return status;
}

uint8_t Adafruit_SPIFlashBase::readStatus2(void) {
  uint8_t status;
  _trans->readCommand(SFLASH_CMD_READ_STATUS2, &status, 1);
  return status;
}

void Adafruit_SPIFlashBase::waitUntilReady(void) {
  // FRAM has no need to wait for either read or write operation
  if (_flash_dev->is_fram) {
    return;
  }

  // both WIP and WREN bit should be clear
  while (readStatus() & 0x03) {
    yield();
  }
}

bool Adafruit_SPIFlashBase::writeEnable(void) {
  return _trans->runCommand(SFLASH_CMD_WRITE_ENABLE);
}

bool Adafruit_SPIFlashBase::writeDisable(void) {
  return _trans->runCommand(SFLASH_CMD_WRITE_DISABLE);
}

bool Adafruit_SPIFlashBase::eraseSector(uint32_t sectorNumber) {
  if (!_flash_dev) {
    return false;
  }

  // skip erase for FRAM
  if (_flash_dev->is_fram) {
    return true;
  }

  _indicator_on();

  // Before we erase the sector we need to wait for any writes to finish
  waitUntilReady();
  writeEnable();

  SPIFLASH_LOG(sectorNumber * SFLASH_SECTOR_SIZE, 0);

  bool const ret =  _trans->eraseCommand(SFLASH_CMD_ERASE_SECTOR,
                              sectorNumber * SFLASH_SECTOR_SIZE);

  _indicator_off();

  return ret;
}

bool Adafruit_SPIFlashBase::eraseBlock(uint32_t blockNumber) {
  if (!_flash_dev)
    return false;

  // skip erase for fram
  if (_flash_dev->is_fram) {
    return true;
  }

  _indicator_on();

  // Before we erase the sector we need to wait for any writes to finish
  waitUntilReady();
  writeEnable();

  bool const ret =  _trans->eraseCommand(SFLASH_CMD_ERASE_BLOCK, blockNumber * SFLASH_BLOCK_SIZE);

  _indicator_off();

  return ret;
}

bool Adafruit_SPIFlashBase::eraseChip(void) {
  if (!_flash_dev)
    return false;

  // skip erase for fram
  if (_flash_dev->is_fram) {
    return true;
  }

  _indicator_on();

  // We need to wait for any writes to finish
  waitUntilReady();
  writeEnable();

  bool const ret = _trans->runCommand(SFLASH_CMD_ERASE_CHIP);

  _indicator_off();

  return ret;
}

uint32_t Adafruit_SPIFlashBase::readBuffer(uint32_t address, uint8_t *buffer,
                                           uint32_t len) {
  if (!_flash_dev)
    return 0;

  _indicator_on();

  waitUntilReady();
  SPIFLASH_LOG(address, len);
  bool const rc = _trans->readMemory(address, buffer, len);

  _indicator_off();

  return rc ? len : 0;
}

uint8_t Adafruit_SPIFlashBase::read8(uint32_t addr) {
  uint8_t ret;
  return readBuffer(addr, &ret, sizeof(ret)) ? ret : 0xff;
}

uint16_t Adafruit_SPIFlashBase::read16(uint32_t addr) {
  uint16_t ret;
  return readBuffer(addr, (uint8_t *)&ret, sizeof(ret)) ? ret : 0xffff;
}

uint32_t Adafruit_SPIFlashBase::read32(uint32_t addr) {
  uint32_t ret;
  return readBuffer(addr, (uint8_t *)&ret, sizeof(ret)) ? ret : 0xffffffff;
}

uint32_t Adafruit_SPIFlashBase::writeBuffer(uint32_t address,
                                            uint8_t const *buffer,
                                            uint32_t len) {
  if (!_flash_dev)
    return 0;

  SPIFLASH_LOG(address, len);

  _indicator_on();

  // FRAM: the whole chip can be written in one pass without waiting.
  // Also we need to explicitly disable WREN
  if (_flash_dev->is_fram) {
    writeEnable();

    _trans->writeMemory(address, buffer, len);

    writeDisable();
  }else{
    uint32_t remain = len;

    // write one page (256 bytes) at a time and
    // must not go over page boundary
    while (remain) {
      waitUntilReady();
      writeEnable();

      uint32_t const leftOnPage = SFLASH_PAGE_SIZE - (address & (SFLASH_PAGE_SIZE - 1));
      uint32_t const toWrite = min(remain, leftOnPage);

      if (!_trans->writeMemory(address, buffer, toWrite))
        break;

      remain -= toWrite;
      buffer += toWrite;
      address += toWrite;
    }

    len -= remain;
  }

  _indicator_off();

  return len;
}
