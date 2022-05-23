# Automated Soil Percolation Tester
Automated soil percolation tester prototype for EMI Uganda using an ESP32. 
Work in progress.

## Project Structure

`auto_perc_tester` contains the project's source code

`libraries` contains the non-standard Arduino libraries used for easy access

`testPrograms` contains a number of simple programs used to help verify proper electrical setup and that the hardware components are correctly functioning

`sd_card_contents` contains the entire contents of the SD card including libraries, html files, and the test log.

## Setup

To compile this code you will need to include all the libraries found in `libraries`. You will also need the ESP32 board manager included in your Arduino IDE.

Then copy the contents of `sd_card_contents` to the SD card on the board.

## Usage:
1) Connect to the Wireless network `Percolation`
2) In your browser, navigate to `www.192.168.4.1` for the dashboard to see the live data output and toggle the valve. Hit the blue `refresh` button to get current results. Upon starting a test, hit the `refresh` button again and you should an updated test phase displayed.
3) Navigate to `www.192.168.4.1/graph` to see a live graph of the pressure and depth data
4) Navigate to `www.192.168.4.1/download` to see the contents of the test log file. This contains the time and pressure readings by default.

## Calibration

Our code was tested in a lab in Oregon, never on the field. You may need to recalibrate a few things before you can get accurate results.

Here are a few things you may need to calibrate:

### The Local Time

To set your time, checkout [this guide](https://www.circuitbasics.com/how-to-use-a-real-time-clock-module-with-the-arduino/) on how to use the RTC (Real time clock) module. This is not critical to run a test, but is very helpful when trying to read the log file to track when each test occurred.

### Pressure to depth conversion

We found that the signal from the pressure sensor can be noisy. We did some initial configuration, but you may need to change a few things to refine your results.

In order to handle the noise from the pressure sensor, we average the raw analog reading over 200 milliseconds. You can change that in the `readPressureSensor()` function.

Through a series of lab tests with a meter stick, we came up with a multiplier and an offset to convert the pressure sensor data to a depth in mm. If your depth readings are consistently off, modify the constants in the `pressure_to_depth()` function.

### Toggling the valve

The ball valve doesn't open and close instantaneously. The three main approaches to this issue are:
1) Ignore it and hope the amount of water that comes out in the close time is insignificant.
2) Don't start the test until the water level drains to the expected initial level for the according test phase.
3) Start the test immediately and end it at the target level plus the overfill amount.

We went with approach number three. To improve the accuracy of your results, update the amount of time that it takes your valve to toggle entirely from one state to the other in the `fill_to_depth()` function.

## Known Issues
* Noise in the data coming from the pressure sensor can cause the test to jump to the next phase too early. More testing is required in order to determine acceptable thresholds to ignore spikes of noise. These thresholds can be updated in the `readPressureSensor()` function
* In our lab tests, the program completes almost instantly after finishing the first drain cycle because we had a constant water level. Each of the test phases should be tested independently in an environment where water can drain.

## Future goals (not in any particular order)
* Create a new CSV file for each test for easy organization of results.
* Create an additional log that uses the time
* Create a captive portal to automatically open the web interface in the browser of any device that connects to the network without having to manually go to the IP address of the server.
* Add more controls in the web interface to allow easy jumping between test phases.
* Create an easy way to update the timezone
* Create an easy way to upload new files to and update existing files on the SD card without having to remove it every time.

## Additional Tests

If you are having a hard time getting the SD card to be detected by the ESP32, try starting with the test code in `testPrograms/esp32SDTest`

To ensure the ball motor valve is functioning properly, use `testPrograms/ballmotortest`

To verify the LED's are all functional, use `testPrograms/led_tester`

If you need to recalibrate your valve toggle times, use `testPrograms/valveTimeTester` to get an idea of how long it takes for the valve to open and close.

## SD Card Contents

All the `.js` files on the SD card are JavaScript libraries necessary for the graph and export feature to work.

The default webpage is `index.htm` and the graphing page is `graph.htm`.

### *Important note about the file server*

The ESP webserver is designed to only support file extensions no longer than 3 characters. For this reason all the html file use the `.htm` extension. The web server is setup to serve all files on the SD card, so before running ensure you don't have extraneous files with long extensions on your SD card or unexpected results may occur.


## Virtual Testing

Interested in testing code without having to deal with the electrical configuration?

We discovered this great web-based Arduino emulator called *Wokwi*. You will need an account to have access to all the features (particularly with use of the SD card), but even without registering it can still be helpful in debugging your code.

You may notice the code running on the emulator is not totally in sync with this git repository. Checkout the branch called `virtual` for a version of the project code made to run on the Wokwi platform.

Checkout the project link [here](https://wokwi.com/projects/330306268263613010)

And the Wokwi documentation [here](https://docs.wokwi.com/)
