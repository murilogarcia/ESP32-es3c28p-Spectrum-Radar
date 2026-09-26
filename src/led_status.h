#pragma once
#include <Adafruit_NeoPixel.h>
#include "pins.h"

class LedStatus {
public:
    void begin() {
        _px.begin();
        _px.setBrightness(60);
        _px.show();
    }

    // Call every loop(); colors cycle continuously as a "still alive" heartbeat.
    void tick() {
        uint32_t now = millis();
        if (now - _lastStep < 15) return;
        _lastStep = now;
        _hue += 256;
        _px.setPixelColor(0, _px.gamma32(_px.ColorHSV(_hue)));
        _px.show();
    }

    void setSolid(uint8_t r, uint8_t g, uint8_t b) {
        _px.setPixelColor(0, _px.Color(r, g, b));
        _px.show();
    }

private:
    Adafruit_NeoPixel _px{1, PIN_RGB_LED, NEO_GRB + NEO_KHZ800};
    uint32_t _lastStep = 0;
    uint16_t _hue = 0;
};
