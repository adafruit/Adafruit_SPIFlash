#ifndef ADAFRUIT_SPIFLASH_FATFS_H
#define ADAFRUIT_SPIFLASH_FATFS_H

#include <Arduino.h>

#include "Adafruit_SPIFlash.h"

#include "utility/diskio.h"
#include "utility/ff.h"

// Forward reference for flashdisk_activate function in flashdisk.h include.
class Adafruit_SPIFlash_FatFs;

#include "utility/flashdisk.h"


#define DEBUG 0
#define DEBUG_PRINTER Serial

#ifdef DEBUG
  #define DEBUG_PRINT(...) { DEBUG_PRINTER.print(__VA_ARGS__); }
  #define DEBUG_PRINTLN(...) { DEBUG_PRINTER.println(__VA_ARGS__); }
#else
  #define DEBUG_PRINT(...) {}
  #define DEBUG_PRINTLN(...) {}
#endif


#define MAX_PATH 256 // Maximum length of a file's full path.  Used to create
                     // a buffer for constructing file paths.

// Simpler modes users can pass to file open functions.
// These just map to FatFs file modes.
#define FILE_READ FA_READ
#define FILE_WRITE (FA_READ | FA_WRITE | FA_OPEN_APPEND)


class File : public Stream {
public:
  File():
    _opened(false), _file({0}), _fileInfo({0}), _directory({0}), _dirPath(NULL),
    _fatfs(NULL)
  {}
  File(const char* filepath, uint8_t mode = FILE_READ,
       Adafruit_SPIFlash_FatFs* fatfs = NULL);
  virtual ~File() {
    // Free directory path member memory if it was set.
    if (_dirPath != NULL) {
      free(_dirPath);
    }
  }

  // This interface exactly mirrors the SD library File class:
  virtual size_t write(uint8_t val) {
    return write(&val, 1);
  }
  virtual size_t write(const uint8_t *buf, size_t size);
  virtual int read();
  virtual int peek();
  virtual int available();
  virtual void flush();
  int read(void *buf, uint16_t nbyte);
  bool seek(uint32_t pos);
  uint32_t position();
  uint32_t size();
  void close();
  operator bool() {
    return _opened;
  }
  char* name() {
    return _fileInfo.fname;
  }
  bool isDirectory() {
    return (_fileInfo.fattrib & AM_DIR) > 0;
  }
  File openNextFile(uint8_t mode = (FA_READ | FA_OPEN_EXISTING));
  void rewindDirectory();

  using Print::write;

private:
  bool _opened;                     // Is the file open?
  FIL _file;                        // FatFs file handle.
  FILINFO _fileInfo;                // FatFs file/dir info.
  DIR _directory;                   // FatFs directory handle.
  char* _dirPath;                   // Full path to directory.
                                    // Only set for directories as they
                                    // need to construct full paths to
                                    // their children in openNextFile.
  Adafruit_SPIFlash_FatFs* _fatfs;  // Reference to associated fatfs
                                    // for this file.

  // Helper to activate the associated fatfs for this file.
  void activate();
};

class Adafruit_SPIFlash_FatFs {
public:
  Adafruit_SPIFlash_FatFs(Adafruit_SPIFlash& flash, int flashSectorSize,
                          int fatSectorSize=512):
    _flash(flash), _fatSectorSize(fatSectorSize),
    _flashSectorSize(flashSectorSize)
  {
    // Allocate a buffer to hold a sector of flash data.  This is used during
    // writes because a sector is the minimum size of data to update.
    _flashSectorBuffer = (uint8_t*)malloc(_flashSectorSize);
  }
  virtual ~Adafruit_SPIFlash_FatFs() {
    // Reclaim the buffer memory when it's not used anymore.
    if (_flashSectorBuffer != NULL) {
      free(_flashSectorBuffer);
    }
  }

  // Functions that are similar to the Arduino SD library:
  bool begin();
  File open(const char *filename, uint8_t mode = FILE_READ);
  File open(const String &filename, uint8_t mode = FILE_READ) {
    return open( filename.c_str(), mode );
  }
  bool exists(const char *filepath);
  bool exists(const String &filepath) {
    return exists(filepath.c_str());
  }
  bool mkdir(const char *filepath);
  bool mkdir(const String &filepath) {
    return mkdir(filepath.c_str());
  }
  bool remove(const char *filepath);
  bool remove(const String &filepath) {
    return remove(filepath.c_str());
  }
  bool rmdir(const char *filepath);
  bool rmdir(const String &filepath) {
    return rmdir(filepath.c_str());
  }

  // Activate FatFs with this instance.
  void activate() {
    flashdisk_activate(this);
  }

  // Functions that will be called by FatFs to perform block reads, writes, etc.
  // See Media Access Interface documentation at:
  //  http://elm-chan.org/fsw/ff/00index_e.html
  // None of these need to take the disk number (pdrv) parameter as it is
  // handled by the global utility/flashdisk.cpp functions.
  virtual DSTATUS diskStatus();
  virtual DSTATUS diskInitialize();
  virtual DRESULT diskRead(BYTE *buff, DWORD sector, UINT count);
  virtual DRESULT diskWrite(const BYTE *buff, DWORD sector, UINT count);
  virtual DRESULT diskIoctl(BYTE cmd, void* buff);

protected:
  Adafruit_SPIFlash& _flash;   // Reference to low level flash library.
  FATFS _fatFs;                // FatFs main mount handle.
  const int _fatSectorSize;    // Size of a fat filesystem sector, should
                               // always be 512 as that's how the FatFs library
                               // is configured.
  const int _flashSectorSize;  // Size of a flash erase sector, usually 4k but
                               // will be set appropriately by subclasses.

  // Return the number of fat sectors/blocks available on the flash chip.
  virtual uint32_t _fatSectorCount() {
    return (_flash.pageSize()*_flash.numPages())/_fatSectorSize;
  }

  // Return the flash drive address of the specified fat sector.
  // Can be overridden if the default implementation below doesn't suffice.
  virtual uint32_t _fatSectorAddress(uint32_t sector) {
    return sector*_fatSectorSize;
  }

  // Return the flash sector base address for the specified flash address.
  // This is the start of the 4k sector block that can be erased.  Subclasses
  // must provide this implementation as it depends on the flash chip
  // address size, etc.
  virtual uint32_t _flashSectorBase(uint32_t address) = 0;
  virtual uint32_t _flashSectorOffset(uint32_t address) = 0;

private:
  uint8_t* _flashSectorBuffer;  // Buffer to store flash sector data during writes.
};

class Adafruit_W25Q16BV_FatFs: public Adafruit_SPIFlash_FatFs {
public:
  Adafruit_W25Q16BV_FatFs(Adafruit_SPIFlash& flash):
    Adafruit_SPIFlash_FatFs(flash, 4096)
  {}

protected:
  virtual uint32_t _flashSectorBase(uint32_t address) {
    // Upper 12-bits of address are the base (start) of the sector for this
    // address.
    return address & 0xFFF000;
  }

  virtual uint32_t _flashSectorOffset(uint32_t address) {
    // Lower 12-bits of address are the offset into a sector for the specified
    // address.
    return address & 0x000FFF;
  }
};

class Adafruit_M0_Express_CircuitPython: public Adafruit_W25Q16BV_FatFs {
public:
  Adafruit_M0_Express_CircuitPython(Adafruit_SPIFlash& flash):
    Adafruit_W25Q16BV_FatFs(flash)
  {}

  // Override block read and write functions to add synthesized MBR (block 0).
  virtual DRESULT diskRead(BYTE *buff, DWORD sector, UINT count);
  virtual DRESULT diskWrite(const BYTE *buff, DWORD sector, UINT count);

protected:

  virtual uint32_t _fatSectorCount() {
    // Remove a 4kb flash sector from the fat sector count because CircuitPython
    // reserves one flash sector for a cache.
    return (_flash.pageSize()*_flash.numPages()-4096)/_fatSectorSize;
  }

  virtual uint32_t _fatSectorAddress(uint32_t sector) {
    // Offset the sector by 1 as the last flash sector is actually an unused
    // cache for writes in CircuitPython.
    return (sector-1)*_fatSectorSize;
  }
};

#endif
