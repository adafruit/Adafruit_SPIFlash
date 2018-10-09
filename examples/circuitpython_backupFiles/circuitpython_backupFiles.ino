// Adafruit M0 Express CircuitPython Repair
// Author: Limor Fried
//
/* 
 *  This script can be useful if you seriously bork up your CircuitPython
 *  install. It will find any files named main.py, boot.py, main.txt, code.py
 *  or code.txt and move them to backup files. its a tad slow but then you
 *  can reload circuitpython safely. This example right now is only for
 *  the Metro M0 Express & Circuit Playground M0 but i have a...
 *  
 *  TODO: automagically detect if it's Feather/Metro/CircuitPlayground!
 */

#include <SPI.h>
#include <Adafruit_SPIFlash.h>
#include <Adafruit_SPIFlash_FatFs.h>
#include <Adafruit_NeoPixel.h>

// Configuration of the flash chip pins and flash fatfs object.
// You don't normally need to change these if using a Feather/Metro
// M0 express board.
#define FLASH_TYPE     SPIFLASHTYPE_W25Q16BV  // Flash chip type.
                                              // If you change this be
                                              // sure to change the fatfs
                                              // object type below to match.

#if !defined(SS1)
  #define FLASH_SS       SS                    // Flash chip SS pin.
  #define FLASH_SPI_PORT SPI                   // What SPI port is Flash on?
  #define NEOPIN         8
#else
  #define FLASH_SS       SS1                    // Flash chip SS pin.
  #define FLASH_SPI_PORT SPI1                   // What SPI port is Flash on?
  #define NEOPIN         40
#endif

#define BUFFERSIZ      200

Adafruit_SPIFlash flash(FLASH_SS, &FLASH_SPI_PORT);     // Use hardware SPI 

// Alternatively you can define and use non-SPI pins!
//Adafruit_SPIFlash flash(SCK1, MISO1, MOSI1, FLASH_SS);

// Finally create an Adafruit_M0_Express_CircuitPython object which gives
// an SD card-like interface to interacting with files stored in CircuitPython's
// flash filesystem.
Adafruit_M0_Express_CircuitPython pythonfs(flash);

Adafruit_NeoPixel pixel = Adafruit_NeoPixel(1, NEOPIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);
  while (!Serial);
  delay(1000); // small delay in case we want to watch it on the serial port  
  Serial.println("Adafruit Express CircuitPython Flash Repair");
  
  pixel.begin();             // This initializes the NeoPixel library
  pixel.setBrightness(30);   // not too bright!
  
  // Initialize flash library and check its chip ID.
  if (!flash.begin(FLASH_TYPE)) {
    Serial.println("Error, failed to initialize flash chip!");
    error(1);
  }
  Serial.print("Flash chip JEDEC ID: 0x"); Serial.println(flash.GetJEDECID(), HEX);
  if (flash.GetJEDECID() == 0) {
    Serial.println("Error, failed to initialize flash chip!");
    error(2);
  }
  // First call begin to mount the filesystem.  Check that it returns true
  // to make sure the filesystem was mounted.
  if (!pythonfs.begin()) {
    Serial.println("Failed to mount filesystem!");
    Serial.println("Was CircuitPython loaded on the board first to create the filesystem?");
    error(3);
  }
  Serial.println("Mounted filesystem!");
  
  moveFile("boot.py", "bootpy.bak");    
  moveFile("main.py", "mainpy.bak");    
  moveFile("main.txt", "maintxt.bak");    
  moveFile("code.py", "codepy.bak");    
  moveFile("code.txt", "codetxt.bak");    

  Serial.println("Finished!");
}

uint8_t i = 0;
void loop() {
  // white pixel pulse -> we're done
  pixel.setPixelColor(0, pixel.Color(i,i,i)); pixel.show();
  i++;
  delay(5);
}


boolean moveFile(char *file, char *dest) {
  if (! pythonfs.exists(file)) {
    Serial.print(file); Serial.println(" not found");
    return false;
  }
  if(pythonfs.exists(dest)) {
    Serial.println("Found old backup, removing...");
    if (!pythonfs.remove(dest)) {
      Serial.println("Error, couldn't delete ");
      Serial.print(dest);
      Serial.println(" file!");
      error(4);
    }
  }

  pixel.setPixelColor(0, pixel.Color(100,100,0)); pixel.show();

  File source = pythonfs.open(file, FILE_READ);
  File backup = pythonfs.open(dest, FILE_WRITE);
  Serial.println("Making backup!");
  Serial.println("\n---------------------\n");

  while (1) {
    int avail = source.available();
    //Serial.print("**Available bytes: "); Serial.print(avail); Serial.print("**");
    if (avail == 0) {
      Serial.println("\n---------------------\n");
      break;
    }
    int toread = min(BUFFERSIZ-1, avail);
    char buffer[BUFFERSIZ];
    
    int numread = source.read(buffer, toread);
    if (numread != toread) {
      Serial.print("Failed to read ");
      Serial.print(toread);
      Serial.print(" bytes, got "); 
      Serial.print(numread);
      error(5);
    }
    buffer[toread] = 0;      
    Serial.print(buffer);
    if (backup.write(buffer, toread) != toread) {
        Serial.println("Error, couldn't write data to backup file!");
        error(6);
    }
  }

  pixel.setPixelColor(0, pixel.Color(100,0,100)); pixel.show();

  Serial.print("Original file size: "); Serial.println(source.size());
  Serial.print("Backup file size: "); Serial.println(backup.size());
  backup.close();
  if (source.size() == backup.size()) {
     if (!pythonfs.remove(file)) {
        Serial.print("Error, couldn't delete ");
        Serial.println(file);
        error(10);
     } 
  }
  pixel.setPixelColor(0, pixel.Color(0,100,0)); pixel.show();
  delay(100);
  return true;
}

void error(uint8_t i) {
  while (1) {
    for (int x = 0; x< i; x++) {
      pixel.setPixelColor(0, pixel.Color(100,0,0)); pixel.show();
      delay(200);
      pixel.setPixelColor(0, pixel.Color(0,0,0)); pixel.show();
      delay(200);
    }
    delay(1000);    
  }
}