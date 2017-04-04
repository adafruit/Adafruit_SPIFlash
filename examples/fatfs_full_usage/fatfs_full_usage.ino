// Adafruit SPI Flash FatFs Full Usage Example
// Author: Tony DiCola
//
// This is an example of all the functions in the SPI Flash
// FatFs library.  Note that you MUST have a flash chip that's
// formatted with a flash filesystem before running.  See the
// fatfs_format example to perform this formatting.
//
// In general the API for this library closely follows the API
// for the Arduino SD card library.  However instead of interacting
// with a global SD object you create an instance of a fatfs class
// and use its open, exists, etc. functions.  See the SD library
// reference for more inspiration and examples to adapt:
//   https://www.arduino.cc/en/reference/SD
//
// Usage:
// - Modify the pins and type of fatfs object in the config
//   section below if necessary (usually not necessary).
// - Upload this sketch to your M0 express board.
// - Open the serial monitor at 115200 baud.  You should see the
//   example start to run and messages printed to the monitor.
//   If you don't see anything close the serial monitor, press
//   the board reset buttton, wait a few seconds, then open the
//   serial monitor again.
#include <SPI.h>
#include <Adafruit_SPIFlash.h>
#include <Adafruit_SPIFlash_FatFs.h>


// Configuration of the flash chip pins and flash fatfs object.
// You don't normally need to change these if using a Feather/Metro
// M0 express board.
#define FLASH_TYPE     SPIFLASHTYPE_W25Q16BV  // Flash chip type.
                                              // If you change this be
                                              // sure to change the fatfs
                                              // object type below to match.
#define FLASH_SS       SS1                    // Flash chip SS pin.
#define FLASH_SPI_PORT SPI1                   // What SPI port is Flash on?

Adafruit_SPIFlash flash(FLASH_SS, &FLASH_SPI_PORT);     // Use hardware SPI 

// Alternatively you can define and use non-SPI pins!
// Adafruit_SPIFlash flash(FLASH_SCK, FLASH_MISO, FLASH_MOSI, FLASH_SS);

Adafruit_W25Q16BV_FatFs fatfs(flash);


void setup() {
  // Initialize serial port and wait for it to open before continuing.
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  Serial.println("Adafruit SPI Flash FatFs Full Usage Example");
  
  // Initialize flash library and check its chip ID.
  if (!flash.begin(FLASH_TYPE)) {
    Serial.println("Error, failed to initialize flash chip!");
    while(1);
  }
  Serial.print("Flash chip JEDEC ID: 0x"); Serial.println(flash.GetJEDECID(), HEX);

  // First call begin to mount the filesystem.  Check that it returns true
  // to make sure the filesystem was mounted.
  if (!fatfs.begin()) {
    Serial.println("Error, failed to mount newly formatted filesystem!");
    Serial.println("Was the flash chip formatted with the fatfs_format example?");
    while(1);
  }
  Serial.println("Mounted filesystem!");

  // Check if a directory called 'test' exists and create it if not there.
  // Note you should _not_ add a trailing slash (like '/test/') to directory names!
  // You can use the same exists function to check for the existance of a file too.
  if (!fatfs.exists("/test")) {
    Serial.println("Test directory not found, creating...");
    // Use mkdir to create directory (note you should _not_ have a trailing slash).
    if (!fatfs.mkdir("/test")) {
      Serial.println("Error, failed to create test directory!");
      while(1);
    }
    Serial.println("Created test directory!");
  }

  // You can also create all the parent subdirectories automatically with mkdir.
  // For example to create the hierarchy /test/foo/bar:
  Serial.println("Creating deep folder structure...");
  if (!fatfs.mkdir("/test/foo/bar")) {
    Serial.println("Error, couldn't create deep directory structure!");
    while(1); 
  }
  // This will create the hierarchy /test/foo/baz, even when /test/foo already exists:
  if (!fatfs.mkdir("/test/foo/baz")) {
    Serial.println("Error, couldn't create deep directory structure!");
    while(1); 
  }
  Serial.println("Created /test/foo/bar and /test/foo/baz folders!");

  // Create a file in the test directory and write data to it.
  // Note the FILE_WRITE parameter which tells the library you intend to
  // write to the file.  This will create the file if it doesn't exist,
  // otherwise it will open the file and start appending new data to the
  // end of it.  More advanced users can specify different file modes by
  // using the FatFs f_open modes directly (which can be specified instead
  // of FILE_WRITE), see documentation at:
  //   http://elm-chan.org/fsw/ff/en/open.html
  File writeFile = fatfs.open("/test/test.txt", FILE_WRITE);
  if (!writeFile) {
    Serial.println("Error, failed to open test.txt for writing!");
    while(1);
  }
  Serial.println("Opened file /test/test.txt for writing/appending...");

  // Once open for writing you can print to the file as if you're printing
  // to the serial terminal, the same functions are available.
  writeFile.println("Hello world!");
  writeFile.print("Hello number: "); writeFile.println(123, DEC);
  writeFile.print("Hello hex number: 0x"); writeFile.println(123, HEX);

  // Close the file when finished writing.
  writeFile.close();
  Serial.println("Wrote to file /test/test.txt!");

  // Now open the same file but for reading.
  File readFile = fatfs.open("/test/test.txt", FILE_READ);
  if (!readFile) {
    Serial.println("Error, failed to open test.txt for reading!");
    while(1);
  }

  // Read data using the same read, find, readString, etc. functions as when using
  // the serial class.  See SD library File class for more documentation:
  //   https://www.arduino.cc/en/reference/SD
  // Read a line of data:
  String line = readFile.readStringUntil('\n');
  Serial.print("First line of test.txt: "); Serial.println(line);

  // You can get the current position, remaining data, and total size of the file:
  Serial.print("Total size of test.txt (bytes): "); Serial.println(readFile.size(), DEC);
  Serial.print("Current position in test.txt: "); Serial.println(readFile.position(), DEC);
  Serial.print("Available data to read in test.txt: "); Serial.println(readFile.available(), DEC);

  // And a few other interesting attributes of a file:
  Serial.print("File name: "); Serial.println(readFile.name());
  Serial.print("Is file a directory? "); Serial.println(readFile.isDirectory() ? "Yes" : "No");

  // You can seek around inside the file relative to the start of the file.
  // For example to skip back to the start (position 0):
  if (!readFile.seek(0)) {
    Serial.println("Error, failed to seek back to start of file!");
    while(1);
  }

  // And finally to read all the data and print it out a character at a time
  // (stopping when end of file is reached):
  Serial.println("Entire contents of test.txt:");
  while (readFile.available()) {
    char c = readFile.read();
    Serial.print(c);
  }

  // Close the file when finished reading.
  readFile.close();

  // You can open a directory to list all the children (files and directories).
  // Just like the SD library the File type represents either a file or directory.
  File testDir = fatfs.open("/test");
  if (!testDir) {
    Serial.println("Error, failed to open test directory!");
    while(1);
  }
  if (!testDir.isDirectory()) {
    Serial.println("Error, expected test to be a directory!");
    while(1);
  }
  Serial.println("Listing children of directory /test:");
  File child = testDir.openNextFile();
  while (child) {
    // Print the file name and mention if it's a directory.
    Serial.print("- "); Serial.print(child.name());
    if (child.isDirectory()) {
      Serial.print(" (directory)");
    }
    Serial.println();
    // Keep calling openNextFile to get a new file.
    // When you're done enumerating files an unopened one will
    // be returned (i.e. testing it for true/false like at the
    // top of this while loop will fail).
    child = testDir.openNextFile();
  }

  // If you want to list the files in the directory again call
  // rewindDirectory().  Then openNextFile will start from the
  // top again.
  testDir.rewindDirectory();

  // Delete a file with the remove command.  For example create a test2.txt file
  // inside /test/foo and then delete it.
  File test2File = fatfs.open("/test/foo/test2.txt", FILE_WRITE);
  test2File.close();
  Serial.println("Deleting /test/foo/test2.txt...");
  if (!fatfs.remove("/test/foo/test2.txt")) {
    Serial.println("Error, couldn't delete test.txt file!");
    while(1);
  }
  Serial.println("Deleted file!");

  // Delete a directory with the rmdir command.  Be careful as
  // this will delete EVERYTHING in the directory at all levels!
  // I.e. this is like running a recursive delete, rm -rf, in
  // unix filesystems!
  Serial.println("Deleting /test directory and everything inside it...");
  if (!fatfs.rmdir("/test")) {
    Serial.println("Error, couldn't delete test directory!");
    while(1);
  }
  // Check that test is really deleted.
  if (fatfs.exists("/test")) {
    Serial.println("Error, test directory was not deleted!");
    while(1);
  }
  Serial.println("Test directory was deleted!");
  
  Serial.println("Finished!");
}

void loop() {
  // Nothing to be done in the main loop.
  delay(100);
}
