#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncWebServer.h>
#include <SPI.h>
#include <SD.h>

#define DBG_OUTPUT_PORT Serial
#define ANALOG_PIN A0

// setup webserver
const char* ssid = "percolation";
const char* host = "percolation";

AsyncWebServer server(80);

// setup SD card
static bool hasSD = false;
File uploadFile;

/**
 * @brief Reads the pressure sensor data
 *
 * TODO: Process the raw data
 *
 * @return pressure sensor data as a string
 */
String readPressureSensor() {
    // Used for averaging data output
    static int PRINT_INTERVAL = 70;

    unsigned long timepoint_measure = millis();
    int total = 0;
    int count = 0;
    int result;

    while (count < PRINT_INTERVAL) {
        total += analogRead(ANALOG_PIN);
        count++;
    }

    result = total / count;
    count = 0;

    return String(result);
}

/**
 *
 */
bool loadFromSdCard(String path) {
  String dataType = "text/plain";
  if (path.endsWith("/")) path += "index.htm";

  if (path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if (path.endsWith(".htm")) dataType = "text/html";
  else if (path.endsWith(".css")) dataType = "text/css";
  else if (path.endsWith(".js")) dataType = "application/javascript";
  else if (path.endsWith(".png")) dataType = "image/png";
  else if (path.endsWith(".gif")) dataType = "image/gif";
  else if (path.endsWith(".jpg")) dataType = "image/jpeg";
  else if (path.endsWith(".ico")) dataType = "image/x-icon";
  else if (path.endsWith(".xml")) dataType = "text/xml";
  else if (path.endsWith(".pdf")) dataType = "application/pdf";
  else if (path.endsWith(".zip")) dataType = "application/zip";

  File dataFile = SD.open(path.c_str());
  if (dataFile.isDirectory()) {
    path += "/index.htm";
    dataType = "text/html";
    dataFile = SD.open(path.c_str());
  }

  if (!dataFile)
    return false;

  if (server.hasArg("download")) dataType = "application/octet-stream";

  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
    DBG_OUTPUT_PORT.println("Sent less data than expected!");
  }

  dataFile.close();
  return true;
}

// Uploads a file to the root of the SD card via POST
void handleFileUpload() {
  if (server.uri() != "/edit") return;

  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {

    if (SD.exists((char *)upload.filename.c_str())) SD.remove((char *)upload.filename.c_str());
    uploadFile = SD.open(upload.filename.c_str(), FILE_WRITE);
    DBG_OUTPUT_PORT.print("Upload: START, filename: "); DBG_OUTPUT_PORT.println(upload.filename);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
    DBG_OUTPUT_PORT.print("Upload: WRITE, Bytes: "); DBG_OUTPUT_PORT.println(upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) uploadFile.close();
    DBG_OUTPUT_PORT.print("Upload: END, Size: "); DBG_OUTPUT_PORT.println(upload.totalSize);
  }
}

void setup() {
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:

}
