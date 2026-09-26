#include "lv_port.h"
#include "pins.h"
#include <esp_heap_caps.h>

namespace {
    TFT_eSPI *s_tft = nullptr;
    FT6336 *s_touch = nullptr;

    lv_disp_draw_buf_t s_drawBuf;
    lv_disp_drv_t s_dispDrv;
    lv_indev_drv_t s_indevDrv;

    int16_t s_lastX = 0;
    int16_t s_lastY = 0;

    void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
        uint32_t w = area->x2 - area->x1 + 1;
        uint32_t h = area->y2 - area->y1 + 1;

        s_tft->startWrite();
        s_tft->setAddrWindow(area->x1, area->y1, w, h);
        s_tft->pushColors((uint16_t *)color_p, w * h, true); // swap bytes (LVGL is little-endian)
        s_tft->endWrite();

        lv_disp_flush_ready(drv);
    }

    // The FT6336 always reports native portrait panel coordinates (240x320),
    // matching TFT_eSPI rotation 0 (MADCTL = MX). Convert to the active rotation.
    // ILI9341 rotation 1 is MADCTL = MV (row/column swap), rotation 3 is
    // MX|MY|MV - derived from lib/TFT_eSPI/TFT_Drivers/ILI9341_Rotation.h.
    void map_touch(int16_t tx, int16_t ty, int16_t *x, int16_t *y) {
        const int16_t W = TFT_WIDTH;   // 240, native panel width
        const int16_t H = TFT_HEIGHT;  // 320, native panel height
        switch (s_tft->getRotation() & 3) {
            case 1: *x = ty;         *y = W - 1 - tx; break;
            case 2: *x = W - 1 - tx; *y = H - 1 - ty; break;
            case 3: *x = H - 1 - ty; *y = tx;         break;
            default: *x = tx;        *y = ty;         break;
        }
    }

    void touchpad_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
        int16_t tx, ty;
        if (s_touch->getTouch(&tx, &ty)) {
            map_touch(tx, ty, &s_lastX, &s_lastY);
            data->state = LV_INDEV_STATE_PR;
        } else {
            data->state = LV_INDEV_STATE_REL;
        }
        data->point.x = s_lastX;
        data->point.y = s_lastY;
    }
}

void lv_port_init(TFT_eSPI &tft, FT6336 &touch) {
    s_tft = &tft;
    s_touch = &touch;

    lv_init();

    // Partial-render double buffers in PSRAM.
    const uint32_t bufRows = 40;
    size_t bufPixels = (size_t)tft.width() * bufRows;
    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(bufPixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(bufPixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    lv_disp_draw_buf_init(&s_drawBuf, buf1, buf2, bufPixels);

    lv_disp_drv_init(&s_dispDrv);
    s_dispDrv.hor_res = tft.width();   // 320 in landscape
    s_dispDrv.ver_res = tft.height();  // 240 in landscape
    s_dispDrv.flush_cb = disp_flush;
    s_dispDrv.draw_buf = &s_drawBuf;
    lv_disp_drv_register(&s_dispDrv);

    lv_indev_drv_init(&s_indevDrv);
    s_indevDrv.type = LV_INDEV_TYPE_POINTER;
    s_indevDrv.read_cb = touchpad_read;
    lv_indev_drv_register(&s_indevDrv);
}
