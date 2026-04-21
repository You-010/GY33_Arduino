/*
 * GY-33 Example Calibration Sketch
 * ----------------------------------
 * 1. Set GY33_PROTOCOL below (1 for UART, 0 for I2C).
 * 2. Ensure S0 jumper is set properly (open/high for UART, closed/gnd for I2C).
 * 3. Verify hardware settings: Ensure the Serial/I2C pins in the configuration section match your physical wiring.
 * 4. Upload sketch and open Serial Monitor (115200 baud).
 * 5. Follow the step-by-step prompts to generate your calibration data.
 */

#include <Arduino.h>
#include <GY33.h>

// --- CONFIGURATION ---
// Set to 1 for UART mode, 0 for I2C mode
#define GY33_PROTOCOL 0 

#if GY33_PROTOCOL == 1
    // On ESP32, we use Serial1. On Uno, you might use SoftwareSerial.
    // We pass the Serial object to the constructor.
    GY33_UART sensor(Serial1); 
#else
    // Create the sensor object
    // Default address is 0x5A. If you changed it, use: GY33_I2C sensor(address);
    GY33_I2C sensor;
#endif

#include <GY33_Calibrator.h>
GY33_Calibrator helper(sensor);
int samples = 0;
uint32_t lastSampleTime = 0;

void runCalibration() {
    // check connection first
    if (sensor.update()) { 
        sensor.setCalibration(helper.start()); 
        samples = 5; lastSampleTime = millis(); 
    } else { Serial.println(F("\n[!] ERROR: Sensor not responding."));
        Serial.println(F("Please check wiring, power and I2C address.")); 
    }
    while(Serial.available()) Serial.read(); // Flush buffer
    Serial.println(F(">>> Send any character to RESTART calibration."));
}

void setup() {
    Serial.begin(115200);
    while (!Serial); 

    #if GY33_PROTOCOL == 1
        // UART Setup
        // Adjust pins for your specific board!
        Serial1.begin(9600, SERIAL_8N1, 8, 9); 
        sensor.begin();
        Serial.println(F("GY-33 UART Calibration Started"));
    #else
        // I2C Setup
        // For ESP32: you can define the sda and scl pins and use sensor.begin(SDA,SCL); 
        //            sensor.begin() will use hardware defaults
        // For Uno/Nano: sensor.begin();
        sensor.begin();
        Serial.println(F("GY-33 I2C Calibration Started"));
    #endif

    runCalibration();
}

void loop() {
    if (Serial.available()) {
        while(Serial.available()) Serial.read(); // Flush buffer
        runCalibration();
    }

    if (samples && (millis() - lastSampleTime >= 1000)) {
        if (sensor.update()) {
            GY33_Raw cal = sensor.getCalibrated();
            Serial.print(F("Sample ")); Serial.print(6 - samples);
            Serial.print(F("/5 -> R: ")); Serial.print(cal.r);
            Serial.print(F(" G: ")); Serial.print(cal.g);
            Serial.print(F(" B: ")); Serial.println(cal.b);
            
            lastSampleTime = millis();
            samples--; 
        }
    }
}
