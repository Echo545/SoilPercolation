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

const int ADC_TEST_PIN = 33;

// SD Card setup
const char *OUTPUT_FILE = "/output.csv";
static bool hasSD = false;
String csv_data = "";
File csv_file;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
const char *SSID = "percolation";

// IO variable declarations
const int LOG_WRITE_INTERVAL = 1000;    // 1 second
const int BUTTON_HOLD_THRESHOLD = 500;  // Approx 5 seconds
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
bool skip_drain_cycle = false;

int current_depth;
time_t drain_cycle_time;

// State constants for web interface
const String MODE_PENDING = "Awaiting Start";
const String MODE_DRAIN_CYCLE = "First Drain Cycle";
const String MODE_SATURATION = "Saturation Mode";
const String MODE_TEST = "Test Running";
const String MODE_STABILITY = "Stability Mode";
const String MODE_COMPLETE = "Test Complete";
const String MODE_UNNECESSARY = "Test not needed";
String current_mode = MODE_PENDING;

// Variables for saturation mode
const int SATURATION_MODE_DEPTH_MAX = 300;
const int SATURATION_MODE_DEPTH_MIN = 250;
float over_fill_amount = 0.0;
time_t saturation_start_time;
time_t saturation_end_time;
time_t saturation_mode_begin_time;
bool saturation_complete = false;
int cycle_count = 0;
int current_index = 0;
time_t prior_tests[2];

// Variables for testing mode
const int TEST_MODE_DEPTH_MAX = 150;
const int TEST_MODE_DEPTH_MIN = 50;
bool test_complete = false;
time_t test_start_time;
time_t test_end_time;
time_t test_mode_begin_time;

// Variables for stability mode
const int STABILITY_MODE_DEPTH_MAX = 150;
const int STABILITY_MODE_DEPTH_MIN = 50;
bool stability_complete = false;
time_t stability_start_time;
time_t stability_end_time;
time_t stability_mode_begin_time;

/**
 * @brief Helper function to increment index of prior_tests array
 *
 * @param current_index current index of the array
 * @return int next index to write to
 */
int next_test_index(int current_index) {
    current_index++;

    if (current_index > 2) {
        current_index = 0;
    }

    return current_index;
}

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
            // csv_file.print(",");
            // csv_file.print(digitalRead(VALVE_OPEN));
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
        int raw_sensor_read = analogRead(SENSOR_PIN);
        int offset_read = analogRead(ADC_TEST_PIN);

        // Serial.print("Raw sensor read: ");
        Serial.println(raw_sensor_read);

        // Serial.print("Offset read: ");
        // Serial.println(raw_sensor_read - offset_read);

        return raw_sensor_read;
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
        raw_read = readPressureSensor_RAW();
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
    // These constants were determined by lab tests with a meter stick
    const double SLOPE = 0.198;
    const int OFFSET = 89;
    const int MM_IN_CM = 10;

    float depth = (pressure * SLOPE - OFFSET) * MM_IN_CM;

    return depth;
}

/**
 * @brief Fills the percolation hole to the specified depth by opening the valve
 *
 * @param depth depth in mm to fill the hole
 * @return the time (in ms) that the valve started closing
 */
time_t fill_to_depth(float target_depth) {
    const float VALVE_TOGGLE_TIME = 3000.0;  // TODO: determine this in testing
    float current_depth = pressure_to_depth(readPressureSensor());
    time_t stop_time;

    // TODO: might have to define constants for time it takes to fill to X
    // height, could be function or constant

    // Open valve
    // Close when depth reaches target depth
    // Wait about 4 seconds after we close the valve for water to trickle
    // through Read depth Record over_fill_amount (mm) from water left in hose
    // Add the over_fill_amount to the end goal

    openValve();

    while (current_depth < target_depth) {
        current_depth = pressure_to_depth(readPressureSensor());
    }

    closeValve();
    stop_time = millis();

    delay(VALVE_TOGGLE_TIME);

    current_depth = pressure_to_depth(readPressureSensor());

    // set overflow amount, can't be 0
    // over_fill_amount = max(current_depth - target_depth, 0.0);
    over_fill_amount = current_depth - target_depth;

    // Return 0 if below target depth (error)
    return stop_time;
}

/**
 * @brief Ensures the drain cycle time is only initialized once even when
 * running in loop
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

/**
 * @brief Returns true if the debounced button is pressed
 * Also blinks the LED_TEST_STATUS while being held
 *
 * @note because this is being called in loop(), all variables are defined globally
 * outside of this function
 *
 * @return true if button is pressed
 */
bool button_check() {

    bool pressed = false;

    if (digitalRead(START_BUTTON)) {
        button_counter++;

        // Debounce button and hold for a few seconds
        if ((button_counter > BUTTON_HOLD_THRESHOLD) &&
            (millis() - button_timer > 2000)) {
            // Reset timer
            button_timer = millis();

            pressed = true;

            // Toggle the LED status
            if (test_running) {
                digitalWrite(LED_TEST_STATUS, HIGH);
                test_start_time = millis();
            } else {
                digitalWrite(LED_TEST_STATUS, LOW);
            }
        }
        // Blink the status LED while the button is being held
        else if ((button_counter > 50) &&
                 (button_counter < BUTTON_HOLD_THRESHOLD)) {
            // Toggle LED based on time
            if (button_counter % 50 > 25) {
                digitalWrite(LED_TEST_STATUS, HIGH);
            } else {
                digitalWrite(LED_TEST_STATUS, LOW);
            }
        }
    } else {
        button_counter = 0;
    }

    return pressed;
}

/**
 * @brief Writes to the log file no more than once a second
 */
void log() {
    if (millis() - log_timer > LOG_WRITE_INTERVAL) {
        log_timer = millis();
        write_log_file();
    }
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
        } else {
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

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", current_mode);
    });

    server.on("/error", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", error_message);
    });

    server.on("/depth", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html",
                      String(pressure_to_depth(readPressureSensor())));
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
    if (button_check()) {

        // Toggle the test status
        test_running = !test_running;
    }

    log();

    // TODO: set the date/time on the RTC module ONCE, then retrieve it

    // TODO: create a new csv file with the date/time for each test

    // Check error status
    if (error_message.length() > 0) {
        digitalWrite(LED_ERROR_STATUS, HIGH);
        test_running = false;
        Serial.println(error_message);
    } else {
        digitalWrite(LED_ERROR_STATUS, LOW);
    }

    // Run the test
    if (test_running) {
        // TODO: write test procedure

        // - - - Test first drain cycle - - -
        // TODO: indicate in the log that first drain cycle is starting

        current_mode = MODE_DRAIN_CYCLE;

        // Fill borehole to 300mm
        fill_to_depth(DRAIN_CYCLE_DEPTH_MAX);

        // Record time to drain hold completely
        drain_cycle_time = initialize_drain_cycle_time();

        current_depth = pressure_to_depth(readPressureSensor());

        while ((current_depth > 0)
                && (millis() - drain_cycle_time < FOUR_HOURS)
                && !skip_drain_cycle) {
            current_depth = pressure_to_depth(readPressureSensor());

            skip_drain_cycle = button_check();
            log();
        }

        // If time is less than 10 minutes, test is not necessary
        if (millis() - drain_cycle_time < TEN_MINUTES) {
            current_mode = MODE_UNNECESSARY;
            test_necessary = false;
            test_running = false;
        }

        // Otherwise proceed with the test
        if (test_necessary) {
            // Saturation mode:
            current_mode = MODE_SATURATION;
            saturation_mode_begin_time = millis();

            while (!saturation_complete &&
                   (millis() - saturation_mode_begin_time < FOUR_HOURS) && test_running) {
                cycle_count++;

                // Fill to 300mm
                saturation_start_time = fill_to_depth(SATURATION_MODE_DEPTH_MAX);

                // Auto record time and water level as water drops to 250mm

                // Wait until depth is at 250 + offset
                current_depth = pressure_to_depth(readPressureSensor());

                while (!saturation_complete && current_depth >
                       SATURATION_MODE_DEPTH_MIN + over_fill_amount) {

                    current_depth = pressure_to_depth(readPressureSensor());

                    saturation_complete = button_check();
                    log();
                }

                saturation_end_time = millis();

                prior_tests[current_index] = saturation_end_time;

                current_index = next_test_index(current_index);

                // Once the time of 3 consecutive saturation cycles are within
                // 5% of each other, the saturation phase is complete
                if (cycle_count > 3) {
                    // If all 3 prior tests are within 5% of each other,
                    // saturation is complete
                    // TODO: verify if this check is correct
                    if (abs(prior_tests[2] - prior_tests[1]) <
                        (prior_tests[0] * 0.05)) {
                        saturation_complete = true;
                    }
                }

                // Break condition
                // Saturation mode can be manually completed by button
                saturation_complete = button_check();
            }

            // Testing mode:
            current_mode = MODE_TEST;
            test_mode_begin_time = millis();
            cycle_count = 0;

            while (!test_complete) {
                cycle_count++;

                // Fill to 150mm
                test_start_time = fill_to_depth(TEST_MODE_DEPTH_MAX);

                // Wait until depth is at 50 + offset
                current_depth = pressure_to_depth(readPressureSensor());

                while (current_depth > TEST_MODE_DEPTH_MIN + over_fill_amount) {
                    current_depth = pressure_to_depth(readPressureSensor());

                    // TODO: add a break condition

                }

                test_end_time = millis();

                prior_tests[current_index] = test_end_time;

                current_index = next_test_index(current_index);

                if (cycle_count > 3) {
                    // TODO: verify if this check is correct
                    if (abs(prior_tests[2] - prior_tests[1]) <
                        (prior_tests[0] * 0.05)) {
                        test_complete = true;
                    }
                }
            }

            // Water level and time is auto recorded as the water level falls
            // from 150mm to 50mm Refill to 150mm upon hitting 50mm. Once the
            // time of 3 consecutive testing cycles are within 5% of each other,
            // the test phase is complete

            // Stability mode:
            // The water level and time it takes for water to drain from 150mm
            // to 50mm is auto recorded 5 times.

            current_mode = MODE_STABILITY;
            stability_mode_begin_time = millis();
            cycle_count = 0;

            while (!stability_complete) {
                for (int i = 0; i < 5; i++) {
                    cycle_count++;

                    // Fill to 150mm
                    stability_start_time = fill_to_depth(STABILITY_MODE_DEPTH_MAX);

                    // Wait until depth is at 50 + offset
                    current_depth = pressure_to_depth(readPressureSensor());
                }
            }
        } else {
            // TODO: elegantly end
        }

        current_mode = MODE_COMPLETE;
    }

    // TESTING SEGMENT:
    // Serial.println(readPressureSensor());
    // readPressureSensor();
    readPressureSensor_RAW();
}