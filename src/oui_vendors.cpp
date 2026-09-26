// Wi-Fi vendor lookup from a BSSID's OUI (its first 3 bytes).
//
// The bulk table lives in oui_vendors_data.h - a COMPLETE set of OUIs for the
// common vendors, generated from the IEEE registry by tools/gen_oui_table.py.
// That file is auto-generated; don't hand-edit it.
//
// To add or override a vendor WITHOUT regenerating, add a line to USER_OUI
// below. It is checked before the generated table, so it also overrides it.
// To add a whole new vendor's full OUI set, edit tools/gen_oui_table.py and
// regenerate. See the "Wi-Fi vendor lookup" section of README.md.

#include "oui_vendors.h"
#include "oui_vendors_data.h"

namespace {
struct UserOui { uint32_t oui; const char* vendor; };

// User additions / overrides. Checked first. Add entries above the {0, nullptr}
// end marker, e.g. {0x001122, "My Vendor"}. Order does not matter here.
const UserOui USER_OUI[] = {
    // {0x001122, "My Vendor"},
    {0, nullptr}, // end marker - keep this last
};
} // namespace

const char* oui_vendor(uint32_t oui) {
    // 1) user overrides (small, linear scan)
    for (const UserOui* u = USER_OUI; u->vendor != nullptr; ++u) {
        if (u->oui == oui) return u->vendor;
    }
    // 2) generated table (sorted, binary search)
    size_t lo = 0, hi = OUI_DATA_COUNT;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (OUI_DATA_KEYS[mid] < oui) lo = mid + 1;
        else hi = mid;
    }
    if (lo < OUI_DATA_COUNT && OUI_DATA_KEYS[lo] == oui) {
        return OUI_DATA_VENDORS[OUI_DATA_VENDOR_IDX[lo]];
    }
    return nullptr;
}

// Parses "AA:BB:CC:..." into a 24-bit OUI (separators ignored) and looks it up.
const char* oui_vendor_from_bssid(const char* bssidStr) {
    if (!bssidStr) return nullptr;
    uint32_t oui = 0;
    int nibbles = 0;
    for (const char* p = bssidStr; *p && nibbles < 6; ++p) {
        char c = *p;
        int v;
        if (c >= '0' && c <= '9') v = c - '0';
        else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
        else continue; // skip ':' separators
        oui = (oui << 4) | v;
        ++nibbles;
    }
    if (nibbles < 6) return nullptr;
    return oui_vendor(oui);
}
