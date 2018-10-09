/* This is my go-to example for erasing, dumping, writing and verifying SPI flash!  */

#include <SD.h>
void PrintHex(const byte * data, const uint32_t numBytes);
#include <Adafruit_SPIFlash.h>

#if !defined(SS1)
  #define FLASH_SS       SS                    // Flash chip SS pin.
  #define FLASH_SPI_PORT SPI                   // What SPI port is Flash on?
#else
  #define FLASH_SS       SS1                    // Flash chip SS pin.
  #define FLASH_SPI_PORT SPI1                   // What SPI port is Flash on?
#endif
Adafruit_SPIFlash flash(FLASH_SS, &FLASH_SPI_PORT);     // Use hardware SPI 

#define SD_CS 2
#define MAXPAGESIZE 256

File dataFile;
uint8_t buffer[MAXPAGESIZE], buffer2[MAXPAGESIZE];
uint32_t results;

void error(char *str) {
  Serial.println(str);
  while (1);
}

void setup(void) 
{
  while (!Serial);
  Serial.begin(115200);

  flash.begin(SPIFLASHTYPE_W25Q16BV);

  uint8_t manid, devid;
  flash.GetManufacturerInfo(&manid, &devid);
  Serial.print(F("Manufacturer: 0x")); Serial.println(manid, HEX);
  Serial.print(F("Device ID: 0x")); Serial.println(devid, HEX);

  Serial.print("Initializing SD card...");
  // see if the card is present and can be initialized:
  if (!SD.begin(SD_CS)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    while (1);
  }

}

void loop(void) 
{
  while (Serial.available()) { Serial.read(); }
  Serial.println(F("FLASH DUMPER - Hello! Press E (rase), W (rite), V (erify), or D (ump) to begin"));

  while (!Serial.available());
  char cmd = toupper(Serial.read());
  
  uint16_t pagesize = flash.pageSize();
  Serial.print("Page size: ");
  Serial.println(pagesize);

  if (cmd == 'D') {
     // open the file. note that only one file can be open at a time,
     // so you have to close this one before opening another.
     // Open up the file we're going to log to!
     dataFile = SD.open("flshdump.bin", FILE_WRITE);
     if (! dataFile) {
       error("error opening flshdump.bin");
     }
    
    Serial.println("Dumping FLASH to disk");
    for (int32_t page=0; page < flash.numPages() ; page++)  {
      memset(buffer, 0, pagesize);
      Serial.print("// Reading page ");
      Serial.println(page);
      uint16_t r = flash.readBuffer (page * pagesize, buffer, pagesize);
      //Serial.print(r); Serial.print(' '); PrintHex(buffer, r);
      dataFile.write(buffer, r);
    }  
    dataFile.flush();
    dataFile.close();
  }
  if (cmd == 'E') {
    Serial.println("Erasing chip");
    flash.EraseChip();
    
  }
  if (cmd == 'V') {
     dataFile = SD.open("flshdump.bin", FILE_READ);
     if (! dataFile) {
       error("error opening flshdump.bin");
     }
    Serial.println("Verifying FLASH from disk");
    for (int32_t page=0; page < flash.numPages() ; page++)  {
      memset(buffer, 0, pagesize);
      memset(buffer2, 0, pagesize);
      
      Serial.print("// Verifying page ");  Serial.println(page);
      
      uint16_t r = flash.readBuffer (page * pagesize, buffer, pagesize);
      if (r != pagesize) {
        error("Flash read failure");
      }
      
      if (r != dataFile.read(buffer2, r)) {
        error("SD read failure");
      }

      if (memcmp(buffer, buffer2, r) != 0) {
        PrintHex(buffer, r);
        PrintHex(buffer2, r);
        Serial.println("verification failed");
        return;
      }      
    }  
    Serial.println("Done!");
    dataFile.close();
  }  

  if (cmd == 'W') {
     dataFile = SD.open("flshdump.bin", FILE_READ);
     if (! dataFile) {
       error("error opening flshdump.bin");
     }

    Serial.println("Writing FLASH from disk");
    for (int32_t page=0; page < flash.numPages() ; page++)  {
      memset(buffer, 0, pagesize);
      
      int16_t r = dataFile.read(buffer, pagesize);
      if (r == 0) 
        break;
      Serial.print("// Writing page ");  Serial.println(page);

      if (r != flash.writeBuffer (page * r, buffer, r)) {
        error("Flash write failure");
      }
    }  
    Serial.println("Done!");
    dataFile.close();
  } 
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
void PrintHexChar(const byte * data, const uint32_t numBytes)
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
	  Serial.print('.');
	else
	  Serial.write(data[szPos]);
  }
  Serial.println("");
}

/**************************************************************************/
/*! 
    @brief  Prints a hexadecimal value in plain characters

    @param  data      Pointer to the byte data
    @param  numBytes  Data length in bytes
*/
/**************************************************************************/
void PrintHex(const byte * data, const uint32_t numBytes)
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



