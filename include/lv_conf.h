#pragma once

// LVGL 8.3 config for ESP32 Spectrum Radar. Anything not set here falls back to
// lib/lvgl/src/lv_conf_internal.h. Forced into effect via -DLV_CONF_INCLUDE_SIMPLE=1.

#define LV_CONF_H

#define LV_COLOR_DEPTH 16

// Use the system heap (internal RAM + PSRAM) instead of a fixed 64KB pool:
// the device list and 104-cell waterfall grid need more than that.
#define LV_MEM_CUSTOM 1

#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_DISP_DEF_REFR_PERIOD 33
#define LV_INDEV_DEF_READ_PERIOD 20

#define LV_USE_LOG 0
#define LV_USE_PERF_MONITOR 0

// Fonts: the UI uses Montserrat generated with Latin-1 accented characters
// (U+00A0-U+00FF) plus LVGL's symbols - see src/fonts/ and include/ui_fonts.h.
// The built-in ASCII-only Montserrat fonts are disabled to save flash.
#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(ui_font_latin_14)
#define LV_FONT_DEFAULT &ui_font_latin_14

// Dark theme to match the original app's look
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1
