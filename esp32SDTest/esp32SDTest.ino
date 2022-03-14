// #include "SD_MMC.h"
#include <SD.h>
#include "FS.h"

const char* OUTPUT_FILE = "output.csv";
const char* testfile = "/graph.htm";
static bool hasSD = false;
String csv_data = "";
File csv_file;

void setupSD() {
    if (SD.begin(SS)) {
     Serial.println("SD Card initialized.");
      hasSD = true;
      csv_file = SD.open(OUTPUT_FILE, FILE_WRITE);

      // Write a new line to visually seperate each new run
      csv_file.print("\n");

      csv_file.close();
  }
}

void setup() {

  Serial.begin(115200);


    if (SD.begin(SS)) {
     Serial.println("SD Card initialized.");

      File f = SD.open(testfile, FILE_READ);

      if (f) {
        Serial.println("File opened");
        while (f.available()) {
          Serial.write(f.read());
        }
        f.close();
      } else {
        Serial.println("Failed to open file");
      }

    } else {
      Serial.println("SD Card initialization failed.");
    }
}

void loop() {}