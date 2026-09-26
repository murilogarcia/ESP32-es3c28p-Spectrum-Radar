#pragma once
#include <stddef.h>

// All text shown on screen. Each language is one UiStrings table in
// src/lang/ui_strings_<code>.cpp, registered in src/lang/ui_languages.cpp.
// The language is picked on the splash screen at startup and remembered.
//
// To add a language: see "Translating the UI" in README.md.

// ---- Credits (names are not translated) ----
#define APP_TITLE          "ESP32 Spectrum Radar"
#define CREDIT_ORIGINAL    "Earl  -  github.com/earlchirchir"
#define CREDIT_PORT        "Murilo Garcia  -  github.com/murilogarcia"

// Every field is a const char*. A field left out of a translation falls back
// to the English text at startup (see ui_languages_init()).
struct UiStrings {
    // ---- Language picker / splash screen ----
    const char* language_name;          // shown on this language's button, in the language itself
    const char* choose_language;
    const char* splash_subtitle;
    const char* splash_original_by;
    const char* splash_ported_by;
    const char* splash_autostart_fmt;   // %d seconds

    // ---- Header bar ----
    const char* header_scanning;
    const char* header_total_fmt;       // %u network count

    // ---- Tab names ----
    const char* tab_spectrum;
    const char* tab_devices;
    const char* tab_waterfall;

    // ---- Spectrum tab ----
    const char* spectrum_analyzing;
    const char* spectrum_summary_fmt;        // %u quietest ch, %u its APs, %u busiest ch, %s strongest SSID
    const char* spectrum_summary_nossid_fmt; // %u quietest ch, %u its APs, %u busiest ch, %u its APs
    const char* spectrum_y_axis;
    const char* spectrum_key;

    // ---- Devices tab ----
    const char* devices_initial;
    const char* devices_empty;
    const char* devices_item_fmt;       // %d position, %s SSID, %d channel, %d RSSI dBm
    const char* hidden_ssid;

    // ---- Waterfall tab ----
    const char* waterfall_title_fmt;    // %d seconds of history
    const char* waterfall_now;
    const char* waterfall_ago_fmt;      // %d seconds ago
    const char* waterfall_key;

    // ---- Network inspector popup ----
    // %s BSSID, %s OUI (first 3 bytes), %s vendor, %u channel, %.3f GHz,
    // %s band, %s security, %d RSSI dBm, %d quality %, %s quality word
    const char* inspector_info_fmt;
    const char* not_available;
    const char* inspector_close;
    const char* vendor_unknown;      // OUI not in the table
    const char* vendor_randomized;   // locally-administered (randomized/virtual) BSSID
    const char* band_24ghz;
    const char* quality_excellent;
    const char* quality_good;
    const char* quality_weak;

    // ---- Wi-Fi security types ----
    const char* auth_open;
    const char* auth_wep;
    const char* auth_wpa_psk;
    const char* auth_wpa2_psk;
    const char* auth_wpa_wpa2_psk;
    const char* auth_wpa2_enterprise;
    const char* auth_wpa3_psk;
    const char* auth_wpa2_wpa3_psk;
    const char* auth_other;
};

// Language tables (defined in src/lang/).
extern UiStrings UI_STRINGS_EN;
extern UiStrings UI_STRINGS_PT_BR;

// Registered languages, in the order their buttons appear on the splash screen.
extern UiStrings* const UI_LANGUAGES[];
extern const size_t UI_LANGUAGE_COUNT;

// The active language. Set once the user picks a language.
extern const UiStrings* g_str;

// Fills any missing (nullptr) translation with the English text. Call once at boot.
void ui_languages_init();
