#include <GY33.h>

// On ESP32, we use Serial1. On Uno, you might use SoftwareSerial.
// We pass the Serial object to the constructor.
GY33_UART sensor(Serial1);

void setup() {
  Serial.begin(115200);
  
  // Initialize the Serial port connected to the sensor
  // Adjust pins for your specific board!
  Serial1.begin(9600, SERIAL_8N1, 16, 17); 
  
  sensor.begin();
  
  // Tell the sensor to send Lcc and Processed data automatically
  sensor.setOutput(GY33_UART::OUTPUT_LCC| GY33_UART::OUTPUT_PROCESSED);

  Serial.println("GY-33 UART Test Started");
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
