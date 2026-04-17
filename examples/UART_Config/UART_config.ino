#include <GY33.h>

// On ESP32, we use Serial1. On Uno, you might use SoftwareSerial.
// We pass the Serial object to the constructor.
GY33_UART sensor(Serial1);
void helptext();

void setup() {
  Serial.begin(115200);
  
  // Initialize the Serial port connected to the sensor
  // Adjust pins for your specific board!
  Serial1.begin(9600, SERIAL_8N1, 16, 17); 
  
  sensor.begin();
  
  // turn off automatic data output
  sensor.setOutput(GY33_UART::OUTPUT_OFF);

  Serial.println("GY-33 UART Config Test Started");
  helptext();
}

void loop() {
  if (Serial.available() > 0) {
    char cmd = toupper(Serial.read());
    
    switch (cmd) {
      case 'L': {
        Serial.println("Enter LED brightness (0-10):");
        while(!Serial.available());
        int val = Serial.parseInt();
        sensor.setLED(val, true); // true saves to flash
        Serial.print("LED set to: "); Serial.println(val);
        break;
      }
      
      case 'I': {
        Serial.println("Enter Int. Time (0:700, 1:154, 2:100, 3:24, 4:2.4):");
        while(!Serial.available());
        int opt = Serial.parseInt();
        uint8_t times[] = {GY33_UART::ITIME_700, GY33_UART::ITIME_154, 
                           GY33_UART::ITIME_100, GY33_UART::ITIME_24, 
                           GY33_UART::ITIME_2_4};
        if(opt >= 0 && opt <= 4) {
           sensor.setIntegrationTime(times[opt]);
           Serial.println("Integration time updated.");
        }
        break;
      }

      case 'Q': {
        Serial.println("Querying I2C Address...");
        sensor.queryI2CAddress();
        unsigned long start = millis();
        while (millis() - start < 500) {
          if (sensor.update()) {
            Serial.print("Current I2C Address: 0x");
            Serial.println(sensor.getI2CAddress(), HEX);
            break;
          }
        }
        break;
      }

      case 'S': {
        Serial.println("Enter new 7-bit I2C address (decimal):");
        while(!Serial.available());
        int addr = Serial.parseInt();
        sensor.setI2CAddress(addr);
        Serial.print("Address set to: 0x"); Serial.println(addr, HEX);
        Serial.println("Use 'Q' command to verify");
        break;
      }

      case 'R': {
        Serial.println("Resetting to defaults...");
        sensor.setBaudRate(GY33_UART::BAUD_9600, true);
        sensor.setLED(10, true);
        sensor.setIntegrationTime(GY33_UART::ITIME_100);
        sensor.setI2CAddress(0x5A);
        Serial.println("Done.");
        break;
      }

      case 'H':
      default: { helptext(); }
    }
    while(Serial.available() > 0) { Serial.read(); } //flush leftovers eg. newline
  }
}


void helptext() {
  Serial.println("\n--- GY-33 UART Config Menu ---");
  Serial.println("H: display Help text");
  Serial.println("L: Set LED (0-10)");
  Serial.println("I: Set Integration Time (0:700ms, 1:154ms, 2:100ms, 3:24ms, 4:2.4ms)");
  Serial.println("Q: Query I2C Address");
  Serial.println("S: Set I2C Address (Enter 7-bit decimal)");
  Serial.println("R: Reset to Factory Defaults");
  Serial.println("------------------------------");
}
