#include <SPI.h>
#include "Adafruit_SPIFlash.h"
#include "pins_arduino.h"
#include "wiring_private.h"

#define SPIFLASH_BUFFERSIZE   (W25Q16BV_PAGESIZE)
byte spiflash_buffer[SPIFLASH_BUFFERSIZE];

/* ************* */
/* CONSTRUCTORS  */
/* ************* */
Adafruit_SPIFlash::Adafruit_SPIFlash(int8_t clk, int8_t miso, int8_t mosi, int8_t ss) 
{
  _clk = clk;
  _miso = miso;
  _mosi = mosi;
  _ss = ss;
}


Adafruit_SPIFlash::Adafruit_SPIFlash(int8_t ss, SPIClass *spiinterface) 
{
  _clk = _miso = _mosi = -1;
  _ss = ss;

  _spi = spiinterface;  // default to built in SPI

  digitalWrite(_ss, HIGH);  
  pinMode(_ss, OUTPUT);
}


boolean Adafruit_SPIFlash::begin(spiflash_type_t t) {
  type = t;

  if (_clk != -1) {
    pinMode(_clk, OUTPUT);
    pinMode(_mosi, OUTPUT);
    pinMode(_miso, INPUT);

    digitalWrite(_clk, LOW);  
    digitalWrite(_mosi, HIGH);  

    clkportreg =  portOutputRegister(digitalPinToPort(_clk));
    clkpin = digitalPinToBitMask(_clk);
    misoportreg =  portInputRegister(digitalPinToPort(_miso));
    misopin = digitalPinToBitMask(_miso);
  } else {
    _spi->begin();
  }

  pinMode(_ss, OUTPUT);
  digitalWrite(_ss, HIGH);  


  currentAddr = 0;

  if (type == SPIFLASHTYPE_W25Q16BV) {
    pagesize = 256;
    addrsize = 24;
    pages = 8192;
    totalsize = pages * pagesize; // 2 MBytes
  } 
  else if (type == SPIFLASHTYPE_25C02) {
    pagesize = 32;
    addrsize = 16;
    pages = 8;
    totalsize = pages * pagesize; // 256 bytes
  } 
  else if (type == SPIFLASHTYPE_W25X40CL) {
    pagesize = 256;
    addrsize = 24;
    pages = 2048;
    totalsize =  pages * pagesize; // 512 Kbytes
  }
  else {
    pagesize = 0;
    return false;
  }
  return true;
}

/* *************** */
/* PRIVATE METHODS */
/* *************** */
 
/**************************************************************************/
/*! 
    @brief Gets the value of the Read Status Register (0x05)

    @return     The 8-bit value returned by the Read Status Register
*/
/**************************************************************************/
uint8_t Adafruit_SPIFlash::readstatus()
{
  uint8_t status;

  digitalWrite(_ss, LOW);
  spiwrite(W25Q16BV_CMD_READSTAT1);    // Send read status 1 cmd
  status = spiread();                  // Dummy write
  digitalWrite(_ss, HIGH);

  return status & (SPIFLASH_STAT_BUSY | SPIFLASH_STAT_WRTEN);
}

/************** low level SPI */

void Adafruit_SPIFlash::readspidata(uint8_t* buff, uint8_t n) 
{
  digitalWrite(_ss, LOW);
  delay(2); 
  spiwrite(0x00);  // ToDo: Send read command

  for (uint8_t i=0; i<n; i++) {
    delay(1);
    buff[i] = spiread();
  }

  digitalWrite(_ss, HIGH);
}

void Adafruit_SPIFlash::spiwrite(uint8_t c) 
{
  if (_clk == -1) {
    // hardware SPI
    _spi->beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    _spi->transfer(c);
    _spi->endTransaction();
  } else {
    // Software SPI

    int8_t i;
    // MSB first, clock low when inactive (CPOL 0), data valid on leading edge (CPHA 0) 
    // Make sure clock starts low
    // slow version - built in shiftOut function
    //shiftOut(_mosi, _clk, MSBFIRST, c); return;
    
    clkportreg =  portOutputRegister(digitalPinToPort(_clk));
    clkpin = digitalPinToBitMask(_clk);
    mosiportreg =  portOutputRegister(digitalPinToPort(_mosi));
    mosipin = digitalPinToBitMask(_mosi);
    
    for (i=7; i>=0; i--) {
      *clkportreg &= ~clkpin;
      if (c & (1<<i)) {
	*mosiportreg |= mosipin;
      } else {
      *mosiportreg &= ~mosipin;
      }    
      *clkportreg |= clkpin;
    }
    
    *clkportreg &= ~clkpin;
    // Make sure clock ends low
  }
}

uint8_t Adafruit_SPIFlash::spiread(void) 
{
  uint8_t x = 0;
  if (_clk == -1) {
    // hardware SPI
    _spi->beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    x = _spi->transfer(0xFF);
    _spi->endTransaction();

    return x;
  } else {
    // Software SPI
    return shiftIn(_miso, _clk, MSBFIRST);
    
    int8_t i;

    // MSB first, clock low when inactive (CPOL 0), data valid on leading edge (CPHA 0) 
    // Make sure clock starts low
    
    for (i=7; i>=0; i--)  {
      *clkportreg &= ~clkpin;
      
      asm("nop; nop");
      if ((*misoportreg) & misopin) {
	x <<= 1;
	x |= 1;
      }
      *clkportreg |= clkpin;
    }
    // Make sure clock ends low
    *clkportreg &= ~clkpin;
    
    return x;
  }
}

/**************************************************************************/
/*! 
    @brief  Waits for the SPI flash to indicate that it is ready (not
            busy) or until a timeout occurs.

    @return An error message indicating that a timeoout occured
            (SPIFLASH_ERROR_TIMEOUT_READY) or an OK signal to indicate that
            the SPI flash is ready (SPIFLASH_ERROR_OK)
*/
/**************************************************************************/
uint32_t Adafruit_SPIFlash::WaitForReady()
{
  uint32_t timeout = 0;
  uint8_t status;

  while ( timeout < 1000 )
  {
    status = readstatus() & SPIFLASH_STAT_BUSY;
    if (status == 0)
    {
      break;
    }
    timeout++;
  }
  if ( timeout == 1000 )
  {
    // In this case, 1 equals an error so we can say "if(results) ..." 
    return 1;
  }

  return 0;
}

/* ************** */
/* PUBLIC METHODS */
/* ************** */

/**************************************************************************/
/*! 
    @brief  Prints a hexadecimal value in plain characters

    @param  data      Pointer to the byte data
    @param  numBytes  Data length in bytes
*/
/**************************************************************************/
void Adafruit_SPIFlash::PrintHex(const byte * data, const uint32_t numBytes)
{
  uint32_t szPos;
  for (szPos=0; szPos < numBytes; szPos++) 
  {
    Serial.print("0x");
	// Append leading 0 for small values
	if (data[szPos] <= 0xF)
      Serial.print("0");
	Serial.print(data[szPos], HEX);
	if ((numBytes > 1) && (szPos != numBytes - 1))
	{
	  Serial.print(" ");
	}
  }
  Serial.println("");
}

/**************************************************************************/
/*! 
    @brief  Prints a hexadecimal value in plain characters, along with
            the char equivalents in the following format:

            00 00 00 00 00 00  ......

    @param  data      Pointer to the byte data
    @param  numBytes  Data length in bytes
*/
/**************************************************************************/
void Adafruit_SPIFlash::PrintHexChar(const byte * data, const uint32_t numBytes)
{
  uint32_t szPos;
  for (szPos=0; szPos < numBytes; szPos++) 
  {
	// Append leading 0 for small values
	if (data[szPos] <= 0xF)
      Serial.print("0");
	Serial.print(data[szPos], HEX);
	if ((numBytes > 1) && (szPos != numBytes - 1))
	{
	  Serial.print(" ");
	}
  }
  Serial.print("  ");
  for (szPos=0; szPos < numBytes; szPos++) 
  {
    if (data[szPos] <= 0x1F)
	  Serial.print(".");
	else
	  Serial.print(data[szPos]);
  }
  Serial.println("");
}

/**************************************************************************/
/*! 
    @brief  Gets the unique 64-bit ID assigned to this IC (useful for
            security purposes to detect if the flash was changed, etc.)

    @param[out] *buffer
                Pointer to the uint8_t buffer that will store the 8 byte
                long unique ID

    @note   The unique ID is return in bit order 63..0
*/
/**************************************************************************/
void Adafruit_SPIFlash::GetUniqueID(uint8_t *buffer)
{
  uint8_t i;

  digitalWrite(_ss, LOW);
  spiwrite(W25Q16BV_CMD_READUNIQUEID); // Unique ID cmd
  spiwrite(0xFF);                      // Dummy write
  spiwrite(0xFF);                      // Dummy write
  spiwrite(0xFF);                      // Dummy write
  spiwrite(0xFF);                      // Dummy write
  // Read 8 bytes worth of data
  for (i = 0; i < 8; i++)
  {
    buffer[i] = spiread();
  }
  digitalWrite(_ss, HIGH);
}

/**************************************************************************/
/*! 
    @brief Gets the 8-bit manufacturer ID and device ID for the flash

    @param[out] *manufID
                Pointer to the uint8_t that will store the manufacturer ID
    @param[out] *deviceID
                Pointer to the uint8_t that will store the device ID
*/
/**************************************************************************/
void Adafruit_SPIFlash::GetManufacturerInfo (uint8_t *manufID, uint8_t *deviceID)
{
  // W25Q16BV_CMD_MANUFDEVID (0x90) provides both the JEDEC manufacturer
  // ID and the device ID

  digitalWrite(_ss, LOW);
  spiwrite(W25Q16BV_CMD_MANUFDEVID); 
  spiwrite(0x00);            // Dummy write
  spiwrite(0x00);            // Dummy write
  spiwrite(0x00);            // Dummy write
  *manufID = spiread();
  *deviceID = spiread();
  digitalWrite(_ss, HIGH);
}

uint32_t Adafruit_SPIFlash::GetJEDECID (void)
{
  // W25Q16BV_CMD_MANUFDEVID (0x90) provides both the JEDEC manufacturer
  // ID and the device ID

  digitalWrite(_ss, LOW);
  spiwrite(W25Q16BV_CMD_JEDECID ); 
  spiwrite(0x00);            // Dummy write
  spiwrite(0x00);            // Dummy write
  spiwrite(0x00);            // Dummy write

  uint32_t id;
  id = spiread(); id <<= 8;
  id |= spiread(); id <<= 8;
  id |= spiread(); id <<= 8;
  id |= spiread();

  digitalWrite(_ss, HIGH);

  return id;
}


/**************************************************************************/
/*! 
    @brief Sets the write flag on the SPI flash, and if required puts the
           WP pin in an appropriate state

    @param[in]  enable
                True (1) to enable writing, false (0) to disable it
*/
/**************************************************************************/
void Adafruit_SPIFlash::WriteEnable (bool enable)
{
  // ToDo: Put the WP pin in an appropriate state if required

  digitalWrite(_ss, LOW);
  spiwrite(enable ? W25Q16BV_CMD_WRITEENABLE : W25Q16BV_CMD_WRITEDISABLE);
  digitalWrite(_ss, HIGH);
}

/**************************************************************************/
/*! 
    @brief Reads the specified number of bytes from the supplied address.

    This function will read one or more bytes starting at the supplied
    address. Please note that bufferLength is zero-based, meaning you
    should  supply '0' to read a single byte, '3' to read 4 bytes of data,
    etc.

    @param[in]  address
                The 24-bit address where the read will start.
    @param[out] *buffer
                Pointer to the buffer that will store the read results
    @param[in]  len
                Length of the buffer.
*/
/**************************************************************************/
uint32_t Adafruit_SPIFlash::readBuffer (uint32_t address, uint8_t *buffer, uint32_t len)
{
  uint32_t a, i;
  a = i = 0;

  // Make sure the address is valid
  if (address >= totalsize)
  {
    return 0;
  }

  // Wait until the device is ready or a timeout occurs
  if (WaitForReady())
    return 0;

  // Send the read data command
  digitalWrite(_ss, LOW);

  if (addrsize == 24) { // 24 bit addr
    spiwrite(SPIFLASH_SPI_DATAREAD);      // 0x03
    spiwrite((address >> 16) & 0xFF);     // address upper 8
    spiwrite((address >> 8) & 0xFF);      // address mid 8
    spiwrite(address & 0xFF);             // address lower 8
  }
  else { // (addrsize == 16) // 16 bit addr, assumed
    spiwrite(SPIFLASH_SPI_DATAREAD);      // 0x03
    spiwrite((address >> 8) & 0xFF);      // address high 8
    spiwrite(address & 0xFF);             // address lower 8
  }

  // Fill response buffer
  for (a = address; a < address + len; a++, i++)
  {
    if (a > totalsize)
    {
      // Oops ... we're at the end of the flash memory
	  // return bytes written up until now
      return i;
    }
    buffer[i] = spiread();
  }
  digitalWrite(_ss, HIGH);

  // Return bytes written
  return i;
}

/**************************************************************************/
/*! 
    @brief Erases the contents of a single sector

    @param[in]  sectorNumber
                The sector number to erase (zero-based).
*/
/**************************************************************************/
uint32_t Adafruit_SPIFlash::EraseSector (uint32_t sectorNumber)
{
  // Make sure the address is valid
  if (sectorNumber >= W25Q16BV_SECTORS)
  {
    return 0;
  }  

  // Wait until the device is ready or a timeout occurs
  if (WaitForReady())
    return 0;

  // Make sure the chip is write enabled
  WriteEnable (1);

  // Make sure the write enable latch is actually set
  uint8_t status;
  status = readstatus();
  if (!(status & SPIFLASH_STAT_WRTEN))
  {
    // Throw a write protection error (write enable latch not set)
    return 0;
  }

  // Send the erase sector command
  uint32_t address = sectorNumber * W25Q16BV_SECTORSIZE;
  digitalWrite(_ss, LOW);
  spiwrite(W25Q16BV_CMD_SECTERASE4); 
  spiwrite((address >> 16) & 0xFF);     // address upper 8
  spiwrite((address >> 8) & 0xFF);      // address mid 8
  spiwrite(address & 0xFF);             // address lower 8
  digitalWrite(_ss, HIGH);

  // Wait until the busy bit is cleared before exiting
  // This can take up to 400ms according to the datasheet
  while (readstatus() & SPIFLASH_STAT_BUSY);

  return 1;
}

/**************************************************************************/
/*! 
    @brief Erases the entire flash chip
*/
/**************************************************************************/
uint32_t Adafruit_SPIFlash::EraseChip (void)
{
  Serial.println("A");
  // Wait until the device is ready or a timeout occurs
  if (WaitForReady())
    return 0;

  Serial.println("B");
  Serial.print("Stat: "); Serial.println(readstatus(), HEX);
  // Make sure the chip is write enabled
  WriteEnable (1);

  Serial.println("C");

  // Make sure the write enable latch is actually set
  uint8_t status;
  status = readstatus();
  Serial.print("Stat: "); Serial.println(readstatus(), HEX);
  if (!(status & SPIFLASH_STAT_WRTEN))
  {
    // Throw a write protection error (write enable latch not set)
    return 0;
  }

  Serial.println("D");

  // Send the erase chip command
  digitalWrite(_ss, LOW);
  spiwrite(W25_CMD_CHIPERASE); 
  digitalWrite(_ss, HIGH);

  Serial.print("Stat: "); Serial.println(readstatus(), HEX);

  Serial.println("E");

  // Wait until the busy bit is cleared before exiting
  // This can take up to 10 seconds according to the datasheet!
  while (readstatus() & SPIFLASH_STAT_BUSY) {
    Serial.print("Stat: "); Serial.println(readstatus(), HEX);
  }

  return 1;
}

/**************************************************************************/
/*! 
    @brief      Writes up to 256 bytes of data to the specified page.
                
    @note       Before writing data to a page, make sure that the 4K sector
                containing the specific page has been erased, otherwise the
                data will be meaningless.

    @param[in]  address
                The 24-bit address where the write will start.
    @param[out] *buffer
                Pointer to the buffer that will store the read results
    @param[in]  len
                Length of the buffer.  Valid values are from 1 to 256,
                within the limits of the starting address and page length.
*/
/**************************************************************************/
uint32_t Adafruit_SPIFlash::WritePage (uint32_t address, uint8_t *buffer, uint32_t len)
{
  uint8_t status;
  uint32_t i;

  // Make sure the address is valid
  if (address >= W25Q16BV_MAXADDRESS)
  {
    return 0;
  }

  // Make sure that the supplied data is no larger than the page size
  if (len > pagesize)
  {
    return 0;
  }

  // Make sure that the data won't wrap around to the beginning of the sector
  if ((address % pagesize) + len > pagesize)
  {
    // If you try to write to a page beyond the last byte, it will
    // wrap around to the start of the page, almost certainly
    // messing up your data
    return 0;
  }

  // Wait until the device is ready or a timeout occurs
  if (WaitForReady())
    return 0;

  // Make sure the chip is write enabled
  WriteEnable (1);

  // Make sure the write enable latch is actually set
  status = readstatus();
  if (!(status & SPIFLASH_STAT_WRTEN))
  {
    // Throw a write protection error (write enable latch not set)
    return 0;
  }

  digitalWrite(_ss, LOW);

  if (addrsize == 24) {
    // Send page write command (0x02) plus 24-bit address
    spiwrite(W25Q16BV_CMD_PAGEPROG);      // 0x02
    spiwrite((address >> 16) & 0xFF);     // address upper 8
    spiwrite((address >> 8) & 0xFF);      // address mid 8
    if (len == pagesize)
      {
	// If len = 256 bytes, lower 8 bits must be 0 (see datasheet 11.2.17)
	spiwrite(0);
      }
    else
      {
	spiwrite(address & 0xFF);           // address lower 8
      }
  } else if (addrsize == 16) {
    // Send page write command (0x02) plus 16-bit address
    spiwrite(W25Q16BV_CMD_PAGEPROG);      // 0x02
    spiwrite((address >> 8) & 0xFF);     // address upper 8
    spiwrite((address) & 0xFF);      // address lower 8
  } else {
    return 0;
  }

  // Transfer data
  for (i = 0; i < len; i++)
  {
    spiwrite(buffer[i]);
  }
  // Write only occurs after the CS line is de-asserted
  digitalWrite(_ss, HIGH);

  // Wait at least 3ms (max page program time according to datasheet)
  delay(3);
  
  // Wait until the device is ready or a timeout occurs
  if (WaitForReady())
    return 0;

  return len;
}

/**************************************************************************/
/*! 
    @brief      Writes a continuous stream of data that will automatically
                cross page boundaries.
                
    @note       Before writing data, make sure that the appropriate sectors
                have been erased, otherwise the data will be meaningless.

    @param[in]  address
                The 24-bit address where the write will start.
    @param[out] *buffer
                Pointer to the buffer that will store the read results
    @param[in]  len
                Length of the buffer, within the limits of the starting
                address and size of the flash device.
*/
/**************************************************************************/
uint32_t Adafruit_SPIFlash::writeBuffer(uint32_t address, uint8_t *buffer, uint32_t len)
{
  uint32_t bytestowrite;
  uint32_t bufferoffset;
  uint32_t results;
  uint32_t byteswritten;

  // There's no point duplicating most error checks here since they will all be
  // done in the underlying call to spiflashWritePage

  // If the data is only on one page we can take a shortcut
  if ((address % pagesize) + len <= pagesize)
  {
    // Only one page ... write and be done with it
    return WritePage(address, buffer, len);
  }

  // Block spans multiple pages
  byteswritten = 0;
  bufferoffset = 0;
  while(len)
  {
    // Figure out how many bytes need to be written to this page
    bytestowrite = pagesize - (address % pagesize);
    // Write the current page
    results = WritePage(address, buffer+bufferoffset, bytestowrite);
	byteswritten += results;
    // Abort if we returned an error
    if (!results)
       return byteswritten;  // Something bad happened ... but return what we've written so far
    // Adjust address and len, and buffer offset
    address += bytestowrite;
    len -= bytestowrite;
    bufferoffset+=bytestowrite;
    // If the next page is the last one, write it and exit
    // otherwise stay in the the loop and keep writing
    if (len <= pagesize)
    {
      // Write the last frame and then quit
      results = WritePage(address, buffer+bufferoffset, len);
      byteswritten += results;
      // Abort if we returned an error
      if (!results)
        return byteswritten;  // Something bad happened ... but return what we've written so far
      // set len to zero to gracefully exit loop
      len = 0;
    }
  }

  // Return the number of bytes written
  return byteswritten;
}




/**************************************************************************/
/*! 
    @brief Finds append address

*/
/**************************************************************************/
uint32_t Adafruit_SPIFlash::findFirstEmptyAddr(void)
{
  uint32_t address, latestUsedAddr;
  uint8_t b;

  // Wait until the device is ready or a timeout occurs
  if (WaitForReady())
    return 0;


  for (int16_t page=0; page < W25Q16BV_PAGES; page++) {
    
    address = page * W25Q16BV_PAGESIZE;

    // read every byte of the page, compare to 0xFF
    // Send the read data command
    digitalWrite(_ss, LOW);
    spiwrite(SPIFLASH_SPI_DATAREAD);      // 0x03
    spiwrite((address >> 16) & 0xFF);     // address upper 8
    spiwrite((address >> 8) & 0xFF);      // address mid 8
    spiwrite(address & 0xFF);             // address lower 8

    //Serial.print("0x");
    //Serial.print(address, HEX);

    for (int16_t i=0; i<W25Q16BV_PAGESIZE; i++) {
      b = spiread();
      // Serial.print(", 0x");
      //Serial.print(b, HEX); 
      if (b == 0xFF) {
	// found a byte that is 'empty'
	Serial.print("Found an empty byte at ");
	Serial.println(latestUsedAddr, HEX);
	return address + i;
      }
    }
    digitalWrite(_ss, HIGH);
  }

  // Return bytes written
  return -1;
}


void Adafruit_SPIFlash::seek(uint32_t addr) {
  currentAddr = addr;
}

uint32_t Adafruit_SPIFlash::getAddr(void) {
  return currentAddr;
}

boolean Adafruit_SPIFlash::appendData(void) {
  uint32_t addr = findFirstEmptyAddr();
  if (addr == -1) return false;
  seek(addr);
}

size_t Adafruit_SPIFlash::write(uint8_t b) {
  // start out with the 'silliest' way
  uint8_t x[1];
  x[0] = b;
  
  return writeBuffer(currentAddr++, x, 1);
}
