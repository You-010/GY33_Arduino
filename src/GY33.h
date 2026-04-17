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

namespace GY33 {
    static const char* getColourName(uint8_t index) {
        static const char* names[] = {
            "Unknown", "Red", "Orange", "Yellow", 
            "Green", "Cyan", "Blue", "Purple"
        };
        return (index > 7) ? names[0] : names[index];
    }
}

class GY33_UART {
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
    bool update(); 

    GY33_Raw getRaw() { return _raw; }
    GY33_LCC getLCC() { return _lcc; }
    GY33_Processed getProcessed() { return _processed; }
    const char* colour() { return GY33::getColourName(_lcc.colourIndex); }

    void setOutput(uint8_t flags);
    void setLED(uint8_t power, bool save = false);
    void setBaudRate(uint8_t baud, bool save = false);
    void setIntegrationTime(uint8_t itime);
    void saveToFlash();
    void calibrateWhiteBalance();

    //Asynchronous
    void queryRaw();
    void queryLCC();
    void queryProcessed();

    uint8_t getI2CAddress() { return _i2cAddr >> 1; }
    void queryI2CAddress();
    void setI2CAddress(uint8_t addr7bit);

    // TODO: Software calibration (raw value correction) not yet implemented
    // void calibrateWhite();
    // void calibrateBlack();
    // GY33_Raw getCalibrated();

private:
    Stream *_serial;
    void sendCommand(uint8_t header, uint8_t cmd);
    
    uint8_t _headerCount = 0;
    uint8_t _frameType = 0;
    int16_t _dataLen = -1;
    uint8_t _recvBuf[12];
    uint8_t _recvPtr = 0;

    GY33_Raw _raw = {0,0,0,0};
    GY33_LCC _lcc = {0,0,0};
    GY33_Processed _processed = {0,0,0};
    uint8_t _i2cAddr = 0;

    uint8_t _savedMode = 0x81;

    void parseFrame();
    bool checkSum(uint8_t incomingChecksum);
};

class GY33_I2C {
public:
    GY33_I2C(uint8_t addr = 0x5A);
    void begin(int sda = -1, int scl = -1);
    bool update();

    GY33_Raw getRaw() { return _raw; }
    GY33_LCC getLCC() { return _lcc; }
    GY33_Processed getProcessed() { return _processed; }
    const char* colour() { return GY33::getColourName(_lcc.colourIndex); }

    //Synchronous
    GY33_Raw queryRaw();
    GY33_LCC queryLCC();
    GY33_Processed queryProcessed();

    void setLED(uint8_t power);
    void calibrateWhiteBalance();

    // TODO: Software calibration (raw value correction) not yet implemented
    // void calibrateWhite();
    // void calibrateBlack();
    // GY33_Raw getCalibrated();

private:
    uint8_t _addr;
    GY33_Raw _raw={0,0,0,0};
    GY33_LCC _lcc={0,0,0};
    GY33_Processed _processed={0,0,0};
    void writeData(uint8_t reg, uint8_t val);
    uint8_t readData(uint8_t reg);
    bool readData(uint8_t reg, uint8_t len, uint8_t* buffer);
};

#endif
