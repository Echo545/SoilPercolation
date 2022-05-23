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
bool valve_open = false;
bool test_running = false;
int button_counter = 0; 
time_t button_timer = 0;
time_t log_timer = 0;
String error_message = "";

// Define time constants (milliseconds)
const int TEN_MINUTES = 600000;
const int FOUR_HOURS = 14400000;

// Test process variable declarations
const int DRAIN_CYCLE_DEPTH_MAX = 300;
bool test_necessary = true;
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
// All depths in mm's
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

// Variables for processing raw pressure
const int PREV_READINGS_SIZE = 50;
int previous_readings_index = 0;
int previous_readings[PREV_READINGS_SIZE];
bool sample_recorded = false;
int prior_processed_read = 0;


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
    if (hasSD) {
        csv_file = SD.open(OUTPUT_FILE, FILE_APPEND);

        if (csv_file) {
            // Write to SD card in format: (time, raw pressure)
            csv_file.print(String(millis()));
            csv_file.print(",");
            csv_file.print(String(readPressureSensor_RAW()));
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
void open_valve() {
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
void close_valve() {
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

        // For debugging
        // Serial.print("Raw sensor read: ");
        // Serial.println(raw_sensor_read);

        return raw_sensor_read;
    }

/**
 * @brief Reads the input from the pressure sensor and smooths it
 *
 * @return String the smoothed pressure sensor data
 */
int readPressureSensor() {
    time_t pressure_average_timer = millis();
    const int SMOOTHING_INTERVAL = 200;
    int total = 0;
    int count = 0;
    int current_pressure;
    int result;
    bool encountered_out_of_bounds_reading = false;

    while (millis() - pressure_average_timer < SMOOTHING_INTERVAL) {
        current_pressure = readPressureSensor_RAW();

        // Keep running average of previous 50 RAW reads to detect invalid pressure readings

        // Update list of prior tests if test is running
        if (test_running) {
            previous_readings[previous_readings_index] = current_pressure;
            previous_readings_index++;
        }

        // Check for index out of bounds for list of previous readings
        if (previous_readings_index >= PREV_READINGS_SIZE) {
            previous_readings_index = 0;
            sample_recorded = true;
        }

        // If at least 50 readings have been recorded, then compare with running average
        if (sample_recorded) {

            // If current read is outside of threshold, sensor reading is bad, don't add to total
            if (is_within_boundry(current_pressure)) {
                total += current_pressure;

                // For debugging
                // Serial.print("is within boundry! New total: ");
                // Serial.println(total);
            }
            else {
                // For debugging
                // Serial.print("not in boundry: ");
                // Serial.println(current_pressure);

                encountered_out_of_bounds_reading = true;
            }
        }
        // Otherwise if less than 50 tests have been recorded, just add it to total
        else {
            total += current_pressure;
        }
        count++;
    }

    result = total / count;
    count = 0;

    // Handle processed read of 0 when out of bounds reading occurs
    if (result > 0) {

        // Store prior processed read for future comparasions
        prior_processed_read = result;
    }
    else if (prior_processed_read == 0) {
        int comparasion_index;
        int i = 1;

        // set result to last non zero value
        while (i < PREV_READINGS_SIZE && result == 0) {

            comparasion_index = previous_readings_index - i;

            // Check if index is withing range of 0 to PREV_READINGS_SIZE - 1
            if (comparasion_index < 0) {
                result = previous_readings[PREV_READINGS_SIZE - comparasion_index];
            }
            else {
                result = previous_readings[comparasion_index];
            }
            i++;
        }
        // For debugging
        // Serial.println("Prior processed read was 0! Processed read of 0 has been updated to: ");
    }
    else {
        result = prior_processed_read;
        
        // For debugging
        // Serial.println("Processed read of 0 has been updated to: ");
    }

    // For debugging
    // Serial.print("Processed read: ");
    // Serial.println(result);

    return result;
}

/**
 * @brief Returns true if the new pressure given is within the boundry percentage of the average of the previous 10 tests
 * 
 * @note Boundry can be changed to fit current needs inorder to avoid recording noise
 *
 * @param pressure New Raw pressure reading  
 * @return true if all tests were within the boundry
 */
bool is_within_boundry(int pressure) {
    const float BOUNDRY = 0.25;
    bool within_boundry = false;
    float avg;
    int total = 0;
    int min;
    int max;

    // take the average of the previous pressure readings
    for (int i = 0; i < PREV_READINGS_SIZE; i++) {
        total += previous_readings[i];
    }

    avg = total / PREV_READINGS_SIZE;

    // Find limits of pressure range based on boundry percentage
    min = avg - (avg * BOUNDRY);
    max = avg + (avg * BOUNDRY);

    // check that given pressure is within these values
    if (min <= pressure && pressure <= max) {
        within_boundry = true;
    }
    else {
        Serial.print("Pressure of: ");
        Serial.print(pressure);
        Serial.print(" was outside of bounds for average: ");
        Serial.println(avg);
    }

    return within_boundry;
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

    // For debugging
    Serial.println((String) "current depth: " + depth + "mm");

    return depth;
}

/**
 * @brief Fills the percolation hole to the specified depth by opening the valve
 *
 * @param target_depth depth in mm to fill the hole
 * @return the time (in ms) that the valve started closing
 */
time_t fill_to_depth(float target_depth) {
    const float VALVE_TOGGLE_TIME = 3000.0;     // milliseconds
    float current_depth = pressure_to_depth(readPressureSensor());
    time_t stop_time;

    open_valve();

    // Close valve when depth reaches target depth
    while (current_depth < target_depth) {
        current_depth = pressure_to_depth(readPressureSensor());
    }

    close_valve();
    stop_time = millis();

    // Wait about some seconds after we close the valve for water to trickle
    delay(VALVE_TOGGLE_TIME);

    // through Read depth Record over_fill_amount (mm) from water left in hose
    current_depth = pressure_to_depth(readPressureSensor());

    // Add the over_fill_amount to the end goal
    over_fill_amount = current_depth - target_depth;

    return stop_time;
}

/**
 * @brief Returns true if the debounced button is pressed
 *
 * @note because this is being called in loop(), all variables are defined globally
 * outside of this function
 *
 * @return true if button is pressed
 */
bool button_check() {
    const int BUTTON_HOLD_THRESHOLD = 5000; // length button has to be held for (in cycles, not in ms)
    const int BUTTON_PRESS_COOLDOWN = 2000; // minimum amount of time between button presses (in ms)
    bool pressed = false;

    if (digitalRead(START_BUTTON)) {
        button_counter++;

        // Debounce button and hold for a few seconds
        if ((button_counter > BUTTON_HOLD_THRESHOLD) && (millis() - button_timer > BUTTON_PRESS_COOLDOWN)) {
            // Reset timer
            button_timer = millis();

            pressed = true;
        }
        else {
            update_status_led();
        }
    } else {
        button_counter = 0;
        update_status_led();
    }

    if (pressed) {
        Serial.println("BUTTON WAS PRESSED");
    }

    return pressed;
}

/**
 * @brief sets the status LED according to if the test is running
 */
void update_status_led() {
    // Update the LED according to the test status when the button isn't being pressed
    // If test is running, green light should be on, otherwise off
    if (test_running) {
        digitalWrite(LED_TEST_STATUS, HIGH);
    } else {
        digitalWrite(LED_TEST_STATUS, LOW);
    }
}

/**
 * @brief Returns true if the three given tests completion times are completed within 5% of eachother
 *
 * @param tests array of previous 3 test completion times
 * @return true if all tests were within 5%
 */
bool is_within_five_percent(time_t tests[]) {
    float min_threshold;
    float max_threshold;
    int next_test_index;
    int following_test_index;
    bool within_threshold = true;
    int i = 0;

    while (within_threshold && i < 3) {
        // Find boundry within 5% of each tests time and check that the other tests were completed within that boundry
        min_threshold = tests[i] - (tests[i] * 0.05);
        max_threshold = tests[i] + (tests[i] * 0.05);

        next_test_index = (i + 1) % 3;
        following_test_index = (i + 2) % 3;

        // Check that the other two tests fall within these values
        if (!((min_threshold <= tests[next_test_index] && tests[next_test_index] <= max_threshold)
            && (min_threshold <= tests[following_test_index] && tests[following_test_index] <= max_threshold))) {
            within_threshold = false;
        }

        i++;
    }

    return within_threshold;
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
    close_valve();

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

        // For debugging
        Serial.println("Toggling test!");
        Serial.println((String) "Current mode: " + current_mode);

        update_status_led();
        log();
    }
    
    // Record the time when the test starts
    if (test_running) {
        test_start_time = millis();
    }


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
        // - - - - - - Test first drain cycle - - - - - -
        current_mode = MODE_DRAIN_CYCLE;        
        Serial.println((String) "New mode: " + current_mode);

        // Fill borehole to 300mm
        fill_to_depth(DRAIN_CYCLE_DEPTH_MAX);

        // Record time to drain hold completely
        drain_cycle_time = millis();

        current_depth = pressure_to_depth(readPressureSensor());

        // Wait for hole to drain, or for 4 hours to pass
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

            // - - - - - - Saturation mode: - - - - - -
            current_mode = MODE_SATURATION;

            Serial.println((String) "New mode: " + current_mode);

            saturation_mode_begin_time = millis();

            while (!saturation_complete &&
                   (millis() - saturation_mode_begin_time < FOUR_HOURS) && test_running) {
                cycle_count++;

                // Fill to 300mm
                saturation_start_time = fill_to_depth(SATURATION_MODE_DEPTH_MAX);

                // Auto record time and water level as water drops to 250mm
                current_depth = pressure_to_depth(readPressureSensor());

                // Wait until depth is at 250 + offset
                while (!saturation_complete && current_depth >
                       SATURATION_MODE_DEPTH_MIN + over_fill_amount) {

                    current_depth = pressure_to_depth(readPressureSensor());

                    saturation_complete = button_check();
                    log();
                }

                saturation_end_time = millis();

                prior_tests[current_index] = saturation_end_time - saturation_start_time;

                current_index = next_test_index(current_index);

                // Once the time of 3 consecutive saturation cycles are within
                // 5% of each other, the saturation phase is complete
                if (cycle_count > 3) {
                    saturation_complete = is_within_five_percent(prior_tests);
                }

                // Break condition
                // Saturation mode can be manually completed by button
                saturation_complete = button_check();
                log();
            }

            // - - - - - - Testing mode: - - - - - -
            // Water level and time is auto recorded as the water level falls
            // from 150mm to 50mm Refill to 150mm upon hitting 50mm. Once the
            // time of 3 consecutive testing cycles are within 5% of each other,
            // the test phase is complete

            current_mode = MODE_TEST;
            Serial.println((String) "New mode: " + current_mode);

            test_mode_begin_time = millis();
            cycle_count = 0;

            while (!test_complete) {
                cycle_count++;

                // Fill to 150mm
                test_start_time = fill_to_depth(TEST_MODE_DEPTH_MAX);
                current_depth = pressure_to_depth(readPressureSensor());

                // Wait until depth is at 50 + offset
                while (current_depth > TEST_MODE_DEPTH_MIN + over_fill_amount && !test_complete) {
                    current_depth = pressure_to_depth(readPressureSensor());
                    log();

                    // break condition
                    if (button_check()) {
                        test_complete = true;
                    }
                }

                test_end_time = millis();
                prior_tests[current_index] = test_end_time - test_start_time;
                current_index = next_test_index(current_index);

                if (cycle_count > 3) {
                    test_complete = is_within_five_percent(prior_tests);
                }

                log();
            }

            // - - - - - - Stability mode: - - - - - -
            // The water level and time it takes for water to drain from 150mm
            // to 50mm is auto recorded 5 times.

            current_mode = MODE_STABILITY;
            Serial.println((String) "New mode: " + current_mode);

            stability_mode_begin_time = millis();
            cycle_count = 0;

            // fill 5 times and wait to drain
            for (int i = 0; i < 5; i++) {
                cycle_count++;

                // Fill to 150mm
                stability_start_time = fill_to_depth(STABILITY_MODE_DEPTH_MAX);

                current_depth = pressure_to_depth(readPressureSensor());

                // Wait until depth is at 50 + offset
                while (current_depth > STABILITY_MODE_DEPTH_MIN + over_fill_amount && !stability_complete) {
                    current_depth = pressure_to_depth(readPressureSensor());

                    log();
                    stability_complete = button_check();
                }

                log();
            }
            stability_complete = true;
        }
        current_mode = MODE_COMPLETE;
        Serial.println((String) "New mode: " + current_mode);

        if (!test_necessary) {
            Serial.println("Test was not necessary!");
        }
    }
}
