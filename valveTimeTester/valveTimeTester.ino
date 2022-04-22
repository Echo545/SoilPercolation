// Define pins
const int START_BUTTON = 22;
const int VALVE_ENABLE = 4;
const int VALVE_OPEN = 16;
const int VALVE_CLOSE = 17;
const int LED_TEST_STATUS = 12;
const int LED_ERROR_STATUS = 13;
const int LED_VALVE_STATUS = 14;
const int SENSOR_PIN = 34;

// IO variable declarations
const int BUTTON_HOLD_THRESHOLD = 12;
bool valve_open = false;

int button_counter = 0;
time_t button_timer = 0;

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
int readPressureSensor_RAW() { return analogRead(SENSOR_PIN); }

/**
 * @brief Reads the input from the pressure sensor and smooths it
 *
 * @return String the smoothed pressure sensor data
 */
int readPressureSensor() {
    // Used for averaging data output
    const int SMOOTHING_INTERVAL = 250;
    const int SMOOTHING_TIME = 2000;
    int total = 0;
    int count = 0;
    int raw_read;
    int result;
    time_t pressure_timer = millis();

    // Average pressure every 0.1 seconds
    while (millis() - pressure_timer < SMOOTHING_TIME) {
        // pressure_timer = millis();
        raw_read = readPressureSensor_RAW();
        total += raw_read;
        count++;
        result = total / count;
    }


    // while (count < SMOOTHING_INTERVAL) {
    //     raw_read = readPressureSensor_RAW();
    //     total += raw_read;
    //     count++;
    // }

    // result = total / count;
    // count = 0;

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
    const int CM_TO_MM = 10;

    float depth = (pressure * SLOPE - OFFSET) * CM_TO_MM;

    return depth;
}

/**
 * @brief Fills the percolation hole to the specified depth by opening the valve
 *
 * @param depth depth in mm to fill the hole
 */
void fill_to_depth(float depth) {
    const float VALVE_OPEN_TIME = 1.0;  // TODO: determine this in testing

    // TODO: might have to define constants for time it takes to fill to X
    // height, could be function or constant


// Open valve
// Close when depth reaches target depth
// Wait about 3 seconds after we close the valve for water to trickle through
// Read depth
// Record over_fill_amount (mm) from water left in hose
// Add the over_fill_amount to the end goal

// Ie, if we want to fill to 300mm, and record to time till we hit zero
// valve starts to close when pressure sensor hits 300mm.
// After 3ish seconds say the pressure sensor reads depth is 320mm
// over_fill_amount = 320 - 300 = 20mm
// end goal now = 0 + 20 = 20mm

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

    Serial.println("Press button to start");
}

float depth = 0;
float depth_raw = 0;
int current_pressure = 0;
int current_raw_pressure = 0;
time_t print_timer = millis();
time_t valve_open_time;
time_t valve_close_time;

void loop() {
    current_raw_pressure = readPressureSensor_RAW();
    current_pressure = readPressureSensor();
    depth_raw = pressure_to_depth(current_raw_pressure);
    depth = pressure_to_depth(current_pressure);

    // Start button check
    if (digitalRead(START_BUTTON)) {
        button_counter++;

        // Debounce button and hold for a few seconds
        if ((button_counter > BUTTON_HOLD_THRESHOLD) && (millis() - button_timer > 300)) {
            // Reset timer
            button_timer = millis();

            // Toggle valve
            if (valve_open) {
                closeValve();
                valve_close_time = millis();

                // Print valve close time
                Serial.print("Valve close time: ");
                Serial.println(valve_close_time);

            } else {
                openValve();
                valve_open_time = millis();

                // Print valve open time
                Serial.print("Valve open time: ");
                Serial.println(valve_open_time);
            }
        }
    }
    else {
        button_counter = 0;
    }

    // Print output every 1 second
    // if (millis() - print_timer > 100) {
        print_timer = millis();
        // Serial.print("Expected Depth processed (mm): ");
        // Serial.print("Expected Depth: ");
        // Serial.println(depth);


        // Serial.print("Expected Depth raw (mm): ");
        // Serial.println(depth_raw);


        // Print pressure
        Serial.print("Pressure: ");
        Serial.println(current_pressure);

        // Print raw pressure
        Serial.print("Raw Pressure: ");
        Serial.println(current_raw_pressure);

        // if (!valve_open) {
        //     // Print time valve was open for
        //     Serial.print("Valve was open for (seconds): ");
        //     float t = (valve_close_time - valve_open_time) / ((float) 1000.00);
        //     Serial.println(t);
        // }

        // Print new line
        Serial.println();
    // }
}

// Gray jumper  (pin 9 on perf board) to IN1
// White jumper (pin 8 on perf board) to IN2

