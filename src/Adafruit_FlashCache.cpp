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

#include "Adafruit_SPIFlash.h"

#define INVALID_ADDR  0xffffffff

static inline uint32_t page_addr_of (uint32_t addr)
{
  return addr & ~(FLASH_CACHE_SIZE - 1);
}

static inline uint32_t page_offset_of (uint32_t addr)
{
  return addr & (FLASH_CACHE_SIZE - 1);
}

Adafruit_FlashCache::Adafruit_FlashCache(void)
{
  cache_addr = INVALID_ADDR;
}

bool Adafruit_FlashCache::sync(Adafruit_SPIFlash* fl)
{
  if ( cache_addr == INVALID_ADDR ) return true;

  fl->eraseSector(cache_addr/FLASH_CACHE_SIZE);
  fl->writeBuffer(cache_addr, cache_buf, FLASH_CACHE_SIZE);

  cache_addr = INVALID_ADDR;

  return true;
}

bool Adafruit_FlashCache::write(Adafruit_SPIFlash* fl, uint32_t addr, void const * src, uint32_t len)
{
  uint8_t const * src8 = (uint8_t const *) src;
  uint32_t remain = len;

  // Program up to sector boundary each loop
  while ( remain )
  {
    uint32_t const page_addr = page_addr_of(addr);
    uint32_t const offset = page_offset_of(addr);

    uint32_t wr_bytes = FLASH_CACHE_SIZE - offset;
    wr_bytes = min(remain, wr_bytes);

    // Page changes, flush old and update new cache
    if ( page_addr != cache_addr )
    {
      this->sync(fl);
      cache_addr = page_addr;

      // read a whole page from flash
      fl->readBuffer(page_addr, cache_buf, FLASH_CACHE_SIZE);
    }

    memcpy(cache_buf + offset, src8, wr_bytes);

    // adjust for next run
    src8 += wr_bytes;
    remain -= wr_bytes;
    addr += wr_bytes;
  }

  return true;
}

bool Adafruit_FlashCache::read(Adafruit_SPIFlash* fl, uint32_t addr, uint8_t* buffer, uint32_t count)
{
  // overwrite with cache value if available
  if ( (cache_addr != INVALID_ADDR) &&
       !(addr < cache_addr && addr + count <= cache_addr) &&
       !(addr >= cache_addr + FLASH_CACHE_SIZE) )
  {
    int32_t dst_off = cache_addr - addr;
    int32_t src_off = 0;

    if ( dst_off < 0 )
    {
      src_off = -dst_off;
      dst_off = 0;
    }

    int32_t cache_bytes = min((int32_t) (FLASH_CACHE_SIZE-src_off), (int32_t) (count - dst_off));

    // start to cached
    if ( dst_off ) fl->readBuffer(addr, buffer, dst_off);

    // cached
    memcpy(buffer + dst_off, cache_buf + src_off, cache_bytes);

    // cached to end
    int copied = dst_off + cache_bytes;
    if ( copied < count ) fl->readBuffer(addr + copied, buffer + copied, count - copied);
  }
  else
  {
    fl->readBuffer(addr, buffer, count);
  }

  return true;
}
