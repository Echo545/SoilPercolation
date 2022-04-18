void setup() {
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:

}
// Importing necessary libraries
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClient.h>

#include "FS.h"

// Define pins
const int START_BUTTON = 22;

const int VALVE_ENABLE = 4;
const int VALVE_OPEN = 16;
const int VALVE_CLOSE = 17;

const int LED_TEST_STATUS = 12;
const int LED_ERROR_STATUS = 13;
const int LED_VALVE_STATUS = 14;

const int SENSOR_PIN = 34;

// SD Card setup
const char *OUTPUT_FILE = "/output.csv";
static bool hasSD = false;
String csv_data = "";
File csv_file;

// Setting network credentials
const char *ssid = "percolation";
const char *host = "percolation";

const char *input_parameter1 = "output";
const char *input_parameter2 = "state";

// Creating a AsyncWebServer object
AsyncWebServer server(80);


bool valve_open = false;
bool test_running = false;
int button_counter = 0;
time_t button_timer = 0;

const int LOG_WRITE_INTERVAL = 1000;    // 1 second
time_t log_timer = 0;

String error_message = "";

void setupSD() {
    if (SD.begin(SS)) {
        // Serial.println("SD Card initialized.");
        hasSD = true;
        csv_file = SD.open(OUTPUT_FILE, FILE_WRITE);

        // Write a new line to visually separate each new run
        csv_file.print("\n");

        csv_file.close();
    }
}

void write_log_file() {
    String printBuffer = "";

    if (hasSD) {
        csv_file = SD.open(OUTPUT_FILE, FILE_APPEND);

        if (csv_file) {
            // Write to SD card in format: (time, pressure, valve status)
            csv_file.print(String(millis()));
            csv_file.print(",");
            csv_file.print(readPressureSensor_RAW());
            csv_file.print(",");
            csv_file.print(digitalRead(VALVE_OPEN));
            csv_file.print("\n");

            csv_file.close();
            error_message = "";
        } else {
            error_message = "Failed to open CSV File";
        }
    } else {
        error_message = "No SD card found";
        setupSD();
    }
}

void openValve() {
    Serial.println("OPENING VALVE");
    digitalWrite(LED_VALVE_STATUS, HIGH);
    valve_open = true;

    digitalWrite(VALVE_OPEN, HIGH);
    digitalWrite(VALVE_CLOSE, LOW);

}

void closeValve() {
    Serial.println("CLOSING VALVE");
    digitalWrite(LED_VALVE_STATUS, LOW);
    valve_open = false;

    digitalWrite(VALVE_OPEN, LOW);
    digitalWrite(VALVE_CLOSE, HIGH);
}

String readPressureSensor_RAW() {
    return String(analogRead(SENSOR_PIN));
}

String readPressureSensor() {
    // Used for averaging data output
    static int PRINT_INTERVAL = 250;

    unsigned long timepoint_measure = millis();
    int total = 0;
    int count = 0;
    int raw_read;
    int result;

    while (count < PRINT_INTERVAL) {
        raw_read = analogRead(SENSOR_PIN);
        total += raw_read;
        count++;
    }

    result = total / count;
    count = 0;

    return String(result);
}

void setup() {
    // Serial port for debugging purposes
    Serial.begin(9600);

    // Sleep 1 seconds
    delay(1000);

    Serial.println("STARTING UP");

    // Setup IO pins
    pinMode(SENSOR_PIN, INPUT);
    pinMode(START_BUTTON, INPUT);
    pinMode(VALVE_ENABLE, OUTPUT);
    pinMode(VALVE_OPEN, OUTPUT);
    pinMode(VALVE_CLOSE, OUTPUT);
    pinMode(LED_TEST_STATUS, OUTPUT);
    pinMode(LED_ERROR_STATUS, OUTPUT);
    pinMode(LED_VALVE_STATUS, OUTPUT);

    digitalWrite(LED_ERROR_STATUS, LOW);
    digitalWrite(LED_VALVE_STATUS, LOW);
    digitalWrite(LED_TEST_STATUS, LOW);
    digitalWrite(VALVE_ENABLE, HIGH);

    // Close valve:
    closeValve();

    // Setup SD Output
    setupSD();

    // Setup Wireless AP
    WiFi.softAP(ssid);

    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);

    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html, processor);
    });

    server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!hasSD) {
            setupSD();
        }

        csv_file = SD.open(OUTPUT_FILE);

        String output_contents = csv_file.readString();
        // Serial.println(csv_file.readString());
        request->send(200, "text/html", output_contents);
        csv_file.close();
    });

    server.on("/rawpressure", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", readPressureSensor_RAW());
    });

    server.on("/pressure", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", readPressureSensor());
    });

    server.on("/graph", HTTP_GET, [](AsyncWebServerRequest *request) {
        File indexFile;
        String result = "Pending SD Card...";

        if (hasSD) {
            indexFile = SD.open("/graph.htm");
            result = indexFile.readString();
            indexFile.close();
        } else {
            setupSD();
        }

        request->send(200, "text/html", result);
    });

    server.serveStatic("/", SD, "/");

    // Start server
    server.begin();
}

void loop() {


    // Start button check
    if (digitalRead(START_BUTTON)) {
        button_counter++;

        if (button_counter > 10000 && millis() - button_timer > 2000) {
            if (valve_open) {
                button_timer = millis();
                closeValve();
            }
            else {
                button_timer = millis();
                openValve();
            }
        }
    }
    else
    {
        button_counter = 0;
    }


    // TODO: blink status LED while test is ready to start

    // TODO: set the date/time on the RTC module ONCE, then retrieve it

    // TODO: create a new csv file with the date/time for each test


    // TODO: write to log every 1 second
    if (millis() - log_timer > LOG_WRITE_INTERVAL) {
        log_timer = millis();
        write_log_file();
    }

}
