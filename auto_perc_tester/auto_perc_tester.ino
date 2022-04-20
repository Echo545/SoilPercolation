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
const char* OUTPUT_FILE = "/output.csv";
static bool hasSD = false;
String csv_data = "";
File csv_file;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
const char* SSID = "percolation";

// IO variable declarations
const int LOG_WRITE_INTERVAL = 1000;        // 1 second
const int BUTTON_HOLD_THRESHOLD = 500;      // Approx 5 seconds
bool valve_open = false;
bool test_running = false;

int button_counter = 0;
time_t button_timer = 0;
time_t log_timer = 0;
time_t status_led_timer = 0;
bool status_led_state = false;
String error_message = "";

// Define time constants (milliseconds)
const int TEN_MINUTES = 600000;
const int FOUR_HOURS = 14400000;

// Test process variable declarations
const int DRAIN_CYCLE_DEPTH_MAX = 300;
const int DRAIN_CYCLE_DEPTH_MIN = 0;

bool test_necessary = true;

bool drain_cycle_initialized = false;

int current_depth;
time_t drain_cycle_time;
time_t test_start_time;


/**
 * @brief Initializes the SD card
 *
 */
void setupSD() {
    if (SD.begin(SS)) {
        hasSD = true;
        csv_file = SD.open(OUTPUT_FILE, FILE_APPEND);

        // Write a new line to visually separate each new run
        csv_file.print("\n");

        csv_file.close();
    }
}

/**
 * @brief Writes the time and pressure sensor data to log
 * If there's an error, updates error_message
 */
void write_log_file() {
    String printBuffer = "";

    if (hasSD) {
        csv_file = SD.open(OUTPUT_FILE, FILE_APPEND);

        if (csv_file) {
            // Write to SD card in format: (time, pressure, valve status)
            csv_file.print(String(millis()));
            csv_file.print(",");
            csv_file.print(String(readPressureSensor_RAW()));
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

/**
 * @brief Opens the valve and updates the LED status
 *
 */
void openValve() {
    Serial.println("OPENING VALVE");
    digitalWrite(LED_VALVE_STATUS, HIGH);
    valve_open = true;

    digitalWrite(VALVE_OPEN, HIGH);
    digitalWrite(VALVE_CLOSE, LOW);
}

/**
 * @brief Closes the valve and updates the LED status
 *
 */
void closeValve() {
    Serial.println("CLOSING VALVE");
    digitalWrite(LED_VALVE_STATUS, LOW);
    valve_open = false;

    digitalWrite(VALVE_OPEN, LOW);
    digitalWrite(VALVE_CLOSE, HIGH);
}

/**
 * @brief Reads the raw analog input from the pressure sensor
 *
 * @return the analog input
 */
int readPressureSensor_RAW() {
    return analogRead(SENSOR_PIN);
}

/**
 * @brief Reads the input from the pressure sensor and smooths it
 *
 * @return String the smoothed pressure sensor data
 */
int readPressureSensor() {
    // Used for averaging data output
    const int SMOOTHING_INTERVAL = 250;
    int total = 0;
    int count = 0;
    int raw_read;
    int result;

    while (count < SMOOTHING_INTERVAL) {
        raw_read = analogRead(SENSOR_PIN);
        total += raw_read;
        count++;
    }

    result = total / count;
    count = 0;

    return result;
}

/**
 * @brief Converts a pressure sensor reading to depth in mm
 *
 * @param pressure input from pressure sensor
 * @return the depth in mm
 */
float pressure_to_depth(int pressure) {
    float depth = 0;

    // TODO:

    return depth;
}

/**
 * @brief Fills the percolation hole to the specified depth by opening the valve
 *
 * @param depth depth in mm to fill the hole
 */
void fill_to_depth(float depth) {

    // TODO:

}

/**
 * @brief Ensures the drain cycle time is only initialized once even when running in loop
 *
 * @return time_t drain cycle time
 */
time_t initialize_drain_cycle_time() {

    time_t time = drain_cycle_time;

    if (!drain_cycle_initialized) {
        drain_cycle_initialized = true;
        time = millis();
    }

    return time;
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
    WiFi.softAP(SSID);
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);

    // Route web end points:
    server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!hasSD) {
            setupSD();
            request->send(500, "text/html", "SD Card not found.");
        }
        else {
            csv_file = SD.open(OUTPUT_FILE);
            request->send(200, "text/html", csv_file.readString());
            csv_file.close();
        }
    });

    server.on("/rawpressure", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", String(readPressureSensor_RAW()));
    });

    server.on("/pressure", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", String(readPressureSensor()));
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
            request->send(500, "text/html", "SD Card not found.");
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

        // Debounce button and hold for a few seconds
        if ((button_counter > BUTTON_HOLD_THRESHOLD) && (millis() - button_timer > 2000)) {

            // Reset timer
            button_timer = millis();

            // Toggle the test status
            test_running = !test_running;

            // Toggle the LED status
            if (test_running) {
                digitalWrite(LED_TEST_STATUS, HIGH);
                test_start_time = millis();
            }
            else {
                digitalWrite(LED_TEST_STATUS, LOW);
            }
        }
        // Blink the status LED while the button is being held
        else if ((button_counter > 50) && (button_counter < BUTTON_HOLD_THRESHOLD)) {

            // Toggle LED based on time
            if (button_counter % 50 > 25) {
                digitalWrite(LED_TEST_STATUS, HIGH);
            }
            else {
                digitalWrite(LED_TEST_STATUS, LOW);
            }
        }
    }
    else {
        button_counter = 0;
    }


    // TODO: set the date/time on the RTC module ONCE, then retrieve it

    // TODO: create a new csv file with the date/time for each test


    // Check error status to update LED's
    if (error_message.length() > 0) {
        digitalWrite(LED_ERROR_STATUS, HIGH);
        Serial.println(error_message);
    }
    else {
        digitalWrite(LED_ERROR_STATUS, LOW);
    }


    // Run the test
    if (test_running) {

        // Write to log every 1 second
        if (millis() - log_timer > LOG_WRITE_INTERVAL) {
            log_timer = millis();
            write_log_file();
        }



        // TODO: write test procedure

        // - - - Test first drain cycle - - -
        // TODO: indicate in the log that first drain cycle is starting

        // Fill borehole to 300mm
        fill_to_depth(DRAIN_CYCLE_DEPTH_MAX);

        // Record time to drain hold completely
        drain_cycle_time = initialize_drain_cycle_time();

        current_depth = pressure_to_depth(readPressureSensor());

        while ((current_depth > 0) && (millis() - drain_cycle_time < FOUR_HOURS)) {
            current_depth = pressure_to_depth(readPressureSensor());

            // TODO: check for break condition (override button pressed)
        }


        // If time is less than 10 minutes, test is not necessary
        if (millis() - drain_cycle_time < TEN_MINUTES) {
            test_necessary = false;
            test_running = false;

            // TODO: elegantly end
        }

        // Otherwise proceed with the test
        if (test_necessary) {

        // Saturation mode:

            // Fill to 300mm
            // Auto record time and water level as water drops to 250mm
            // Refill to 300mm upon hitting 250mm.
            // Once the time of 3 consecutive saturation cycles are within 5% of each other, the saturation phase is complete

            // If 4 hours have passed in saturation phase, move to the next mode
            // Saturation mode can be manually completed by a switch/button

        // Testing mode:

            // Fill to 150mm
            // Water level and time is auto recorded as the water level falls from 150mm to 50mm
            // Refill to 150mm upon hitting 50mm.
            // Once the time of 3 consecutive testing cycles are within 5% of each other, the test phase is complete

        // Stability mode:
            // The water level and time it takes for water to drain from 150mm to 50mm is auto recorded 5 times.

        }
    }
}