#pragma once
#include <lvgl.h>
#include <TFT_eSPI.h>
#include "FT6336.h"

// Wires LVGL's display flush + touch input to the TFT_eSPI/FT6336 drivers.
// Call once from setup(), after tft.init()/tft.setRotation() and touch.begin().
void lv_port_init(TFT_eSPI &tft, FT6336 &touch);
