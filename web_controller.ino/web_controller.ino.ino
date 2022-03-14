// Importing necessary libraries
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "FS.h"
#include <SPI.h>
#include <SD.h>

// Setup motor controller
const uint8_t VALVE_ENABLE = 0;
const uint8_t SENSOR_PIN = 0;

const int VALVE_OPEN = 4;
const int VALVE_CLOSE = 13;


// SD Card setup
const char* OUTPUT_FILE = "output.csv";
static bool hasSD = false;
String csv_data = "";
File csv_file;

// Setting network credentials
const char* ssid = "percolation";
const char* password = "percolation";
const char* host = "percolation";

const char* input_parameter1 = "output";
const char* input_parameter2 = "state";


// Creating a AsyncWebServer object
AsyncWebServer server(80);



const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP8266 WEB SERVER</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    p {font-size: 3.0rem;}
    body {max-width: 600px; margin:0px auto; padding-bottom: 25px;}
    .switch {position: relative; display: inline-block; width: 120px; height: 68px}
    .switch input {display: none}
    .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 6px}
    .slider:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 3px}
    input:checked+.slider {background-color: #b30000}
    input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}
  </style>
</head>
<body>
  <h2> Raw Pressure Sensor Output:
    <span id="rawpressure">%SENSOR_OUTPUT_PLACEHOLDER%</span>
  </h2>
  <br>
  <h2>
    Processed Pressure Sensor Output: <span id=pressure> </span>
  </h2>
  <h3> Updates every 30 ms </h3>
  <br>
  %BUTTONPLACEHOLDER%
<script>function toggleCheckbox(element) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ xhr.open("GET", "/update?output="+element.id+"&state=1", true); }
  else { xhr.open("GET", "/update?output="+element.id+"&state=0", true); }
  xhr.send();
}

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("rawpressure").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/rawpressure", true);
  xhttp.send();
}, 15 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("pressure").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/pressure", true);
  xhttp.send();
}, 30 ) ;

</script>
</body>
</html>
)rawliteral";

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

void writeSD() {
    if (hasSD) {

      csv_file = SD.open(OUTPUT_FILE, FILE_WRITE);

      // Write to SD card
      csv_file.print(String(millis()));
      csv_file.print(",");
      csv_file.print(readPressureSensor_RAW());
      csv_file.print(",");
      csv_file.print(digitalRead(VALVE_OPEN));
      csv_file.print("\n");
      Serial.println("Wrote to SD card");

      csv_file.close();
    }
    else {
      Serial.println("SD card not found... attempting setup");
      setupSD();
    }
}

void openValve() {
  digitalWrite(VALVE_OPEN, HIGH);
  digitalWrite(VALVE_CLOSE, LOW);
}

void closeValve() {
  digitalWrite(VALVE_OPEN, LOW);
  digitalWrite(VALVE_CLOSE, HIGH);
}

String readPressureSensor_RAW() {
    int result = analogRead(SENSOR_PIN);
    Serial.printf("Raw read: %d\n", result);

    return String(result);
}

String readPressureSensor() {
    // Used for averaging data output
    static int PRINT_INTERVAL = 70;

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

    Serial.printf("Processed read: %d\n", result);
    return String(result);
}


// Replaces placeholder with button section in your web page
String processor(const String& var) {
  String output = "";

  if(var == "BUTTONPLACEHOLDER") {
    output += "<h4>Output - GPIO " + String(VALVE_OPEN) +"</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"" + String(VALVE_OPEN) + "\" " + outputState(VALVE_OPEN) + "><span class=\"slider\"></span></label>";
  }
  else if (var == "SENSOR_OUTPUT_PLACEHOLDER") {
    output += readPressureSensor_RAW();
  }
  return output;
}

String outputState(int output){
  if(digitalRead(output)){
    return "checked";
  }
  else {
    return "";
  }
}

void setup(){
  // Serial port for debugging purposes
  Serial.begin(9600);

  Serial.println("STARTING UP (wait 7 seconds)...");

  // Sleep 7 seconds
  delay(7000);

  Serial.println("STARTING UP");

  // Setup IO pins
  pinMode(VALVE_ENABLE, OUTPUT);
  pinMode(VALVE_OPEN, OUTPUT);
  pinMode(VALVE_CLOSE, OUTPUT);
  pinMode(SENSOR_PIN, INPUT);


  // Setup SD Output
  setupSD();


  //Host AP
  Serial.println();
  Serial.println("Configuring access point...");
  /* You can remove the password parameter if you want the AP to be open. */
  WiFi.softAP(ssid);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);

    writeSD();
  });

  server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request){

    if (!hasSD) {
      setupSD();
    }

    String output_contents = csv_file.readString();
    Serial.println(csv_file.readString());
    Serial.println(csv_file.read());
    request->send(200, "text/html", output_contents);
  });

  server.on("/rawpressure", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", readPressureSensor_RAW());
  });

  server.on("/pressure", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", readPressureSensor());
  });

  server.on("/graph", HTTP_GET, [](AsyncWebServerRequest *request){
    File indexFile;
    String result = "Pending SD Card...";

    if (hasSD) {
      indexFile = SD.open("/graph.htm");
      result = indexFile.readString();
      indexFile.close();
    }
    else {
      setupSD();
    }

    request->send(200, "text/html", result);
  });


  // server.on("/highcharts.js", HTTP_GET, [](AsyncWebServerRequest *request){
  //   request->send(SD, "/highcharts.js", "text/html");
  // });

  // server.serveStatic("/", SD, "/");


  server.on("/highcharts.js", HTTP_GET, [](AsyncWebServerRequest *request){
    File indexFile;
    String result = "";

    if (hasSD) {
      indexFile = SD.open("/highcharts.js");

      while (indexFile.available()) {
        // result += indexFile.read();
        Serial.write(indexFile.read());
      }
      indexFile.close();

      indexFile = SD.open("/highcharts.js");

      String strResult = indexFile.readString();
      Serial.println(strResult);

      request->send(200, "text/html", strResult);
      indexFile.close();
    }
    else {
      setupSD();
      request->send(500, "text/html", "SD CARD NOT AVAILABLE: TRY AGAIN");
    }
  });


  // Send a GET request to <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage1;
    String inputMessage2;
    // GET input1 value on <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
    if (request->hasParam(input_parameter1) && request->hasParam(input_parameter2)) {
      inputMessage1 = request->getParam(input_parameter1)->value();
      inputMessage2 = request->getParam(input_parameter2)->value();
      // digitalWrite(inputMessage1.toInt(), inputMessage2.toInt());

      // Control valve
      if (inputMessage1.equals(String(VALVE_OPEN))) {
        if (inputMessage2.equals("1")) {
          openValve();
        }
        else if (inputMessage2.equals("0")) {
          closeValve();
        }
      }

    }
    else {
      inputMessage1 = "No message sent";
      inputMessage2 = "No message sent";
    }
    Serial.print("GPIO: ");
    Serial.print(inputMessage1);
    Serial.print(" - Set to: ");
    Serial.println(inputMessage2);
    writeSD();
    request->send(200, "text/plain", "OK");
  });

  // Start server
  server.begin();
}

void loop() {

  // if (hasSD) {
  //   // Write to SD card
  //   csv_file.print(String(millis()));
  //   csv_file.print(",");
  //   csv_file.print(readPressureSensor_RAW());
  //   csv_file.print(",");
  //   csv_file.print(digitalRead(VALVE_OPEN));
  //   csv_file.print("\n");
  //   Serial.println("Wrote to SD card");
  // }
  // else {
  //   Serial.println("SD card not found");
  // }
}
