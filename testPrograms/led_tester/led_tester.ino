const int LED_TEST_STATUS = 12;
const int LED_ERROR_STATUS = 13;
const int LED_VALVE_STATUS = 14;
const int START_BUTTON = 22;

void setup() {

    Serial.begin(9600);
  // put your setup code here, to run once:
//     pinMode(START_BUTTON, INPUT_PULLUP);
          pinMode(START_BUTTON, INPUT);
    pinMode(LED_TEST_STATUS, OUTPUT);
    pinMode(LED_ERROR_STATUS, OUTPUT);
    pinMode(LED_VALVE_STATUS, OUTPUT);
}

void loop() {
      if (digitalRead(START_BUTTON)) {
        Serial.println("START_BUTTON pressed");
        digitalWrite(LED_TEST_STATUS, LOW);
        digitalWrite(LED_ERROR_STATUS, LOW);
        digitalWrite(LED_VALVE_STATUS, LOW);
    }
    else {
       Serial.println("not pressed");

        digitalWrite(LED_TEST_STATUS, HIGH);
        digitalWrite(LED_ERROR_STATUS, HIGH);
        digitalWrite(LED_VALVE_STATUS, HIGH);
    }
}
