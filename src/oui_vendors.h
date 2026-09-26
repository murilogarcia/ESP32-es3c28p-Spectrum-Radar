#pragma once
#include <stdint.h>
#include <stddef.h>

// Wi-Fi hardware-vendor lookup from a BSSID's OUI (its first 3 bytes).
//
// The table in oui_vendors.cpp is a small, editable set of common vendors, not
// the full IEEE registry. See README.md "Wi-Fi vendor lookup" to extend it.

// Returns the vendor name for a 24-bit OUI (e.g. 0x240AC4), or nullptr if the
// OUI is not in the table.
const char* oui_vendor(uint32_t oui);

// Parses the OUI from a "AA:BB:CC:DD:EE:FF" BSSID string and looks it up.
// Returns nullptr if the string is malformed or the vendor is unknown.
const char* oui_vendor_from_bssid(const char* bssidStr);
