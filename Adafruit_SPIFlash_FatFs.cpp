#include "Adafruit_SPIFlash_FatFs.h"

// Add addition to the specified base path and store the new path in the
// specified dest buffer (which must be MAX_PATH+1 size!)  Will insert the
// specified path separator between base and addition. Returns true if
// successful and false if base + addition + separator exceed MAX_PATH.
// Note this will NOT add a null terminator and it is assumed dest has been
// set to all zeros.
static bool concatPath(const char* base, const char* addition, char* dest, char separator='/') {
  // Check there is enough room to construct this path.
  int baseLength = strlen(base);
  int additionLength = strlen(addition);
  if ((MAX_PATH-baseLength+1) < additionLength) {
    // Not enough room to construct the path, fail.
    return false;
  }
  // Copy in base, separator, addition.
  memcpy(dest, base, baseLength);
  dest[baseLength] = '/';
  memcpy(dest+baseLength+1, addition, additionLength);
  return true;
}

// Build a fake MBR partition.  This is taken from the CircuitPython code
// and needs to match it exactly to interop with files between both Arduino
// and CircuitPython.
static void build_partition(uint8_t *buf, int boot, int type, uint32_t
                            start_block, uint32_t num_blocks) {
    buf[0] = boot;

    if (num_blocks == 0) {
        buf[1] = 0;
        buf[2] = 0;
        buf[3] = 0;
    } else {
        buf[1] = 0xff;
        buf[2] = 0xff;
        buf[3] = 0xff;
    }

    buf[4] = type;

    if (num_blocks == 0) {
        buf[5] = 0;
        buf[6] = 0;
        buf[7] = 0;
    } else {
        buf[5] = 0xff;
        buf[6] = 0xff;
        buf[7] = 0xff;
    }

    buf[8] = start_block;
    buf[9] = start_block >> 8;
    buf[10] = start_block >> 16;
    buf[11] = start_block >> 24;

    buf[12] = num_blocks;
    buf[13] = num_blocks >> 8;
    buf[14] = num_blocks >> 16;
    buf[15] = num_blocks >> 24;
}

File::File(const char* filepath, uint8_t mode, Adafruit_SPIFlash_FatFs* fatfs):
  _opened(false), _file({0}), _fileInfo({0}), _directory({0}), _dirPath(NULL),
  _fatfs(fatfs)
{
  activate();
  // Check if the file exists and get information about it.
  if (f_stat(filepath, &_fileInfo) != FR_OK) {
    // File doesn't exist, but should it? (i.e. we're not writing).
    if ((mode & FA_READ) == 0) {
      DEBUG_PRINTLN("File/directory doesn't exist!");
      return;
    }
  }
  // Force the directory bit on if this is the root.
  // This works around a weird quirk or maybe even bug in FatFs where the
  // root doesn't have the directory bit set.  Perhaps this is by design
  // with FAT filesystems, but regardless forcing the dir bit on will make
  // root paths look like directories as expected.
  if ((strcmp(filepath, "") == 0) || (strcmp(filepath, "/") == 0)) {
    _fileInfo.fattrib |= AM_DIR;
  }
  // Handle directory case (assumes we are only ever opening dirs to read,
  // to write/create use the explicit mkdir function).
  if ((_fileInfo.fattrib & AM_DIR) > 0) {
    // Save the directory path so paths to its children can be constructed
    // later in the openNextFile function.
    int pathLength = strlen(filepath);
    _dirPath = (char*)malloc(pathLength+1);
    if (_dirPath == NULL) {
      // Couldn't allocate memory for the directory path.
      DEBUG_PRINTLN("Failed to allocate memory for directory path!");
      return;
    }
    memcpy(_dirPath, filepath, pathLength);
    _dirPath[pathLength] = 0;
    // Open the directory for later openNextFile calls.
    if (f_opendir(&_directory, filepath) != FR_OK) {
      DEBUG_PRINTLN("Failed to open directory!");
      return;
    }
  }
  else {
    // Else not a directory so open the file and return it.
    FRESULT fr = f_open(&_file, filepath, mode);
    if (fr != FR_OK) {
      DEBUG_PRINT("Error opening file: "); DEBUG_PRINTLN(filepath);
      DEBUG_PRINT("Error code: "); DEBUG_PRINTLN(fr, DEC);
      return;
    }
  }
  _opened = true;
}

size_t File::write(const uint8_t *buf, size_t size) {
  activate();
  UINT bw = 0;
  if (f_write(&_file, buf, size, &bw) != FR_OK) {
    DEBUG_PRINTLN("f_write failed!");
    return 0;
  }
  return bw;
}

int File::read() {
  activate();
  UINT br = 0;
  uint8_t buff[1] = {0};
  if (f_read(&_file, buff, 1, &br) != FR_OK) {
    DEBUG_PRINTLN("f_read failed!");
    return -1;
  }
  if (br != 1) {
    DEBUG_PRINTLN("Failed to read byte from file!");
    return -1;
  }
  return buff[0];
}

int File::peek() {
  activate();
  int c = read();
  if (c != -1) {
    f_lseek(&_file, f_tell(&_file) - 1);
  }
  return c;
}

int File::available() {
  uint32_t n = size() - position();
  return n > 0X7FFF ? 0X7FFF : n;
}

void File::flush() {
  activate();
  f_sync(&_file);
}

int File::read(void *buf, uint16_t nbyte) {
  activate();
  UINT br = 0;
  if (f_read(&_file, buf, nbyte, &br) != FR_OK) {
    DEBUG_PRINTLN("f_read failed!");
    return 0;
  }
  return br;
}

bool File::seek(uint32_t pos) {
  activate();
  return f_lseek(&_file, pos) == FR_OK;
}

uint32_t File::position() {
  activate();
  return f_tell(&_file);
}

uint32_t File::size() {
  activate();
  return f_size(&_file);
}

void File::close() {
  activate();
  if (!isDirectory()) {
    f_close(&_file);
  }
  else {
    f_closedir(&_directory);
  }
  _opened = false;
}

File File::openNextFile(uint8_t mode) {
  activate();
  // Check this is a directory and it's open.
  if (!_opened || !isDirectory()) {
    return File();
  }
  // Call f_readdir to read the next file from the directory and return it.
  FILINFO info = {0};
  if (f_readdir(&_directory, &info) != FR_OK) {
    DEBUG_PRINTLN("Error calling f_readdir!");
    return File();
  }
  // Check if we've reached the end of enumeration.
  if (strlen(info.fname) == 0) {
    return File();
  }
  // Construct the path to this file using the directory's path and adding
  // a separateorand the file name.
  char path[MAX_PATH+1] = {0};
  if (!concatPath(_dirPath, info.fname, path)) {
    DEBUG_PRINTLN("Exceeded MAX_PATH trying to create file path!");
    return File();
  }
  return File(path, mode, _fatfs);
}

void File::rewindDirectory() {
  activate();
  // Use f_readdir with a NULL fno to reset directory enumeration.
  if (f_readdir(&_directory, NULL) != FR_OK) {
    DEBUG_PRINTLN("f_readdir failed to rewind directory enumeration!");
  }
}

void File::activate() {
  if (_fatfs != NULL) {
    _fatfs->activate();
  }
}

bool Adafruit_SPIFlash_FatFs::begin() {
  activate();
  // Mount the filesystem.
  FRESULT r = f_mount(&_fatFs, "", 1);
  if (r != FR_OK) {
    DEBUG_PRINT("f_mount failed with error code: "); DEBUG_PRINTLN(r, DEC);
    return false;
  }
  return true;
}

File Adafruit_SPIFlash_FatFs::open(const char *filename, uint8_t mode) {
  activate();
  return File(filename,  mode, this);
}

bool Adafruit_SPIFlash_FatFs::exists(const char *filepath) {
  activate();
  return f_stat(filepath, NULL) == FR_OK;
}

bool Adafruit_SPIFlash_FatFs::mkdir(const char *filepath) {
  activate();
  // Check the path to create can be fit inside a buffer.
  if (strlen(filepath) > MAX_PATH) {
    DEBUG_PRINTLN("Can't make a directory deeper than the max path!");
    return false;
  }
  // Copy the path to a buffer because it needs to be modified.
  char buffer[MAX_PATH+1] = {0};
  // Walk through each character of the path from the start and copy it
  // charactr by character into the buffer.  When a path separator is found
  // (a / or \ slash) before the character is copied the current path is checked
  // to see if it exists and created if not.
  for (int i=0; i<strlen(filepath); ++i) {
    // Check if the current character is a path separator.
    if ((filepath[i] == '\\') || (filepath[i] == '/')) {
      // Found a separator, check if the current path buffer has data and test
      // if a directory exists for it.
      if ((strlen(buffer) > 0) && (f_stat(buffer, NULL) != FR_OK)) {
        // Intermediate directory doesn't exist, try to create the directory.
        if (f_mkdir(buffer) != FR_OK) {
          DEBUG_PRINT("Failed to create intermediate directory: ");
          DEBUG_PRINTLN(buffer);
          return false;
        }
      }
    }
    buffer[i] = filepath[i];
  }
  // At the end and have created all the parent directories, now create the
  // very bottom directory.
  return f_mkdir(buffer) == FR_OK;
}

bool Adafruit_SPIFlash_FatFs::remove(const char *filepath) {
  activate();
  return f_unlink(filepath) == FR_OK;
}

bool Adafruit_SPIFlash_FatFs::rmdir(const char *filepath) {
  activate();
  // Check the specified path is a directory and can be opened.
  File root = open(filepath);
  if (!root || !root.isDirectory()) {
    DEBUG_PRINTLN("rmdir was not given a directory!");
    return false;
  }
  // Walk through all the children, deleting if it's a file and traversing
  // down to delete if a subdirectory (depth first search).
  File child = root.openNextFile();
  while (child) {
    // Construct full path to the child by concatenating a path separateor
    // and the file name.
    char path[MAX_PATH+1] = {0};
    if (!concatPath(filepath, child.name(), path)) {
      DEBUG_PRINTLN("Exceeded MAX_PATH trying to create file path!");
      return false;
    }
    if (!child.isDirectory()) {
      // Not a directory, just delete it.
      if (!remove(path)) {
        DEBUG_PRINT("Failed to delete file: "); DEBUG_PRINTLN(path);
        return false;
      }
    }
    else {
      // Is a directory, call rmdir on its path to traverse through and clear
      // it out too.
      if (!rmdir(path)) {
        DEBUG_PRINT("Failed to delete directory: "); DEBUG_PRINTLN(path);
        return false;
      }
    }
    child = root.openNextFile();
  }
  // Finally with all the children gone delete the root itself.
  return f_unlink(filepath) == FR_OK;
}

DSTATUS Adafruit_SPIFlash_FatFs::diskStatus() {
  return 0;  // Normal status, 0 (no bits set).
}

DSTATUS Adafruit_SPIFlash_FatFs::diskInitialize() {
  return 0;  // Normal status, 0 (no bits set).
}

DRESULT Adafruit_SPIFlash_FatFs::diskRead(BYTE *buff, DWORD sector,
                                          UINT count) {
  // Convert fat sector number to flash address, then read the specified
  // amount of sectors into the provided buffer.
  uint32_t address = _fatSectorAddress(sector);
  if (_flash.readBuffer(address, buff, count*_fatSectorSize) <= 0) {
   DEBUG_PRINTLN("readBuffer failed!");
   return RES_ERROR;
  }
  return RES_OK;
}

DRESULT Adafruit_SPIFlash_FatFs::diskWrite(const BYTE *buff, DWORD sector,
                                           UINT count) {
  if (_flashSectorBuffer == NULL) {
    DEBUG_PRINTLN("Flash sector buffer was not allocated!");
    return RES_ERROR;
  }
  // Loop through each of the specified fat sectors and update them.
  // This loop tries to be smart about minimizing flash sector writes by
  // combining multiple contiguous fat sector writes into one erase/update
  // cycle of the loop.
  for (int i=0; i < count; ) {
    // Determine starting flash address of this fat sector.
    uint32_t address = _fatSectorAddress(sector+i);
    uint32_t sectorStart = _flashSectorBase(address);

    // Check how many fat sectors can be written in this flash sector.
    int available = ((sectorStart + _flashSectorSize) - address)/_fatSectorSize;

    // Determine the number of fat sectors to write to fill the flash sector
    // based on the number that are left to write.
    int countToWrite = min(count-i, available);

    // Read the entire associated sector into memory.
    if (_flash.readBuffer(sectorStart, _flashSectorBuffer, _flashSectorSize) !=
        _flashSectorSize) {
      // Error, couldn't read sector into before before performing update.
      DEBUG_PRINTLN("Couldn't read sector before performing write!")
      return RES_ERROR;
    }

    // Modify appropriate part of sector with updated block data.
    uint16_t blockOffset = _flashSectorOffset(address);
    memcpy(_flashSectorBuffer+blockOffset, buff+(i*_fatSectorSize),
           countToWrite*_fatSectorSize);

    // Erase the sector.
    // Find the sector number by taking the sector base address and dividing by
    // the size of each sector (so this returns a value like sector 0, 1, 2,
    // etc. instead of an absolute address in memory).
    if (!_flash.EraseSector(sectorStart/_flashSectorSize)) {
      // Error, couldn't erase the sector.
      DEBUG_PRINTLN("Couldn't erase sector before performing write!")
      return RES_ERROR;
    }

    // Write the entire sector back out.
    if (_flash.writeBuffer(sectorStart, _flashSectorBuffer, _flashSectorSize) !=
        _flashSectorSize) {
      // Error, couldn't write the updated sector.
      DEBUG_PRINTLN("Couldn't write updated sector!")
      return RES_ERROR;
    }

    // Increment based on the amount of fat sectors that were written.
    i += countToWrite;
  }

  return RES_OK;
}

DRESULT Adafruit_SPIFlash_FatFs::diskIoctl(BYTE cmd, void* buff) {
  // Handle the minimum required ioctl commands.  See:
  //   http://elm-chan.org/fsw/ff/en/dioctl.html
  switch(cmd) {
    case CTRL_SYNC:
      // Nothing to do, no buffers to sync to flash.
      break;
    case GET_SECTOR_COUNT:
      {
        // Set the number of fat sectors available.
        DWORD* count = (DWORD*)buff;
        *count = _fatSectorCount();
        break;
      }
    case GET_SECTOR_SIZE:
      {
        // Set the fat sector size.
        WORD* count = (WORD*)buff;
        *count = _fatSectorSize;
        break;
      }
    case GET_BLOCK_SIZE:
      {
        // Return the number of fat sectors per flash sector.
        // Used to align data for efficient erasing.
        DWORD* count = (DWORD*)buff;
        *count = _flashSectorSize/_fatSectorSize;
        break;
      }
    case CTRL_TRIM:
      // Trim not supported right now, ignore it.
      break;
  }
  return RES_OK;
}

DRESULT Adafruit_M0_Express_CircuitPython::diskRead(BYTE *buff, DWORD sector,
                                                    UINT count) {
  // Handle synthesizing block 0 (the MBR).
  if (sector == 0) {
    // Synthesize a MBR in the same way as Micro/CircuitPython, see the code
    // in spi_flash_read here:
    //  https://github.com/adafruit/circuitpython/blob/master/atmel-samd/spi_flash.c#L456-L472
    memset(buff, 0, 512);
    // First partition starts at block 1.
    build_partition(buff + 446, 0, 0x01 /* FAT12 */, 1, _fatSectorCount());
    build_partition(buff + 462, 0, 0, 0, 0);
    build_partition(buff + 478, 0, 0, 0, 0);
    build_partition(buff + 494, 0, 0, 0, 0);
    buff[510] = 0x55;
    buff[511] = 0xaa;
    // After reading the synthesized MBR go on to read other blocks if
    // requested.
    if (count > 1) {
      return Adafruit_W25Q16BV_FatFs::diskRead(buff, sector+1, count-1);
    }
    else {
      return RES_OK;
    }
  }
  else {
    // Read other blocks using base class logic (no synthetic MBR requested).
    return Adafruit_W25Q16BV_FatFs::diskRead(buff, sector, count);
  }
}

DRESULT Adafruit_M0_Express_CircuitPython::diskWrite(const BYTE *buff,
                                                     DWORD sector, UINT count)
{
  // Ignore writes to block 0 (synthesized MBR).
  if (sector == 0) {
    // Ignore write to block zero, but write any other sectors after it.
    if (count > 1) {
      return Adafruit_W25Q16BV_FatFs::diskWrite(buff, sector+1, count-1);
    }
    else {
      return RES_OK;
    }
  }
  else {
    return Adafruit_W25Q16BV_FatFs::diskWrite(buff, sector, count);
  }
}
