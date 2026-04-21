#include "GY33.h"

GY33_UART::GY33_UART(Stream &serial) : _serial(&serial) {}

void GY33_UART::begin() {
    setOutput(OUTPUT_PROCESSED | OUTPUT_LCC | OUTPUT_RAW);
}

bool GY33_UART::update() {
    bool newFrame = false;
    while (_serial->available()) {
        uint8_t c = _serial->read();

        if (_headerCount == 2) {
            if (_frameType == 0) {
                _frameType = c; // 0x15, 0x25, 0x45, 0x55
            } else if (_dataLen == -1) {
                _dataLen = c;
                _recvPtr = 0;
            } else if (_recvPtr < _dataLen) {
                _recvBuf[_recvPtr++] = c;
            } else {
                if (checkSum(c)) {
                    parseFrame();
                    newFrame = true;
                }
                _headerCount = 0; _frameType = 0; _dataLen = -1;
            }
        } else {
            _headerCount = (c == 0x5A) ? _headerCount + 1 : 0;
        }
    }
    return newFrame;
}

void GY33_UART::parseFrame() {
    switch (_frameType) {
        case 0x15: // Raw RGB+C
            _raw.r = (_recvBuf[0] << 8) | _recvBuf[1];
            _raw.g = (_recvBuf[2] << 8) | _recvBuf[3];
            _raw.b = (_recvBuf[4] << 8) | _recvBuf[5];
            _raw.c = (_recvBuf[6] << 8) | _recvBuf[7];
            break;
        case 0x25: // LCC
            _lcc.lux = (_recvBuf[0] << 8) | _recvBuf[1];
            _lcc.colourTemp = (_recvBuf[2] << 8) | _recvBuf[3];
            _lcc.colourIndex = (_recvBuf[4] << 8) | _recvBuf[5];
            break;
        case 0x45: // Processed RGB (0-255)
            _processed.r = _recvBuf[0];
            _processed.g = _recvBuf[1];
            _processed.b = _recvBuf[2];
            break;
        case 0x55: // I2C Addr
            _i2cAddr = _recvBuf[0];
            break;
    }
}

bool GY33_UART::checkSum(uint8_t incomingChecksum) {
    uint16_t sum = 0x5A + 0x5A + _frameType + _dataLen;
    for (int i = 0; i < _dataLen; i++) sum += _recvBuf[i];
    return (uint8_t)(sum & 0xFF) == incomingChecksum;
}

void GY33_UART::sendCommand(uint8_t header, uint8_t cmd) {
    uint8_t packet[] = {header, cmd, (uint8_t)((header + cmd) & 0xFF)};
    _serial->write(packet, 3);
}

void GY33_UART::setOutput(uint8_t flags) {
    if (flags == OUTPUT_OFF) {
        _savedMode = 0x80;
        sendCommand(0xA5, 0x80);
        return;
    }
    if (flags & OUTPUT_PAUSE) {
        sendCommand(0xA5, 0x80);
        return;
    }
    if (flags & OUTPUT_RESUME) {
        sendCommand(0xA5, _savedMode);
        return;
    }
    uint8_t cmd = 0x80 | (flags & 0x07);
    _savedMode = cmd; 
    sendCommand(0xA5, cmd);
}

void GY33_UART::setLED(uint8_t power, bool save) {
    if (power > 10) power = 10;
    sendCommand(0xA5, 0x60 + (10 - power));
    if (save) { delay(100); saveToFlash(); }
}

void GY33_UART::setBaudRate(uint8_t baud, bool save) {
    sendCommand(0xA5, baud);
    if (save) { delay(100); saveToFlash(); }
}

void GY33_UART::setIntegrationTime(uint8_t itime) { sendCommand(0xA5, itime); }
void GY33_UART::saveToFlash() { sendCommand(0xA5, 0xCC); }
void GY33_UART::calibrateWhiteBalance() { sendCommand(0xA5, 0xBB); }

void GY33_UART::queryRaw() { sendCommand(0xA5, 0x51); }
void GY33_UART::queryLCC() { sendCommand(0xA5, 0x52); }
void GY33_UART::queryProcessed() { sendCommand(0xA5, 0x54); }

void GY33_UART::queryI2CAddress() { sendCommand(0xAA, 0xF5); }
void GY33_UART::setI2CAddress(uint8_t addr7bit) { sendCommand(0xAA, addr7bit << 1); }
