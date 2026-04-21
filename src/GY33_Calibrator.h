/*
 * GY33_Calibrator.h - Interactive calibration helper for the GY-33 sensor.
 * * USAGE:
 * one-time setup in a calibration sketch (see calibration example):
 * 1. Include this file in your calibration sketch
 * 2. Create a calibrator: GY33_Calibrator helper(sensor);
 * 3. Run: helper.start(); and copy the generated code from the Serial Monitor into your main project
 * OR real time calibration in your main project:
 * 1. Include this file in your main project
 * 2. Create a calibrator: GY33_Calibrator helper(sensor);
 * 3. Use sensor.setCalibration(helper.start()); when you want to run the calibration (eg. in setup())
 */

#ifndef GY33_CALIBRATOR_H
#define GY33_CALIBRATOR_H

#include "GY33.h"

class GY33_Calibrator {
public:
    GY33_Calibrator(GY33_I2C &s)  : _s(&s), _i2c(&s), _uart(nullptr) {}
    GY33_Calibrator(GY33_UART &s) : _s(&s), _i2c(nullptr), _uart(&s) {}

    GY33_Calibration start() {
        GY33_Calibration cal = {0,0,0,0,0,0,1,1,1};
        
        Serial.println(F("\n--- GY-33 Calibration helper ---"));
        Serial.println(F("Ensure your color samples are matte (not shiny) and well-lit."));

        // 1. BLACK CALIBRATION
        prompt(F("Step 1: BLACK - Cover the sensor completely or place in a dark box"));
        waitForNewData();
        _s->calibrateBlack();
        Serial.println(F("Black level stored."));

        // 2. RED CROSSTALK
        prompt(F("Step 2: RED - Place a pure red sample in front of the sensor"));
        waitForNewData();
        GY33_Raw red = _s->getZeroed();
        // Calculate how much green/blue leaks into the red reading
        cal.g_leak_r = (float)red.g / red.r;
        cal.b_leak_r = (float)red.b / red.r;

        // 3. GREEN CROSSTALK
        prompt(F("Step 3: GREEN - Place a pure green sample in front of the sensor"));
        waitForNewData();
        GY33_Raw green = _s->getZeroed();
        cal.r_leak_g = (float)green.r / green.g;
        cal.b_leak_g = (float)green.b / green.g;

        // 4. BLUE CROSSTALK
        prompt(F("Step 4: BLUE - Place a pure blue sample in front of the sensor"));
        waitForNewData();
        GY33_Raw blue = _s->getZeroed();
        cal.r_leak_b = (float)blue.r / blue.b;
        cal.g_leak_b = (float)blue.g / blue.b;

        // 5. WHITE BALANCE (SCALING)
        prompt(F("Step 5: WHITE - Place a pure white sample in front of the sensor"));
        waitForNewData();
        GY33_Raw white = _s->getZeroed();
        
        // Calculate cleaned values for white to find the scaling factors
        float r_cln = (float)white.r - (white.g * cal.r_leak_g) - (white.b * cal.r_leak_b);
        float g_cln = (float)white.g - (white.r * cal.g_leak_r) - (white.b * cal.g_leak_b);
        float b_cln = (float)white.b - (white.r * cal.b_leak_r) - (white.g * cal.b_leak_g);

        // Scale everything to a 0-255 range
        cal.r_scale = 255.0f / r_cln;
        cal.g_scale = 255.0f / g_cln;
        cal.b_scale = 255.0f / b_cln;

        printResults(cal);
        return cal;
    }

private:
    GY33_Base* _s;
    GY33_I2C* _i2c;
    GY33_UART* _uart;

    void prompt(const __FlashStringHelper* msg) {
        Serial.println();
        Serial.println(msg);
        Serial.println(F("Press any key in Serial Monitor to capture..."));
        while (Serial.available()) Serial.read(); // Clear buffer
        while (!Serial.available());              // Wait for input
        delay(200); // Wait for sensor to settle after hand movement
    }

    void waitForNewData() {
        // Works for I2C (immediate) and UART (waits for packet)
        if (_i2c) {
            _i2c->queryRaw(); // Hardware specific
        } else {
            _uart->queryRaw(); // Hardware specific
            while(!_uart->update()); // Wait for serial packet
        }
    }

    void printResults(const GY33_Calibration &c) {
        Serial.println(F("\n--- Calibration Complete! ---"));
        Serial.println(F("Copy and paste this into your project setup:\n"));
        
        Serial.println(F("GY33_Calibration myCal = {"));
        Serial.print(F("  ")); Serial.print(c.r_leak_g, 4); Serial.print(F(", ")); Serial.println(c.r_leak_b, 4);
        Serial.print(F("  ")); Serial.print(c.g_leak_r, 4); Serial.print(F(", ")); Serial.println(c.g_leak_b, 4);
        Serial.print(F("  ")); Serial.print(c.b_leak_r, 4); Serial.print(F(", ")); Serial.println(c.b_leak_g, 4);
        Serial.print(F("  ")); Serial.print(c.r_scale, 4);  Serial.print(F(", ")); 
        Serial.print(c.g_scale, 4);  Serial.print(F(", ")); 
        Serial.println(c.b_scale, 4);
        Serial.println(F("};"));
        Serial.println(F("sensor.setCalibration(myCal);"));
        Serial.println();
    }
};

#endif