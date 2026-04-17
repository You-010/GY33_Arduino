#include <GY33.h>

// Create the sensor object (Default address is 0x5A)
GY33_I2C sensor;

void setup() {
  Serial.begin(115200);
  
  // For ESP32: you can define the scl and sda pins and use sensor.begin(SCL, SDA); 
  //            sensor.begin() will use hardware defaults
  // For Uno/Nano: sensor.begin();
  sensor.begin();

  Serial.println("GY-33 I2C Test Started");
}

void loop() {
  if (sensor.update()) {
    GY33_Processed rgb = sensor.getProcessed();

    Serial.print("Colour: ");
    Serial.print(sensor.colour());
    Serial.print(" | R: "); Serial.print(rgb.r);
    Serial.print(" G: "); Serial.print(rgb.g);
    Serial.print(" B: "); Serial.println(rgb.b);
  }
  
  delay(500);
}
