#pragma once

// ES3C28P pin map (ESP32-S3 + ILI9341V + FT6336G).
// TFT pins are defined as build_flags in platformio.ini for TFT_eSPI.

// --- Touch (FT6336G, I2C, address 0x38) ---
#define PIN_TOUCH_SDA   16
#define PIN_TOUCH_SCL   15
#define PIN_TOUCH_RST   18
#define PIN_TOUCH_INT   17

// --- Misc ---
#define PIN_RGB_LED     42  // WS2812B, single pixel
#define PIN_BATT_ADC    9

// --- Display orientation ---
// The original Spectrum Radar UI is laid out for 320x240 landscape.
// 1 = landscape, 3 = landscape rotated 180 degrees (USB on the other side).
// The touch mapping in lv_port.cpp follows this automatically.
#define DISPLAY_ROTATION 1
