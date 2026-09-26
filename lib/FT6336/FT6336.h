#pragma once
#include <Arduino.h>
#include <Wire.h>

#ifndef TOUCH_SWAP_XY
#define TOUCH_SWAP_XY 0
#endif
#ifndef TOUCH_INVERT_X
#define TOUCH_INVERT_X 0
#endif
#ifndef TOUCH_INVERT_Y
#define TOUCH_INVERT_Y 0
#endif

// Minimal I2C driver for the FocalTech FT6336G capacitive touch controller.
// Register map is shared across the FT62xx/FT63xx family.
class FT6336 {
public:
    FT6336(TwoWire &wire = Wire, uint8_t address = 0x38) : _wire(wire), _addr(address) {}

    void begin(int sdaPin, int sclPin, int rstPin = -1, int intPin = -1) {
        _rstPin = rstPin;
        _intPin = intPin;
        if (_rstPin >= 0) {
            pinMode(_rstPin, OUTPUT);
            digitalWrite(_rstPin, LOW);
            delay(10);
            digitalWrite(_rstPin, HIGH);
            delay(50);
        }
        if (_intPin >= 0) {
            pinMode(_intPin, INPUT);
        }
        _wire.begin(sdaPin, sclPin);
        _wire.setClock(400000);
    }

    // Returns true if a finger is currently down, and fills x/y with panel coordinates.
    bool getTouch(int16_t *x, int16_t *y) {
        uint8_t buf[5];
        if (!readRegs(0x02, buf, 5)) return false; // TD_STATUS + P1 XH/XL/YH/YL

        uint8_t touches = buf[0] & 0x0F;
        if (touches == 0) return false;

        uint16_t rawX = ((buf[1] & 0x0F) << 8) | buf[2];
        uint16_t rawY = ((buf[3] & 0x0F) << 8) | buf[4];

        applyOrientation(rawX, rawY, x, y);
        return true;
    }

private:
    TwoWire &_wire;
    uint8_t _addr;
    int _rstPin = -1;
    int _intPin = -1;

    bool readRegs(uint8_t reg, uint8_t *buf, size_t len) {
        _wire.beginTransmission(_addr);
        _wire.write(reg);
        if (_wire.endTransmission(false) != 0) return false;
        if (_wire.requestFrom((int)_addr, (int)len) != (int)len) return false;
        for (size_t i = 0; i < len; i++) buf[i] = _wire.read();
        return true;
    }

    void applyOrientation(uint16_t rawX, uint16_t rawY, int16_t *x, int16_t *y) {
#if TOUCH_SWAP_XY
        uint16_t t = rawX; rawX = rawY; rawY = t;
#endif
#if TOUCH_INVERT_X
        rawX = 240 - 1 - rawX;
#endif
#if TOUCH_INVERT_Y
        rawY = 320 - 1 - rawY;
#endif
        *x = rawX;
        *y = rawY;
    }
};
