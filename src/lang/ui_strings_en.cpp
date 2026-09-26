// English UI strings (reference language and fallback for missing translations).
//
// Keep every printf-style placeholder (%s, %u, %d, %.3f, %%) in the same order
// and with the same type when translating. The screen is only 320 px wide.

#include "ui_strings.h"

UiStrings UI_STRINGS_EN = {
    .language_name          = "English",
    .choose_language        = "Choose language",
    .splash_subtitle        = "2.4 GHz Wi-Fi spectrum analyzer",
    .splash_original_by     = "Original project by",
    .splash_ported_by       = "ES3C28P port by",
    .splash_autostart_fmt   = "Starting in %d s...",

    .header_scanning        = "ESP32 SPECTRUM RADAR | Scanning...",
    .header_total_fmt       = "ESP32 RADAR | Total APs Discovered: %u",

    .tab_spectrum           = "SPECTRUM",
    .tab_devices            = "DEVICES",
    .tab_waterfall          = "WATERFALL",

    .spectrum_analyzing     = "2.4GHz Ch 1-13 | Analyzing Wi-Fi congestion...",
    .spectrum_summary_fmt   = "Rec: Ch %u (%u APs) | Peak: Ch %u (%s)",
    .spectrum_summary_nossid_fmt = "Rec: Ch %u (%u APs) | Peak Ch: %u (%u APs)",
    .spectrum_y_axis        = "APs",
    .spectrum_key           = "KEY: Green=Clean (1-2 APs)  Yellow=Med (3-5)  Red=Heavy (6+)",

    .devices_initial        = "Scanning networks...",
    .devices_empty          = "Scanning for Access Points...",
    .devices_item_fmt       = "%d. %s  (Ch %d | %d dBm)",
    .hidden_ssid            = "<Hidden>",

    .waterfall_title_fmt    = "Spectrogram Waterfall | History (Top: NOW -> Btm: -%ds)",
    .waterfall_now          = "NOW",
    .waterfall_ago_fmt      = "-%ds",
    .waterfall_key          = "KEY: Blue=None  Green=1-2  Yellow=3-5  Red=6+ APs",

    .inspector_info_fmt     = "BSSID: %s\n"
                              "OUI: %s  (%s)\n"
                              "Channel: %u  -  %.3f GHz  (%s)\n"
                              "Security: %s\n"
                              "Signal: %d dBm (%d%% - %s)",
    .not_available          = "N/A",
    .inspector_close        = "CLOSE INSPECTOR",
    .vendor_unknown         = "Unknown",
    .vendor_randomized      = "Randomized MAC",
    .band_24ghz             = "2.4 GHz",
    .quality_excellent      = "Excellent",
    .quality_good           = "Good",
    .quality_weak           = "Weak",

    .auth_open              = "Open (Unsecured)",
    .auth_wep               = "WEP",
    .auth_wpa_psk           = "WPA-PSK",
    .auth_wpa2_psk          = "WPA2-PSK (AES)",
    .auth_wpa_wpa2_psk      = "WPA/WPA2-PSK",
    .auth_wpa2_enterprise   = "WPA2-Enterprise",
    .auth_wpa3_psk          = "WPA3-PSK",
    .auth_wpa2_wpa3_psk     = "WPA2/WPA3-PSK",
    .auth_other             = "Secured",
};
