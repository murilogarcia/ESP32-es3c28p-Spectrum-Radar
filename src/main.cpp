// ESP32 Spectrum Radar - 2.4GHz Wi-Fi spectrum analyzer + rolling waterfall.
//
// Port of https://github.com/earlchirchir/ESP32-Spectrum-Radar (ESP32-2432S022C,
// LVGL 9 via esp32_smartdisplay) to the ES3C28P board (ESP32-S3, ILI9341V,
// FT6336G) using the TFT_eSPI + FT6336 + LVGL 8.3 stack verified on this board.
//
// Core 0: Wi-Fi scanner task. Core 1: Arduino loop() running LVGL.
// Shared scan results are protected by dataMutex.

#include <Arduino.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include "pins.h"
#include "FT6336.h"
#include "lv_port.h"
#include "led_status.h"
#include "ui_strings.h"
#include "ui_fonts.h"
#include "oui_vendors.h"
#include <Preferences.h>

#define MAX_NETWORKS 35
#define WATERFALL_ROWS 8
#define WATERFALL_COLS 13
#define SCAN_INTERVAL_MS 3000
#define UI_UPDATE_MS 1000

TFT_eSPI tft;
FT6336 touch;
LedStatus led;

struct WiFiItem {
    char ssid[33];
    int32_t rssi;
    uint8_t channel;
    char bssidStr[18];
    wifi_auth_mode_t authmode;
};

// ---- Shared scan data (guarded by dataMutex) ----
static WiFiItem wifiList[MAX_NETWORKS];
static uint16_t wifiCount = 0;
static uint8_t channelApCount[14];     // index 1..13
static int32_t channelPeakRssi[14];    // strongest RSSI per channel
static char channelTopSsid[14][33];    // strongest SSID per channel
static uint32_t scanGeneration = 0;    // bumped after every completed scan
static SemaphoreHandle_t dataMutex = NULL;

// ---- UI-side state (only touched from loop()/LVGL) ----
static uint8_t waterfallHistory[WATERFALL_ROWS][WATERFALL_COLS]; // row 0 = newest
static lv_obj_t* waterfall_cells[WATERFALL_ROWS][WATERFALL_COLS];
static uint32_t lastWaterfallGen = 0;
static uint32_t lastListGen = 0;
static bool ui_ready = false;          // main UI built (language picked)

static lv_obj_t* tabview = nullptr;
static lv_obj_t* tab_spectrum = nullptr;
static lv_obj_t* tab_devices = nullptr;
static lv_obj_t* tab_waterfall = nullptr;
static lv_obj_t* spectrum_chart = nullptr;

// 3 bar series: green (1-2 APs), yellow (3-5), red (6+)
static lv_chart_series_t* series_green = nullptr;
static lv_chart_series_t* series_yellow = nullptr;
static lv_chart_series_t* series_red = nullptr;

static lv_obj_t* device_list = nullptr;
static lv_obj_t* status_label = nullptr;
static lv_obj_t* channel_val_labels[14];
static lv_obj_t* summary_label = nullptr;
static lv_obj_t* inspector_modal = nullptr;

static lv_style_t style_wf_cell;

// Local style setters apply to the main part in the default state.
// (The original passed LV_OPA_COVER as the selector, which LVGL reads as a
// state mask - so those colors never applied.)
#define SEL_MAIN (LV_PART_MAIN | LV_STATE_DEFAULT)

static lv_color_t color_gold() { return lv_color_make(255, 215, 0); }
static lv_color_t color_axis() { return lv_color_make(170, 170, 190); }

const char* getAuthModeName(wifi_auth_mode_t authmode) {
    switch (authmode) {
        case WIFI_AUTH_OPEN: return g_str->auth_open;
        case WIFI_AUTH_WEP: return g_str->auth_wep;
        case WIFI_AUTH_WPA_PSK: return g_str->auth_wpa_psk;
        case WIFI_AUTH_WPA2_PSK: return g_str->auth_wpa2_psk;
        case WIFI_AUTH_WPA_WPA2_PSK: return g_str->auth_wpa_wpa2_psk;
        case WIFI_AUTH_WPA2_ENTERPRISE: return g_str->auth_wpa2_enterprise;
        case WIFI_AUTH_WPA3_PSK: return g_str->auth_wpa3_psk;
        case WIFI_AUTH_WPA2_WPA3_PSK: return g_str->auth_wpa2_wpa3_psk;
        default: return g_str->auth_other;
    }
}

static lv_obj_t* make_label(lv_obj_t* parent, const char* text, const lv_font_t* font, lv_color_t color) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, SEL_MAIN);
    lv_obj_set_style_text_color(l, color, SEL_MAIN);
    return l;
}

// Hidden networks are stored with an empty SSID and labelled at display time,
// so the label follows the language picked on the splash screen.
static const char* display_ssid(const char* ssid) {
    return ssid[0] ? ssid : g_str->hidden_ssid;
}

static lv_color_t congestion_color(uint8_t cnt) {
    if (cnt == 0) return lv_color_make(25, 35, 60);    // quiet
    if (cnt <= 2) return lv_color_make(0, 230, 120);   // clean
    if (cnt <= 5) return lv_color_make(255, 190, 0);   // moderate
    return lv_color_make(255, 40, 40);                 // congested
}

// ---------------------------------------------------------------------------
// Network inspector modal
// ---------------------------------------------------------------------------
static void close_inspector_cb(lv_event_t* e) {
    if (inspector_modal) {
        lv_obj_del_async(inspector_modal); // async: we're inside a child's event
        inspector_modal = nullptr;
    }
}

void openNetworkInspector(uint32_t idx) {
    WiFiItem item;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) != pdTRUE) return;
    bool valid = idx < wifiCount;
    if (valid) item = wifiList[idx];
    xSemaphoreGive(dataMutex);
    if (!valid) return;

    if (inspector_modal) {
        lv_obj_del(inspector_modal);
        inspector_modal = nullptr;
    }

    inspector_modal = lv_obj_create(lv_layer_top());
    lv_obj_set_size(inspector_modal, 300, 228);
    lv_obj_align(inspector_modal, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(inspector_modal, lv_color_make(18, 24, 38), SEL_MAIN);
    lv_obj_set_style_bg_opa(inspector_modal, LV_OPA_COVER, SEL_MAIN);
    lv_obj_set_style_border_color(inspector_modal, lv_color_make(0, 210, 255), SEL_MAIN);
    lv_obj_set_style_border_width(inspector_modal, 2, SEL_MAIN);
    lv_obj_set_style_radius(inspector_modal, 10, SEL_MAIN);
    lv_obj_set_style_shadow_width(inspector_modal, 20, SEL_MAIN);
    lv_obj_set_style_pad_all(inspector_modal, 10, SEL_MAIN);
    lv_obj_clear_flag(inspector_modal, LV_OBJ_FLAG_SCROLLABLE);

    // Title: SSID
    lv_obj_t* title_lbl = make_label(inspector_modal, "", &ui_font_latin_12, lv_color_make(0, 240, 255));
    lv_label_set_text_fmt(title_lbl, LV_SYMBOL_WIFI " %s", display_ssid(item.ssid));
    lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(title_lbl, 236);
    lv_obj_align(title_lbl, LV_ALIGN_TOP_LEFT, 0, 4);

    // Top-right X button
    lv_obj_t* x_btn = lv_btn_create(inspector_modal);
    lv_obj_set_size(x_btn, 30, 26);
    lv_obj_align(x_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(x_btn, lv_color_make(180, 40, 40), SEL_MAIN);
    lv_obj_add_event_cb(x_btn, close_inspector_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* x_lbl = make_label(x_btn, LV_SYMBOL_CLOSE, &ui_font_latin_12, lv_color_white());
    lv_obj_center(x_lbl);

    // Details
    int qualityPct = constrain(map(item.rssi, -100, -30, 0, 100), 0, 100);
    const char* qualDesc = (qualityPct > 70) ? g_str->quality_excellent : (qualityPct > 40 ? g_str->quality_good : g_str->quality_weak);
    float freqGHz = 2.407f + (item.channel * 0.005f);

    // Vendor from the BSSID's OUI. A locally-administered BSSID (bit 0x02 of the
    // first octet) that we can't match is a randomized/virtual MAC, not a real
    // vendor, so label it as such instead of "Unknown".
    const char* vendor = oui_vendor_from_bssid(item.bssidStr);
    if (!vendor) {
        unsigned firstOctet = (unsigned)strtoul(item.bssidStr, nullptr, 16);
        vendor = (firstOctet & 0x02) ? g_str->vendor_randomized : g_str->vendor_unknown;
    }

    // OUI = first 3 bytes of the BSSID, e.g. "24:0A:C4" (first 8 chars of "AA:BB:CC:...").
    char ouiStr[9];
    if (strlen(item.bssidStr) >= 8) {
        memcpy(ouiStr, item.bssidStr, 8);
        ouiStr[8] = '\0';
    } else {
        strlcpy(ouiStr, g_str->not_available, sizeof(ouiStr));
    }

    char infoBuf[256];
    snprintf(infoBuf, sizeof(infoBuf), g_str->inspector_info_fmt,
             item.bssidStr[0] != '\0' ? item.bssidStr : g_str->not_available,
             ouiStr, vendor,
             item.channel, freqGHz, g_str->band_24ghz,
             getAuthModeName(item.authmode),
             (int)item.rssi, qualityPct, qualDesc);
    lv_obj_t* info_lbl = make_label(inspector_modal, infoBuf, &ui_font_latin_12, lv_color_make(220, 220, 230));
    lv_obj_align(info_lbl, LV_ALIGN_TOP_LEFT, 0, 32);

    // Signal strength bar
    lv_obj_t* bar = lv_bar_create(inspector_modal);
    lv_obj_set_size(bar, 276, 14);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 130);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, qualityPct, LV_ANIM_ON);
    lv_color_t barColor = (qualityPct > 70) ? lv_color_make(0, 230, 100)
                        : (qualityPct > 40) ? lv_color_make(255, 200, 0)
                                            : lv_color_make(255, 60, 60);
    lv_obj_set_style_bg_color(bar, barColor, LV_PART_INDICATOR);

    // Large close button
    lv_obj_t* close_btn = lv_btn_create(inspector_modal);
    lv_obj_set_size(close_btn, 150, 36);
    lv_obj_align(close_btn, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(close_btn, lv_color_make(0, 130, 240), SEL_MAIN);
    lv_obj_add_event_cb(close_btn, close_inspector_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* btn_lbl = make_label(close_btn, g_str->inspector_close, &ui_font_latin_12, lv_color_white());
    lv_obj_center(btn_lbl);
}

static void list_btn_event_cb(lv_event_t* e) {
    uint32_t idx = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    openNetworkInspector(idx);
}

// ---------------------------------------------------------------------------
// Core 0: Wi-Fi scanner task
// ---------------------------------------------------------------------------
void networkScannerTask(void* pvParameters) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));

    while (true) {
        int n = WiFi.scanNetworks(false, true); // blocking scan, include hidden
        if (n >= 0) {
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
                wifiCount = min(n, (int)MAX_NETWORKS);
                memset(channelApCount, 0, sizeof(channelApCount));
                for (int ch = 1; ch <= 13; ch++) {
                    channelPeakRssi[ch] = -120;
                    channelTopSsid[ch][0] = '\0';
                }

                for (int i = 0; i < wifiCount; i++) {
                    String s = WiFi.SSID(i);
                    strlcpy(wifiList[i].ssid, s.c_str(), sizeof(wifiList[i].ssid));
                    wifiList[i].rssi = WiFi.RSSI(i);
                    wifiList[i].channel = WiFi.channel(i);
                    wifiList[i].authmode = WiFi.encryptionType(i);
                    strlcpy(wifiList[i].bssidStr, WiFi.BSSIDstr(i).c_str(), sizeof(wifiList[i].bssidStr));

                    uint8_t ch = wifiList[i].channel;
                    if (ch >= 1 && ch <= 13) {
                        channelApCount[ch]++;
                        if (wifiList[i].rssi > channelPeakRssi[ch]) {
                            channelPeakRssi[ch] = wifiList[i].rssi;
                            strlcpy(channelTopSsid[ch], wifiList[i].ssid, sizeof(channelTopSsid[ch]));
                        }
                    }
                }
                scanGeneration++;
                xSemaphoreGive(dataMutex);
            }
            WiFi.scanDelete();
        }
        vTaskDelay(pdMS_TO_TICKS(SCAN_INTERVAL_MS));
    }
}

// ---------------------------------------------------------------------------
// UI construction (320x240 landscape)
// ---------------------------------------------------------------------------
static lv_obj_t* make_row_container(lv_obj_t* parent, lv_coord_t w, lv_coord_t h, lv_coord_t x, lv_coord_t y) {
    lv_obj_t* c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_set_size(c, w, h);
    lv_obj_align(c, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

static void add_channel_axis_labels(lv_obj_t* container) {
    for (int ch = 1; ch <= 13; ch++) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%d", ch);
        bool primary = (ch == 1 || ch == 6 || ch == 11);
        make_label(container, buf, &ui_font_latin_10, primary ? color_gold() : color_axis());
    }
}

void buildUI() {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_make(15, 18, 28), SEL_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, SEL_MAIN);
    lv_obj_set_style_pad_all(scr, 0, SEL_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    // Fade in from the splash screen and free it afterwards.
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, true);

    // Header (y = 0..15)
    status_label = make_label(scr, g_str->header_scanning, &ui_font_latin_12, lv_color_make(0, 230, 255));
    lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 6, 1);

    // Tabview (y = 16..239), 24px tab bar on top
    tabview = lv_tabview_create(scr, LV_DIR_TOP, 24);
    lv_obj_set_size(tabview, LV_PCT(100), 224);
    lv_obj_align(tabview, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(tabview, lv_color_make(15, 18, 28), SEL_MAIN);

    lv_obj_t* tab_btns = lv_tabview_get_tab_btns(tabview);
    lv_obj_set_style_bg_opa(tab_btns, LV_OPA_COVER, SEL_MAIN);
    lv_obj_set_style_bg_color(tab_btns, lv_color_make(30, 42, 65), SEL_MAIN);
    lv_obj_set_style_text_font(tab_btns, &ui_font_latin_12, LV_PART_ITEMS);
    lv_obj_set_style_text_color(tab_btns, lv_color_make(180, 225, 255), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(tab_btns, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(tab_btns, lv_color_make(0, 130, 240), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(tab_btns, lv_color_white(), LV_PART_ITEMS | LV_STATE_CHECKED);

    tab_spectrum = lv_tabview_add_tab(tabview, g_str->tab_spectrum);
    tab_devices = lv_tabview_add_tab(tabview, g_str->tab_devices);
    tab_waterfall = lv_tabview_add_tab(tabview, g_str->tab_waterfall);
    lv_obj_set_style_pad_all(tab_spectrum, 0, SEL_MAIN);
    lv_obj_set_style_pad_all(tab_devices, 0, SEL_MAIN);
    lv_obj_set_style_pad_all(tab_waterfall, 0, SEL_MAIN);
    lv_obj_clear_flag(tab_spectrum, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(tab_waterfall, LV_OBJ_FLAG_SCROLLABLE);

    // ---------------- Tab 1: spectrum bar chart ----------------
    summary_label = make_label(tab_spectrum, g_str->spectrum_analyzing,
                               &ui_font_latin_12, lv_color_make(230, 230, 230));
    lv_label_set_long_mode(summary_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(summary_label, 312);
    lv_obj_align(summary_label, LV_ALIGN_TOP_LEFT, 4, 0);

    const lv_color_t axisGrey = lv_color_make(150, 150, 170);
    lv_obj_align(make_label(tab_spectrum, g_str->spectrum_y_axis, &ui_font_latin_10, lv_color_make(0, 220, 255)), LV_ALIGN_TOP_LEFT, 0, 16);
    lv_obj_align(make_label(tab_spectrum, "10-", &ui_font_latin_10, axisGrey), LV_ALIGN_TOP_LEFT, 0, 28);
    lv_obj_align(make_label(tab_spectrum, " 5-", &ui_font_latin_10, axisGrey), LV_ALIGN_TOP_LEFT, 0, 84);
    lv_obj_align(make_label(tab_spectrum, " 0-", &ui_font_latin_10, axisGrey), LV_ALIGN_TOP_LEFT, 0, 143);

    spectrum_chart = lv_chart_create(tab_spectrum);
    lv_obj_set_size(spectrum_chart, 275, 125);
    lv_obj_align(spectrum_chart, LV_ALIGN_TOP_LEFT, 24, 28);
    lv_chart_set_type(spectrum_chart, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(spectrum_chart, 13);
    lv_chart_set_range(spectrum_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 10);
    lv_chart_set_div_line_count(spectrum_chart, 3, 0);
    lv_obj_set_style_bg_color(spectrum_chart, lv_color_make(18, 22, 36), SEL_MAIN);
    lv_obj_set_style_bg_opa(spectrum_chart, LV_OPA_COVER, SEL_MAIN);
    lv_obj_set_style_border_color(spectrum_chart, lv_color_make(40, 50, 75), SEL_MAIN);
    lv_obj_set_style_line_color(spectrum_chart, lv_color_make(40, 50, 75), SEL_MAIN);
    lv_obj_set_style_pad_all(spectrum_chart, 4, SEL_MAIN);
    lv_obj_set_style_pad_column(spectrum_chart, 0, LV_PART_ITEMS);   // gap between series bars
    lv_obj_set_style_pad_column(spectrum_chart, 4, SEL_MAIN);        // gap between channels
    lv_obj_clear_flag(spectrum_chart, LV_OBJ_FLAG_CLICKABLE);

    series_green = lv_chart_add_series(spectrum_chart, lv_color_make(0, 230, 100), LV_CHART_AXIS_PRIMARY_Y);
    series_yellow = lv_chart_add_series(spectrum_chart, lv_color_make(255, 200, 0), LV_CHART_AXIS_PRIMARY_Y);
    series_red = lv_chart_add_series(spectrum_chart, lv_color_make(255, 50, 50), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_value(spectrum_chart, series_green, 0);
    lv_chart_set_all_value(spectrum_chart, series_yellow, 0);
    lv_chart_set_all_value(spectrum_chart, series_red, 0);

    lv_obj_t* val_container = make_row_container(tab_spectrum, 275, 12, 24, 15);
    lv_obj_t* x_container = make_row_container(tab_spectrum, 275, 14, 24, 155);
    for (int ch = 1; ch <= 13; ch++) {
        channel_val_labels[ch] = make_label(val_container, "", &ui_font_latin_10, color_axis());
        lv_obj_set_style_text_align(channel_val_labels[ch], LV_TEXT_ALIGN_CENTER, SEL_MAIN);
    }
    add_channel_axis_labels(x_container);

    lv_obj_align(make_label(tab_spectrum, g_str->spectrum_key,
                            &ui_font_latin_10, lv_color_make(180, 200, 220)),
                 LV_ALIGN_TOP_LEFT, 2, 174);

    // ---------------- Tab 2: device list ----------------
    device_list = lv_list_create(tab_devices);
    lv_obj_set_size(device_list, LV_PCT(100), LV_PCT(100));
    lv_obj_align(device_list, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(device_list, &ui_font_latin_12, SEL_MAIN);
    lv_list_add_btn(device_list, NULL, g_str->devices_initial);

    // ---------------- Tab 3: waterfall heatmap ----------------
    char wfTitle[128];
    snprintf(wfTitle, sizeof(wfTitle), g_str->waterfall_title_fmt,
             (WATERFALL_ROWS - 1) * SCAN_INTERVAL_MS / 1000);
    lv_obj_align(make_label(tab_waterfall, wfTitle, &ui_font_latin_10, lv_color_make(0, 230, 255)),
                 LV_ALIGN_TOP_LEFT, 2, 0);

    const int cellW = 20, cellH = 17, gridStartX = 32, gridStartY = 16;

    for (int r = 0; r < WATERFALL_ROWS; r++) {
        char tBuf[16];
        if (r == 0) strlcpy(tBuf, g_str->waterfall_now, sizeof(tBuf));
        else snprintf(tBuf, sizeof(tBuf), g_str->waterfall_ago_fmt, r * SCAN_INTERVAL_MS / 1000);
        lv_obj_align(make_label(tab_waterfall, tBuf, &ui_font_latin_10, lv_color_make(150, 160, 180)),
                     LV_ALIGN_TOP_LEFT, 0, gridStartY + r * cellH + 2);
    }

    // One shared style for all 104 cells; only bg color is set per cell.
    lv_style_init(&style_wf_cell);
    lv_style_set_bg_opa(&style_wf_cell, LV_OPA_COVER);
    lv_style_set_bg_color(&style_wf_cell, congestion_color(0));
    lv_style_set_border_width(&style_wf_cell, 1);
    lv_style_set_border_color(&style_wf_cell, lv_color_make(10, 14, 25));
    lv_style_set_radius(&style_wf_cell, 2);
    lv_style_set_pad_all(&style_wf_cell, 0);

    for (int r = 0; r < WATERFALL_ROWS; r++) {
        for (int c = 0; c < WATERFALL_COLS; c++) {
            lv_obj_t* cell = lv_obj_create(tab_waterfall);
            lv_obj_remove_style_all(cell);
            lv_obj_add_style(cell, &style_wf_cell, SEL_MAIN);
            lv_obj_set_size(cell, cellW - 2, cellH - 2);
            lv_obj_align(cell, LV_ALIGN_TOP_LEFT, gridStartX + c * cellW, gridStartY + r * cellH);
            lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
            waterfall_cells[r][c] = cell;
        }
    }

    lv_obj_t* wf_x_container = make_row_container(tab_waterfall, WATERFALL_COLS * cellW, 14,
                                                  gridStartX - 1, gridStartY + WATERFALL_ROWS * cellH + 2);
    add_channel_axis_labels(wf_x_container);

    lv_obj_align(make_label(tab_waterfall, g_str->waterfall_key,
                            &ui_font_latin_10, lv_color_make(180, 200, 220)),
                 LV_ALIGN_TOP_LEFT, 2, 174);
}

// ---------------------------------------------------------------------------
// Periodic UI refresh (runs on core 1 from loop())
// ---------------------------------------------------------------------------
void updateUI() {
    if (!ui_ready) return;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) != pdTRUE) return;

    const uint32_t gen = scanGeneration;

    // Header
    if (gen == 0) {
        lv_label_set_text(status_label, g_str->header_scanning);
    } else {
        lv_label_set_text_fmt(status_label, g_str->header_total_fmt, wifiCount);
    }

    // 1. Spectrum bars + per-channel counts + summary
    uint8_t maxCount = 0, peakChannel = 1, quietestCh = 1, minCount = 255;
    for (int ch = 1; ch <= 13; ch++) {
        uint8_t cnt = channelApCount[ch];
        lv_chart_set_value_by_id(spectrum_chart, series_green, ch - 1, cnt <= 2 ? cnt : 0);
        lv_chart_set_value_by_id(spectrum_chart, series_yellow, ch - 1, (cnt > 2 && cnt <= 5) ? cnt : 0);
        lv_chart_set_value_by_id(spectrum_chart, series_red, ch - 1, cnt > 5 ? cnt : 0);

        if (cnt > maxCount) { maxCount = cnt; peakChannel = ch; }
        if (cnt < minCount) { minCount = cnt; quietestCh = ch; }

        if (cnt > 0) {
            lv_label_set_text_fmt(channel_val_labels[ch], "%u", cnt);
            lv_color_t c = cnt <= 2 ? lv_color_make(0, 255, 120)
                         : cnt <= 5 ? lv_color_make(255, 200, 0)
                                    : lv_color_make(255, 60, 60);
            lv_obj_set_style_text_color(channel_val_labels[ch], c, SEL_MAIN);
        } else {
            lv_label_set_text(channel_val_labels[ch], "");
        }
    }
    lv_chart_refresh(spectrum_chart);

    if (gen > 0) {
        if (wifiCount > 0 && channelPeakRssi[peakChannel] > -120) {
            lv_label_set_text_fmt(summary_label, g_str->spectrum_summary_fmt,
                                  quietestCh, minCount, peakChannel, display_ssid(channelTopSsid[peakChannel]));
        } else {
            lv_label_set_text_fmt(summary_label, g_str->spectrum_summary_nossid_fmt,
                                  quietestCh, minCount, peakChannel, maxCount);
        }
    }

    // 2. Waterfall: advance one row per completed scan (not per UI tick), so
    //    the row time labels match reality.
    if (gen != lastWaterfallGen) {
        lastWaterfallGen = gen;
        memmove(&waterfallHistory[1], &waterfallHistory[0], sizeof(waterfallHistory[0]) * (WATERFALL_ROWS - 1));
        for (int c = 0; c < WATERFALL_COLS; c++) waterfallHistory[0][c] = channelApCount[c + 1];

        for (int r = 0; r < WATERFALL_ROWS; r++)
            for (int c = 0; c < WATERFALL_COLS; c++)
                lv_obj_set_style_bg_color(waterfall_cells[r][c], congestion_color(waterfallHistory[r][c]), SEL_MAIN);
    }

    // 3. Device list: rebuild only on new scan data, and never while the user
    //    is scrolling or inspecting (retried on a later tick). Keeps scroll pos.
    if (gen != lastListGen && !inspector_modal && !lv_obj_is_scrolling(device_list)) {
        lastListGen = gen;
        lv_coord_t saved_scroll_y = lv_obj_get_scroll_y(device_list);

        lv_obj_clean(device_list);
        if (wifiCount == 0) {
            lv_list_add_btn(device_list, NULL, g_str->devices_empty);
        } else {
            for (int i = 0; i < wifiCount; i++) {
                char itemBuf[112];
                snprintf(itemBuf, sizeof(itemBuf), g_str->devices_item_fmt,
                         i + 1, display_ssid(wifiList[i].ssid), wifiList[i].channel, (int)wifiList[i].rssi);
                lv_obj_t* btn = lv_list_add_btn(device_list, LV_SYMBOL_WIFI, itemBuf);
                lv_obj_add_event_cb(btn, list_btn_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
            }
        }
        lv_obj_update_layout(device_list);
        lv_obj_scroll_to_y(device_list, saved_scroll_y, LV_ANIM_OFF);
    }

    xSemaphoreGive(dataMutex);
}

// ---------------------------------------------------------------------------
// Splash screen: credits + language picker
// ---------------------------------------------------------------------------
#define SPLASH_AUTOSTART_S 10   // start with the remembered language after this many seconds

static size_t saved_lang = 0;
static lv_timer_t* splash_timer = nullptr;
static lv_obj_t* splash_countdown = nullptr;
static int splash_seconds_left = SPLASH_AUTOSTART_S;

static size_t load_saved_language() {
    Preferences prefs;
    prefs.begin("radar", true);
    size_t idx = prefs.getUChar("lang", 0);
    prefs.end();
    return idx < UI_LANGUAGE_COUNT ? idx : 0;
}

static void save_language(size_t idx) {
    Preferences prefs;
    prefs.begin("radar", false);
    prefs.putUChar("lang", (uint8_t)idx);
    prefs.end();
}

static void start_app(size_t lang) {
    if (ui_ready) return;
    if (splash_timer) { lv_timer_del(splash_timer); splash_timer = nullptr; }
    g_str = UI_LANGUAGES[lang];
    if (lang != saved_lang) save_language(lang);
    Serial.printf("[System] UI language: %s\n", g_str->language_name);
    buildUI();   // fades in and deletes the splash screen
    ui_ready = true;
    updateUI();
}

static void splash_lang_btn_cb(lv_event_t* e) {
    start_app((size_t)(uintptr_t)lv_event_get_user_data(e));
}

static void splash_timer_cb(lv_timer_t* t) {
    if (--splash_seconds_left <= 0) {
        start_app(saved_lang);
        return;
    }
    lv_label_set_text_fmt(splash_countdown, UI_LANGUAGES[saved_lang]->splash_autostart_fmt, splash_seconds_left);
}

static lv_obj_t* splash_label(lv_obj_t* parent, const char* text, const lv_font_t* font, lv_color_t color, lv_coord_t y) {
    lv_obj_t* l = make_label(parent, text, font, color);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, SEL_MAIN);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, y);
    return l;
}

void buildSplash() {
    const UiStrings* L = UI_LANGUAGES[saved_lang];

    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_make(15, 18, 28), SEL_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, SEL_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_scr_load(scr);

    const lv_color_t cyan = lv_color_make(0, 230, 255);
    const lv_color_t grey = lv_color_make(150, 160, 180);
    const lv_color_t white = lv_color_make(230, 230, 235);

    splash_label(scr, LV_SYMBOL_WIFI "  " APP_TITLE, &ui_font_latin_20, cyan, 10);
    splash_label(scr, L->splash_subtitle, &ui_font_latin_12, grey, 38);

    splash_label(scr, L->splash_original_by, &ui_font_latin_10, grey, 64);
    splash_label(scr, CREDIT_ORIGINAL, &ui_font_latin_12, white, 77);
    splash_label(scr, L->splash_ported_by, &ui_font_latin_10, grey, 98);
    splash_label(scr, CREDIT_PORT, &ui_font_latin_12, white, 111);

    // "Choose language" in every available language, e.g. "Choose language / Escolha o idioma"
    String choose;
    for (size_t i = 0; i < UI_LANGUAGE_COUNT; i++) {
        if (i) choose += "  /  ";
        choose += UI_LANGUAGES[i]->choose_language;
    }
    splash_label(scr, choose.c_str(), &ui_font_latin_10, cyan, 138);

    // One button per language; the remembered one is highlighted.
    lv_obj_t* row = lv_obj_create(scr);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, 320, 48);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 156);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    for (size_t i = 0; i < UI_LANGUAGE_COUNT; i++) {
        lv_obj_t* btn = lv_btn_create(row);
        lv_obj_set_size(btn, 140, 42);
        bool current = (i == saved_lang);
        lv_obj_set_style_bg_color(btn, current ? lv_color_make(0, 130, 240) : lv_color_make(30, 42, 65), SEL_MAIN);
        lv_obj_set_style_border_width(btn, current ? 2 : 0, SEL_MAIN);
        lv_obj_set_style_border_color(btn, cyan, SEL_MAIN);
        lv_obj_add_event_cb(btn, splash_lang_btn_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        lv_obj_t* lbl = make_label(btn, UI_LANGUAGES[i]->language_name, &ui_font_latin_14, lv_color_white());
        lv_obj_center(lbl);
    }

    splash_seconds_left = SPLASH_AUTOSTART_S;
    splash_countdown = splash_label(scr, "", &ui_font_latin_10, grey, 216);
    lv_label_set_text_fmt(splash_countdown, L->splash_autostart_fmt, splash_seconds_left);
    splash_timer = lv_timer_create(splash_timer_cb, 1000, NULL);
}

// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(1500); // let the native USB-CDC host attach before printing
    Serial.println("[System] Starting ESP32 Spectrum Radar (ES3C28P)...");

    dataMutex = xSemaphoreCreateMutex();
    ui_languages_init();
    saved_lang = load_saved_language();

    tft.init();                        // also turns the backlight on (TFT_BL)
    tft.setRotation(DISPLAY_ROTATION); // landscape 320x240
    tft.fillScreen(TFT_BLACK);

    touch.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL, PIN_TOUCH_RST, PIN_TOUCH_INT);
    led.begin();

    lv_port_init(tft, touch);
    buildSplash();

    // Scanner on core 0 (starts right away, so data is ready when the splash
    // closes); Arduino loop()/LVGL run on core 1.
    xTaskCreatePinnedToCore(networkScannerTask, "NetScanTask", 6144, NULL, 1, NULL, 0);
    Serial.println("[System] FreeRTOS scanner launched on core 0");
}

void loop() {
    static uint32_t last_ui_update = 0;
    uint32_t now = millis();
    if (now - last_ui_update >= UI_UPDATE_MS) {
        last_ui_update = now;
        updateUI();
    }

    lv_timer_handler();
    led.tick();
    delay(5);
}
