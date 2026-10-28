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

#ifndef ADAFRUIT_FLASHCACHE_H_
#define ADAFRUIT_FLASHCACHE_H_

#include <stdbool.h>
#include <stdint.h>

// forward declaration
class Adafruit_SPIFlashBase;

/// Single 4KB sector cache used to turn small block writes into whole sector
/// erase + program cycles on the flash
class Adafruit_FlashCache {
private:
  uint8_t _buf[4096] __attribute__((aligned(4))); // must be sector size
  uint32_t _addr;

public:
  /// Create an empty cache
  Adafruit_FlashCache(void);

  /// Erase and program the cached sector to flash, if any is cached
  /// @param fl flash device the cache belongs to
  /// @return true (always succeeds)
  bool sync(Adafruit_SPIFlashBase *fl);

  /// Write data through the cache, flushing the cached sector to flash
  /// whenever the write moves on to a different sector
  /// @param fl  flash device the cache belongs to
  /// @param dst flash address to write to
  /// @param src data to write
  /// @param len number of bytes to write
  /// @return true (always succeeds)
  bool write(Adafruit_SPIFlashBase *fl, uint32_t dst, void const *src,
             uint32_t len);

  /// Read data, taking any bytes held in the cache in preference to flash
  /// @param fl    flash device the cache belongs to
  /// @param addr  flash address to read from
  /// @param dst   buffer to hold the data
  /// @param count number of bytes to read
  /// @return true (always succeeds)
  bool read(Adafruit_SPIFlashBase *fl, uint32_t addr, uint8_t *dst,
            uint32_t count);
};

#endif /* ADAFRUIT_FLASHCACHE_H_ */
