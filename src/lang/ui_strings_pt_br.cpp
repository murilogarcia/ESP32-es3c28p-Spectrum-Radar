// Textos da interface em português do Brasil.
//
// Mantenha todos os marcadores printf (%s, %u, %d, %.3f, %%) na mesma ordem e
// com o mesmo tipo. A tela tem só 320 px de largura. Este arquivo deve ser
// salvo em UTF-8 (as fontes ui_font_latin_* incluem os acentos).

#include "ui_strings.h"

UiStrings UI_STRINGS_PT_BR = {
    .language_name          = "Português (BR)",
    .choose_language        = "Escolha o idioma",
    .splash_subtitle        = "Analisador de espectro Wi-Fi 2,4 GHz",
    .splash_original_by     = "Projeto original de",
    .splash_ported_by       = "Adaptação para ES3C28P por",
    .splash_autostart_fmt   = "Iniciando em %d s...",

    .header_scanning        = "ESP32 SPECTRUM RADAR | Buscando...",
    .header_total_fmt       = "ESP32 RADAR | Redes encontradas: %u",

    .tab_spectrum           = "ESPECTRO",
    .tab_devices            = "REDES",
    .tab_waterfall          = "WATERFALL",

    .spectrum_analyzing     = "2,4GHz Can 1-13 | Analisando congestionamento...",
    .spectrum_summary_fmt   = "Rec: Can %u (%u APs) | Pico: Can %u (%s)",
    .spectrum_summary_nossid_fmt = "Rec: Can %u (%u APs) | Pico: Can %u (%u APs)",
    .spectrum_y_axis        = "APs",
    .spectrum_key           = "Verde=Livre (1-2 APs)  Amarelo=Médio (3-5)  Verm.=Alto (6+)",

    .devices_initial        = "Buscando redes...",
    .devices_empty          = "Buscando pontos de acesso...",
    .devices_item_fmt       = "%d. %s  (Can %d | %d dBm)",
    .hidden_ssid            = "<Oculta>",

    .waterfall_title_fmt    = "Espectrograma | Histórico (Topo: AGORA -> Base: -%ds)",
    .waterfall_now          = "Agora",
    .waterfall_ago_fmt      = "-%ds",
    .waterfall_key          = "Azul=Nenhum  Verde=1-2  Amarelo=3-5  Vermelho=6+ APs",

    .inspector_info_fmt     = "BSSID: %s\n"
                              "Canal: %u (%.3f GHz)\n"
                              "Segurança: %s\n"
                              "Sinal: %d dBm (%d%% - %s)",
    .not_available          = "N/D",
    .inspector_close        = "FECHAR",
    .quality_excellent      = "Excelente",
    .quality_good           = "Bom",
    .quality_weak           = "Fraco",

    .auth_open              = "Aberta (sem senha)",
    .auth_wep               = "WEP",
    .auth_wpa_psk           = "WPA-PSK",
    .auth_wpa2_psk          = "WPA2-PSK (AES)",
    .auth_wpa_wpa2_psk      = "WPA/WPA2-PSK",
    .auth_wpa2_enterprise   = "WPA2-Enterprise",
    .auth_wpa3_psk          = "WPA3-PSK",
    .auth_wpa2_wpa3_psk     = "WPA2/WPA3-PSK",
    .auth_other             = "Protegida",
};
