#include "Adafruit_SPIFlash.h"

#if SPIFLASH_DEBUG
#define SPIFLASH_LOG(_block, _count)                                           \
  do {                                                                         \
    Serial.print(__FUNCTION__);                                                \
    Serial.print(": lba = ");                                                  \
    Serial.print(_block);                                                      \
    if (_count) {                                                              \
      Serial.print(" count = ");                                               \
      Serial.print(_count);                                                    \
    }                                                                          \
    Serial.println();                                                          \
  } while (0)
#else
#define SPIFLASH_LOG(_sector, _count)
#endif

Adafruit_SPIFlash::Adafruit_SPIFlash() : _cache(), Adafruit_SPIFlashBase() {}

Adafruit_SPIFlash::Adafruit_SPIFlash(Adafruit_FlashTransport *transport)
    : _cache(), Adafruit_SPIFlashBase(transport) {}

//--------------------------------------------------------------------+
// SdFat BaseBlockDRiver API
// A block is 512 bytes
//--------------------------------------------------------------------+
bool Adafruit_SPIFlash::readBlock(uint32_t block, uint8_t *dst) {
  SPIFLASH_LOG(block, 1);
  return _cache.read(this, block * 512, dst, 512);
}

bool Adafruit_SPIFlash::syncBlocks() {
  SPIFLASH_LOG(0, 0);
  return _cache.sync(this);
}

bool Adafruit_SPIFlash::writeBlock(uint32_t block, const uint8_t *src) {
  SPIFLASH_LOG(block, 1);
  return _cache.write(this, block * 512, src, 512);
}

bool Adafruit_SPIFlash::readBlocks(uint32_t block, uint8_t *dst, size_t nb) {
  SPIFLASH_LOG(block, nb);
  return _cache.read(this, block * 512, dst, 512 * nb);
}

bool Adafruit_SPIFlash::writeBlocks(uint32_t block, const uint8_t *src,
                                    size_t nb) {
  SPIFLASH_LOG(block, nb);
  return _cache.write(this, block * 512, src, 512 * nb);
}
