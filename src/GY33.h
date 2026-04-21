/*
 * GY33_UART/I2C - Arduino library for the GY-33 Colour Sensor
 * Ported to Arduino by me
 * Original MicroPython implementation by QuirkyCort:
 * https://github.com/QuirkyCort/micropython-gy33
*/

#ifndef GY33_H
#define GY33_H

#include <Arduino.h>

struct GY33_Raw { uint16_t r, g, b, c; };
struct GY33_LCC { uint16_t lux, colourTemp, colourIndex; };
struct GY33_Processed { uint8_t r, g, b; };
struct GY33_Calibration {
    float r_leak_g, r_leak_b;
    float g_leak_r, g_leak_b;
    float b_leak_r, b_leak_g;
    float r_scale, g_scale, b_scale;
};

class GY33_Base {
public:
    virtual bool update() = 0;

    GY33_Raw getRaw() { return _raw; }
    GY33_LCC getLCC() { return _lcc; }
    GY33_Processed getProcessed() { return _processed; }
    
    const char* colour() {
        static const char* names[] = {
            "Unknown", "Red", "Orange", "Yellow", 
            "Green", "Cyan", "Blue", "Purple"
        };
        return (_lcc.colourIndex > 7) ? names[0] : names[_lcc.colourIndex];
    }

    virtual void setLED(uint8_t power, bool save = false) = 0;
    virtual void calibrateWhiteBalance() = 0;

    // Software calibration (raw value correction)
    void calibrateBlack() {
        _blackOffset = _raw;
    }

    GY33_Raw getZeroed() {
        return {
            (uint16_t)(_raw.r > _blackOffset.r ? _raw.r - _blackOffset.r : 0),
            (uint16_t)(_raw.g > _blackOffset.g ? _raw.g - _blackOffset.g : 0),
            (uint16_t)(_raw.b > _blackOffset.b ? _raw.b - _blackOffset.b : 0),
            (uint16_t)(_raw.c > _blackOffset.c ? _raw.c - _blackOffset.c : 0)
        };
    }

    void setCalibration(const GY33_Calibration &c) { _cal = c; }

    GY33_Raw getCalibrated() {
        GY33_Raw z = getZeroed();
        
        // Crosstalk Subtraction
        float r_cln = (float)z.r - (z.g * _cal.r_leak_g) - (z.b * _cal.r_leak_b);
        float g_cln = (float)z.g - (z.r * _cal.g_leak_r) - (z.b * _cal.g_leak_b);
        float b_cln = (float)z.b - (z.r * _cal.b_leak_r) - (z.g * _cal.b_leak_g);

        // Apply scale
        return {
            (uint16_t)max(0.0f, r_cln * _cal.r_scale),
            (uint16_t)max(0.0f, g_cln * _cal.g_scale),
            (uint16_t)max(0.0f, b_cln * _cal.b_scale),
            z.c 
        };
    }

protected:
    GY33_Raw _raw = {0,0,0,0};
    GY33_LCC _lcc = {0,0,0};
    GY33_Processed _processed = {0,0,0};

    // Calibration values
    GY33_Raw _blackOffset = {0,0,0,0};
    GY33_Calibration _cal = {0,0,0,0,0,0,1,1,1};
};

class GY33_UART : public GY33_Base {
public:
    enum OutputControl {
        OUTPUT_OFF       = 0x00,
        OUTPUT_PROCESSED = 0x01,
        OUTPUT_LCC       = 0x02,
        OUTPUT_RAW       = 0x04,
        OUTPUT_PAUSE     = 0x10,
        OUTPUT_RESUME    = 0x20
    };

    enum Baud {
        BAUD_9600       = 0xAE,
        BAUD_115200     = 0xAF
    };

    enum IntegrationTime {
        ITIME_700       = 0x58,
        ITIME_154       = 0x59,
        ITIME_100       = 0x5A,
        ITIME_24        = 0x5B,
        ITIME_2_4       = 0x5C
    };

    GY33_UART(Stream &serial);
    void begin();
    bool update() override; 

    void setOutput(uint8_t flags);
    void setLED(uint8_t power, bool save = false) override;
    void setBaudRate(uint8_t baud, bool save = false);
    void setIntegrationTime(uint8_t itime);
    void saveToFlash();
    void calibrateWhiteBalance() override;

    //Asynchronous
    void queryRaw();
    void queryLCC();
    void queryProcessed();

    void queryZeroed() { queryRaw(); }
    void queryCalibrated() { queryRaw(); }

    uint8_t getI2CAddress() { return _i2cAddr >> 1; }
    void queryI2CAddress();
    void setI2CAddress(uint8_t addr7bit);

private:
    Stream *_serial;
    void sendCommand(uint8_t header, uint8_t cmd);
    
    uint8_t _headerCount = 0;
    uint8_t _frameType = 0;
    int16_t _dataLen = -1;
    uint8_t _recvBuf[12];
    uint8_t _recvPtr = 0;

    uint8_t _i2cAddr = 0;

    uint8_t _savedMode = 0x81;

    void parseFrame();
    bool checkSum(uint8_t incomingChecksum);
};

class GY33_I2C : public GY33_Base {
public:
    GY33_I2C(uint8_t addr = 0x5A);
    void begin(int sda = -1, int scl = -1);
    bool update() override;

    //Synchronous
    GY33_Raw queryRaw();
    GY33_LCC queryLCC();
    GY33_Processed queryProcessed();

    GY33_Raw queryZeroed() { queryRaw(); return getZeroed(); }
    GY33_Raw queryCalibrated() { queryRaw(); return getCalibrated(); }

    void setLED(uint8_t power, bool save = false) override;
    void calibrateWhiteBalance() override;

private:
    uint8_t _addr;
    void writeData(uint8_t reg, uint8_t val);
    uint8_t readData(uint8_t reg);
    bool readData(uint8_t reg, uint8_t len, uint8_t* buffer);
};

#endif