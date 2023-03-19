/*

  This example downloads sketch update over NB/Cat1 network. (but can also be used for wifi / GSM / other types of networks as well)
  You can choose between HTTP and HTTPS connection.
  It downloads the binary file and saves it to the MKRMEM flash to store and apply the downloaded binary file.
  You can also works with other types of flash memory such as winbond W25QXX, just remember to adjust the flash page size if needed. (currently set to 256 bytes per page)

  To create the bin file for update of a SAMD board (except of M0),
  use in Arduino IDE command "Export compiled binary".
  To create a bin file for AVR boards see the instructions in README.MD.
  To try this example, you should have a web server where you put
  the binary update. (such as amazon s3)
  Modify the constants below to match your configuration.

  Important note:
  Don't forget to include the SFU library in the new sketch file as well!

  Created based on ArduinoOTA library in March 2023
  by Amir Pinkert
  based on Nicola Elia and Juraj Andrassy work

*/


#include <MKRNB.h>
#include <ArduinoHttpClient.h>
#include <SFU.h>
#include <Arduino_MKRMEM.h>


#include "arduino_secrets.h"

const char* BIN_FILENAME = "UPDATE.BIN";

// PIN Number
const char PINNUMBER[] = SECRET_PINNUMBER;


GPRS gprs;
NB nbAccess(false);

NBClient nbClient;



void setup() {

  Serial.begin(115200);
  while (!Serial)
    ;


  flash.begin();

  /*
  // you can format the flash at this point, but everything stored on it will be deleted so use with care
  Serial.println("Erasing chip ...");
  flash.eraseChip();

  Serial.println("Mounting ...");
  int res = filesystem.mount();
  if(res != SPIFFS_OK && res != SPIFFS_ERR_NOT_A_FS) {
    Serial.println("mount() failed with error code "); Serial.println(res); return;
  }

  Serial.println("Unmounting ...");
  filesystem.unmount();

  Serial.println("Formatting ...");
  res = filesystem.format();
  if(res != SPIFFS_OK) {
    Serial.println("format() failed with error code "); Serial.println(res); return;
  }
  */


  
  Serial.print("Mounting ... ");
  if(SPIFFS_OK != filesystem.mount()) {
    Serial.println("mount() failed with error code "); Serial.println(filesystem.err()); return;
  }
  Serial.println("OK");


  Serial.print("Checking ... ");
  if(SPIFFS_OK != filesystem.check()) {
    Serial.println("check() failed with error code "); Serial.println(filesystem.err()); return;
  }
  Serial.println("OK");

  // now connect to network

  Serial.println("Starting Arduino web client.");

  // connection state
  boolean connected = false;

  // start the modem with NB.begin()
  while (!connected) {
    if ((nbAccess.begin(PINNUMBER) == NB_READY) && (gprs.attachGPRS() == GPRS_READY)) {
      connected = true;
    } else {
      Serial.println("Not connected");
      delay(1000);
    }
  }

  // check for updates
  checkForOTAUpdates();

}

void loop() {


  // add your normal loop code below ...
}


void checkForOTAUpdates() {
  const char* SERVER = "www.XYZ.com";  // Set your correct hostname
  const unsigned short SERVER_PORT = 80;                              // Commonly 80 (HTTP) | 443 (HTTPS)
  const char* PATH = "/UPDATE.bin";                                 // Set the URI to the .bin firmware


  HttpClient client(nbClient, SERVER, SERVER_PORT);  // HTTP

  char buff[32];
  snprintf(buff, sizeof(buff), PATH);

  Serial.print("Check for update file ");
  Serial.println(buff);

  // Make the GET request
  client.get(buff);

  int statusCode = client.responseStatusCode();
  Serial.print("Update status code: ");
  Serial.println(statusCode);
  if (statusCode != 200) {
    client.stop();
    return;
  }

  unsigned long length = client.contentLength();
  if (length == HttpClient::kNoContentLengthHeader) {
    client.stop();
    Serial.println("Server didn't provide Content-length header. Can't continue with update.");
    return;
  }
  Serial.print("Server returned update file of size ");
  Serial.print(length);
  Serial.println(" bytes");


  Serial.println("Creating \"UPDATE.BIN\" ... ");
  File file = filesystem.open(BIN_FILENAME, CREATE | WRITE_ONLY| APPEND);
  Serial.println("File created");

  client.setTimeout(30000); // works well with 30 seconds

  byte b;
  uint16_t flashBufferSize = 256; // adjust according to your flash page size
  byte flashBuffer[flashBufferSize]; 
  uint16_t i = 0;

  int localChecksum = 0; // this is optional - you can get the intended checksum value from your server and compare with your local file to make sure your local file was downloaded correctly

  while (length > 0) {
    if (!client.readBytes(&b, 1)) // reading a byte with timeout
      break;

    // add to flashBuffer
    flashBuffer[i] = b;

    // add to checksum
    int bValue = (int)b;
    localChecksum = localChecksum + bValue;

    i ++;

    // check if flashBuffer is full
    if (i > (flashBufferSize - 1)){

      // write to file
      file.write((void *)flashBuffer, flashBufferSize);

      // reset i
      i = 0;

    }

    length--;


  }

  // add whats left on the flashBuffer
  if (i > 0){
  
    byte remainderBuffer[i];
    
    // fill buffer
    for (uint16_t x=0; x <= i; x++) {
      remainderBuffer[x] = flashBuffer[x];
    }

    // write to file
    file.write((void *)remainderBuffer, i);
  
  }


  file.close();
  client.stop();


  if (length > 0) {
    filesystem.remove(BIN_FILENAME);
    filesystem.unmount();
    Serial.print("Timeout downloading update file at ");
    Serial.print(length);
    Serial.println(" bytes. Can't continue with update.");
    return;
  }

  Serial.print("Local checksum:");
  Serial.println(localChecksum);

  Serial.print("Download finished. Unmounting ... ");
  filesystem.unmount();
  Serial.println("OK");

  Serial.println("Sketch update apply and reset.");

  Serial.flush();

}