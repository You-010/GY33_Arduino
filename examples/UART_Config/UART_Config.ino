/*
 * GY-33 UART COnfig example
 * ----------------------
 * Hardware: Ensure S0 jumper is Open (or High) for UART mode
 * S1 should be left open
 * Ensure TX/RX pins match the Serial object used 
 * 
 * * Commands (Case-Insensitive):
 * H: Display Help text
 * L: Set LED (0-10)
 * I: Set Integration Time (0:700ms, 1:154ms, 2:100ms, 3:24ms, 4:2.4ms)
 * Q: Query I2C Address
 * S: Set I2C Address (Enter 7-bit decimal)
 * R: Reset to Factory Defaults
 */

#include <GY33.h>

// On ESP32, we use Serial1. On Uno, you might use SoftwareSerial.
// We pass the Serial object to the constructor.
GY33_UART sensor(Serial1);
void helptext();

void setup() {
  Serial.begin(115200);
  
  // Initialize the Serial port connected to the sensor
  // Adjust pins for your specific board!
  Serial1.begin(9600, SERIAL_8N1, 8, 9); 
  
  sensor.begin();
  
  // turn off automatic data output
  sensor.setOutput(GY33_UART::OUTPUT_OFF);

  Serial.println(F("GY-33 UART Config Test Started"));
  Serial.println(F("Note: S0 must be Open/High for UART."));
  helptext();
}

void loop() {
  if (Serial.available() > 0) {
    char cmd = toupper(Serial.read());
    while(Serial.available() > 0) { Serial.read(); } // Clear buffer

    sensor.queryRaw(); //ping sensor to test it's connected
    unsigned long pingStart = millis();
    bool alive = false;
    while (millis() - pingStart < 300) { 
      if (sensor.update()) { alive = true; break; }
    }
    if (!alive) {
      Serial.println(F("ERROR: Sensor not responding. Check wiring and S0 jumper."));
      return;
    }
    
    switch (cmd) {
      case 'L': {
        Serial.println(F("Enter LED brightness (0-10):"));
        while(!Serial.available());
        int val = Serial.parseInt();
        sensor.setLED(val, true); // true saves to flash
        Serial.print(F("LED set to: ")); Serial.println(val);
        break;
      }
      
      case 'I': {
        Serial.println(F("Enter Int. Time (0:700, 1:154, 2:100, 3:24, 4:2.4):"));
        while(!Serial.available());
        int opt = Serial.parseInt();
        uint8_t times[] = {GY33_UART::ITIME_700, GY33_UART::ITIME_154, 
                           GY33_UART::ITIME_100, GY33_UART::ITIME_24, 
                           GY33_UART::ITIME_2_4};
        if(opt >= 0 && opt <= 4) {
           sensor.setIntegrationTime(times[opt]);
           Serial.println(F("Integration time updated."));
        }
        break;
      }

      case 'Q': {
        Serial.println(F("Querying I2C Address..."));
        sensor.queryI2CAddress();
        unsigned long start = millis();
        while (millis() - start < 500) {
          if (sensor.update()) {
            Serial.print(F("Current I2C Address: 0x"));
            Serial.println(sensor.getI2CAddress(), HEX);
            break;
          }
        }
        break;
      }

      case 'S': {
        Serial.println(F("Enter new 7-bit I2C address (decimal):"));
        while(!Serial.available());
        int addr = Serial.parseInt();
        sensor.setI2CAddress(addr);
        Serial.print(F("Address set to: 0x")); Serial.println(addr, HEX);
        Serial.println(F("Use 'Q' command to verify"));
        break;
      }

      case 'R': {
        Serial.println(F("Resetting to defaults..."));
        sensor.setBaudRate(GY33_UART::BAUD_9600, true);
        sensor.setLED(10, true);
        sensor.setIntegrationTime(GY33_UART::ITIME_100);
        sensor.setI2CAddress(0x5A);
        Serial.println(F("Done."));
        break;
      }

      case 'H':
      default: { helptext(); }
    }
    while(Serial.available() > 0) { Serial.read(); } //flush leftovers eg. newline
  }
}


void helptext() {
  Serial.println(F("\n--- GY-33 UART Config Menu ---"));
  Serial.println(F("H: display Help text"));
  Serial.println(F("L: Set LED (0-10)"));
  Serial.println(F("I: Set Integration Time (0:700ms, 1:154ms, 2:100ms, 3:24ms, 4:2.4ms)"));
  Serial.println(F("Q: Query I2C Address"));
  Serial.println(F("S: Set I2C Address (Enter 7-bit decimal)"));
  Serial.println(F("R: Reset to Factory Defaults"));
  Serial.println(F("------------------------------"));
}
