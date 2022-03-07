    /***********************************************************
      DFRobot Gravity: Analog Current to Voltage Converter(For 4~20mA Application)
      SKU:SEN0262

      GNU Lesser General Public License.
      See <http://www.gnu.org/licenses/> for details.
      All above must be included in any redistribution
     ****************************************************/

    #define ANALOG_PIN A0
    #define RANGE 5000 // Depth measuring range 5000mm (for water)
    
    #define VREF 2700 // ADC's reference voltage on your Arduino,typical value:5000mV
    
    #define CURRENT_INIT 4.00 // Current @ 0mm (uint: mA)
    #define DENSITY_WATER 1  // Pure water density normalized to 1
    #define DENSITY_GASOLINE 0.74  // Gasoline density
    #define PRINT_INTERVAL 70

    int16_t dataVoltage;
    int16_t rawAO;
    int16_t average = 0;
    int16_t total = 0;
    float dataCurrent, depth; //unit:mA
    unsigned long timepoint_measure;
    int i = 0;
    int count = 0;

    void setup()
    {
      Serial.begin(9600);
      pinMode(ANALOG_PIN, INPUT);
      timepoint_measure = millis();
    }

    void loop()
    {

      total += analogRead(ANALOG_PIN);
      i++;
      count++;
      
      //if (millis() - timepoint_measure > PRINT_INTERVAL) {
      if (count > PRINT_INTERVAL) {
        count = 0;

        dataVoltage = total / i;
        i = 0;
        total = 0;
        
        //dataVoltage = analogRead(ANALOG_PIN);
        Serial.print("Raw A0: ");
        Serial.println(dataVoltage);
        //Used to check if 168 or less
        rawAO = dataVoltage;
        dataVoltage = dataVoltage / 1024.0 * VREF;

        Serial.print("Data Voltage: ");
        Serial.println(dataVoltage);
        
        
        //dataCurrent = dataVoltage / 120.0; //Sense Resistor:120ohm
        //depth = (dataCurrent - CURRENT_INIT) * (RANGE/ DENSITY_WATER / 16.0); //Calculate depth from current readings

        //Slope equation for pressure sensor. X10 to convert to millimeters
        depth = (dataVoltage * 1.258 + 161.818)* 10;

        // rawAO of 168 and lower indicates that depth is 0
        if (depth < 0 || rawAO <= 168) 
        {
          depth = 0.0;
        }

        //Serial print results
        Serial.print("depth:");
        Serial.println(depth);
        Serial.println("mm");

        
      }
    }
