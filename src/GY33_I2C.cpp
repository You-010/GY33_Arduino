#include "GY33.h"
#include <Wire.h>

GY33_I2C::GY33_I2C(uint8_t addr) : _addr(addr) {}

void GY33_I2C::begin(int sda, int scl) {
    #if defined(ESP32) || defined(ESP8266)
        if (sda >= 0 && scl >= 0) {
            Wire.begin(sda, scl);
        } else {
            Wire.begin(); //default
        }
    #else
        Wire.begin(); 
    #endif
}

void GY33_I2C::writeData(uint8_t r, uint8_t v) {
    Wire.beginTransmission(_addr); 
    Wire.write(r); 
    Wire.write(v); 
    Wire.endTransmission();
}

uint8_t GY33_I2C::readData(uint8_t r) {
    Wire.beginTransmission(_addr); 
    Wire.write(r); 
    Wire.endTransmission(false);
    Wire.requestFrom(_addr, (uint8_t)1); 
    return Wire.available() ? Wire.read() : 0;
}

bool GY33_I2C::readData(uint8_t reg, uint8_t len, uint8_t* buffer) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    
    Wire.requestFrom(_addr, len);
    if (Wire.available() == len) {
        for (uint8_t i = 0; i < len; i++) buffer[i] = Wire.read();
        return true;
    }
    return false;
}

bool GY33_I2C::update() {
    uint8_t buf[16];
    if (!readData(0x00, 16, buf)) return false;

    _raw.r = (buf[0] << 8) | buf[1];
    _raw.g = (buf[2] << 8) | buf[3];
    _raw.b = (buf[4] << 8) | buf[5];
    _raw.c = (buf[6] << 8) | buf[7];
    
    _lcc.lux       = (buf[8] << 8) | buf[9];
    _lcc.colourTemp = (buf[10] << 8) | buf[11];
    
    _processed.r   = buf[12];
    _processed.g   = buf[13];
    _processed.b   = buf[14];
    _lcc.colourIndex = buf[15];

    return true;
}

GY33_Raw GY33_I2C::queryRaw() {
    uint8_t buf[8];
    if (readData(0x00, 8, buf)) {
        _raw.r = (buf[0] << 8) | buf[1];
        _raw.g = (buf[2] << 8) | buf[3];
        _raw.b = (buf[4] << 8) | buf[5];
        _raw.c = (buf[6] << 8) | buf[7];
    }
    return _raw;
}

GY33_LCC GY33_I2C::queryLCC() {
    uint8_t buf[4];
    if (readData(0x08, 4, buf)) {
        _lcc.lux       = (buf[0] << 8) | buf[1];
        _lcc.colourTemp = (buf[2] << 8) | buf[3];
        _lcc.colourIndex =  readData(0x0F);
    }
    return _lcc;
}

GY33_Processed GY33_I2C::queryProcessed() {
    uint8_t buf[3];
    if (readData(0x0C, 3, buf)) {
        _processed.r = buf[0];
        _processed.g = buf[1];
        _processed.b = buf[2];
    }
    return _processed;
}

void GY33_I2C::setLED(uint8_t p, bool save) { writeData(0x10, 0xA0 - ((p > 10 ? 10 : p) * 16)); }
void GY33_I2C::calibrateWhiteBalance() { writeData(0x10, readData(0x10) | 0x01); }
