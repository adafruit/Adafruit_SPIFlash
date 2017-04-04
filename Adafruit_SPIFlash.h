// SPIFlash library by adafruit
// MIT license
#ifndef ADAFRUIT_SPIFLASH_H
#define ADAFRUIT_SPIFLASH_H

#if ARDUINO >= 100
 #include "Arduino.h"
#else
 #include "WProgram.h"
#endif

#include <SPI.h>

#if defined(__ARM__) || defined(ARDUINO_ARCH_SAMD)
  #define REGTYPE uint32_t
#else
  #define REGTYPE uint8_t
#endif

#define  SPIFLASH_SPI_STATREAD      0x02
#define  SPIFLASH_SPI_DATAWRITE     0x01
#define  SPIFLASH_SPI_DATAREAD      0x03
#define  SPIFLASH_SPI_READY         0x01

// Flash status bits
#define  SPIFLASH_STAT_BUSY         0x01   // Erase/Write in Progress
#define  SPIFLASH_STAT_WRTEN        0x02   // Write Enable Latch

// SPI Flash Characteristics (W25Q16BV Specific)
#define W25Q16BV_MAXADDRESS         0x1FFFFF
#define W25Q16BV_PAGESIZE           256    // 256 bytes per programmable page
#define W25Q16BV_PAGES              8192   // 2,097,152 Bytes / 256 bytes per page
#define W25Q16BV_SECTORSIZE         4096   // 1 erase sector = 4096 bytes
#define W25Q16BV_SECTORS            512    // 2,097,152 Bytes / 4096 bytes per sector
#define W25Q16BV_MANUFACTURERID     0xEF   // Used to validate read data
#define W25Q16BV_DEVICEID           0x14   // Used to validate read data

// Erase/Program Instructions
#define W25Q16BV_CMD_WRITEENABLE    0x06   // Write Enabled
#define W25Q16BV_CMD_WRITEDISABLE   0x04   // Write Disabled
#define W25Q16BV_CMD_READSTAT1      0x05   // Read Status Register 1
#define W25Q16BV_CMD_READSTAT2      0x35   // Read Status Register 2
#define W25Q16BV_CMD_WRITESTAT      0x01   // Write Status Register
#define W25Q16BV_CMD_PAGEPROG       0x02   // Page Program
#define W25Q16BV_CMD_QUADPAGEPROG   0x32   // Quad Page Program
#define W25Q16BV_CMD_SECTERASE4     0x20   // Sector Erase (4KB)
#define W25Q16BV_CMD_BLOCKERASE32   0x52   // Block Erase (32KB)
#define W25Q16BV_CMD_BLOCKERASE64   0xD8   // Block Erase (64KB)
#define W25_CMD_CHIPERASE      0x60   // Chip Erase
#define W25Q16BV_CMD_ERASESUSPEND   0x75   // Erase Suspend
#define W25Q16BV_CMD_ERASERESUME    0x7A   // Erase Resume
#define W25Q16BV_CMD_POWERDOWN      0xB9   // Power Down
#define W25Q16BV_CMD_CRMR           0xFF   // Continuous Read Mode Reset
// Read Instructions
#define W25Q16BV_CMD_FREAD          0x0B   // Fast Read
#define W25Q16BV_CMD_FREADDUALOUT   0x3B   // Fast Read Dual Output
#define W25Q16BV_CMD_FREADDUALIO    0xBB   // Fast Read Dual I/O
#define W25Q16BV_CMD_FREADQUADOUT   0x6B   // Fast Read Quad Output
#define W25Q16BV_CMD_FREADQUADIO    0xEB   // Fast Read Quad I/O
#define W25Q16BV_CMD_WREADQUADIO    0xE7   // Word Read Quad I/O
#define W25Q16BV_CMD_OWREADQUADIO   0xE3   // Octal Word Read Quad I/O
// ID/Security Instructions
#define W25Q16BV_CMD_RPWRDDEVID     0xAB   // Release Power Down/Device ID
#define W25Q16BV_CMD_MANUFDEVID     0x90   // Manufacturer/Device ID
#define W25Q16BV_CMD_MANUFDEVID2    0x92   // Manufacturer/Device ID by Dual I/O
#define W25Q16BV_CMD_MANUFDEVID4    0x94   // Manufacturer/Device ID by Quad I/O
#define W25Q16BV_CMD_JEDECID        0x9F   // JEDEC ID
#define W25Q16BV_CMD_READUNIQUEID   0x4B   // Read Unique ID


typedef enum
{
  SPIFLASHTYPE_W25Q16BV,
  SPIFLASHTYPE_25C02,
  SPIFLASHTYPE_W25X40CL,
} spiflash_type_t;

class Adafruit_SPIFlash  : public Print {
 public:
  Adafruit_SPIFlash(int8_t clk, int8_t miso, int8_t mosi, int8_t ss);
  Adafruit_SPIFlash(int8_t ss, SPIClass *spiinterface=&SPI);

  boolean begin(spiflash_type_t t);

  // Help functions to display formatted text
  void PrintHex(const byte * data, const uint32_t numBytes);
  void PrintHexChar(const byte * pbtData, const uint32_t numBytes);
  // Flash Functions
  void GetUniqueID(uint8_t *buffer);
  void GetManufacturerInfo (uint8_t *manufID, uint8_t *deviceID);
  uint32_t GetJEDECID (void);

  void WriteEnable (bool enable);
  uint32_t readBuffer (uint32_t address, uint8_t *buffer, uint32_t len);
  uint32_t EraseSector (uint32_t sectorNumber);
  uint32_t EraseChip (void);
  uint32_t WritePage (uint32_t address, uint8_t *buffer, uint32_t len);
  uint32_t writeBuffer (uint32_t address, uint8_t *buffer, uint32_t len);
  uint32_t findFirstEmptyAddr(void);
  void seek(uint32_t);
  size_t write(uint8_t b);
  boolean appendData(void);
  uint32_t getAddr();
  uint8_t readstatus();

  uint16_t numPages() {return pages; }
  uint16_t pageSize() {return pagesize;}

 private:
  spiflash_type_t type;
  int32_t pagesize;
  int32_t pages;
  int32_t totalsize;
  uint8_t addrsize;

  SPIClass *_spi;

  int8_t _ss, _clk, _mosi, _miso;
  volatile REGTYPE *clkportreg, *misoportreg, *mosiportreg;
  uint32_t clkpin, misopin, mosipin;

  uint32_t currentAddr;

  void readspidata(uint8_t* buff, uint8_t n);
  void spiwrite(uint8_t c);
  uint8_t spiread(void);
  uint32_t WaitForReady();
};

#endif
