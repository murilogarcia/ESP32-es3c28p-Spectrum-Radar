#pragma once
#include <lvgl.h>

// Montserrat Medium with ASCII + Latin-1 accented characters (U+0020-U+007E,
// U+00A0-U+00FF, U+2022) and LVGL's built-in symbols (LV_SYMBOL_*).
// Generated with lv_font_conv 1.5.2 using the same options as LVGL's own
// built-in fonts; the exact command is in each file's header in src/fonts/.
LV_FONT_DECLARE(ui_font_latin_10)
LV_FONT_DECLARE(ui_font_latin_12)
LV_FONT_DECLARE(ui_font_latin_14)
LV_FONT_DECLARE(ui_font_latin_20)
