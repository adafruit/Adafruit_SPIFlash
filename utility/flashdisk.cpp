#include "Adafruit_SPIFlash_FatFs.h"

#include "utility/diskio.h"
#include "utility/ff.h"


// Basic partitioning scheme for when fdisk and mkfs are used to format the
// flash.  This just creates one partition on the flash drive, see more
// details in FatFs docs:
//   http://elm-chan.org/fsw/ff/en/fdisk.html
PARTITION VolToPart[] = {
  {0, 0},    /* "0:" ==> Physical drive 0, 1st partition */
  // {1, 0},     // Logical drive 1 ==> Physical drive 1 (auto detection)
  // {2, 0},     // Logical drive 2 ==> Physical drive 2 (auto detection)
  // {3, 0},     // Logical drive 3 ==> Physical drive 3 (auto detection)
  // /*
  // {0, 2},     // Logical drive 2 ==> Physical drive 0, 2nd partition
  // {0, 3},     // Logical drive 3 ==> Physical drive 0, 3rd partition
  // */
};

// Global reference to activated flash FatFs object.  This object will receive
// all the low level FatFs callbacks for block reads, writes, etc.
static Adafruit_SPIFlash_FatFs* flashFatFs = NULL;

// Activate a flash FatFs object so it receives all the low level FatFs
// callbacks.
void flashdisk_activate(Adafruit_SPIFlash_FatFs* _flashFatFs) {
  flashFatFs = _flashFatFs;
}

// Below are the minimum FatFs library callback implemntations.  These are just
// glue functions that pass through to the activated FatFs object where
// appropriate (checking that one is activated first).
DSTATUS disk_status(BYTE pdrv) {
  if (flashFatFs == NULL) {
    return STA_NOINIT;
  }
  else {
    return flashFatFs->diskStatus();
  }
}

DSTATUS disk_initialize(BYTE pdrv) {
  if (flashFatFs == NULL) {
    return STA_NOINIT;
  }
  else {
    return flashFatFs->diskInitialize();
  }
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count) {
  if (flashFatFs == NULL) {
    return RES_NOTRDY;
  }
  else {
    return flashFatFs->diskRead(buff, sector, count);
  }
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count) {
  if (flashFatFs == NULL) {
    return RES_NOTRDY;
  }
  else {
    return flashFatFs->diskWrite(buff, sector, count);
  }
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
  if (flashFatFs == NULL) {
    return RES_NOTRDY;
  }
  else {
    return flashFatFs->diskIoctl(cmd, buff);
  }
}
