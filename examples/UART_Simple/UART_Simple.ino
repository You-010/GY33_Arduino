/*
 * GY-33 UART Simple Test
 * ----------------------
 * Hardware: Ensure S0 jumper is Open (or High) for UART mode
 * S1 should be left open
 * Ensure TX/RX pins match the Serial object used 
 */

#include <GY33.h>

// On ESP32, we use Serial1. On Uno, you might use SoftwareSerial.
// We pass the Serial object to the constructor.
GY33_UART sensor(Serial1);

void setup() {
  Serial.begin(115200);
  
  // Initialize the Serial port connected to the sensor
  // Adjust pins for your specific board!
  Serial1.begin(9600, SERIAL_8N1, 8, 9); 
  
  sensor.begin();
  
  // Tell the sensor to send Lcc and Processed data automatically
  sensor.setOutput(GY33_UART::OUTPUT_LCC| GY33_UART::OUTPUT_PROCESSED);

  Serial.println(F("GY-33 UART Test Started"));
  Serial.println(F("Note: S0 must be Open/High for UART."));
}

void loop() {
  if (sensor.update()) {
    GY33_Processed rgb = sensor.getProcessed();

    Serial.print(F("Colour: "));
    Serial.print(sensor.colour());
    Serial.print(F(" | R: ")); Serial.print(rgb.r);
    Serial.print(F(" G: ")); Serial.print(rgb.g);
    Serial.print(F(" B: ")); Serial.println(rgb.b);
  }
  else { Serial.println(F("No data from sensor. Check wiring/S0 pin.")); }

  delay(500);
}
