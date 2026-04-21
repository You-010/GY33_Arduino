/*
 * GY-33 I2C Simple Test
 * ---------------------
 * Hardware: Ensure S0 jumper is Closed (or Grounded) for I2C mode
 * S1 should be left open
 * Ensure SDA/SCL wiring matches the pins used
 */

#include <GY33.h>

// Create the sensor object
// Default address is 0x5A. If you changed it, use: GY33_I2C sensor(address);
GY33_I2C sensor;

void setup() {
  Serial.begin(115200);
  
  // For ESP32: you can define the sda and scl pins and use sensor.begin(SDA,SCL); 
  //            sensor.begin() will use hardware defaults
  // For Uno/Nano: sensor.begin();
  sensor.begin();

  Serial.println(F("GY-33 I2C Test Started"));
  Serial.println(F("Note: S0 must be Closed/Grounded for I2C."));
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
  else { Serial.println(F("No data from sensor. Check wiring/S0 pin and I2C address.")); }

  delay(500);
}
