#!/usr/bin/env python3
"""Generate src/oui_vendors_data.h from the IEEE OUI registry.

The generated file holds every OUI belonging to the "common vendors" listed in
VENDORS below - a complete set for those manufacturers, not the whole ~40k-entry
IEEE registry. To add a vendor, add a line to VENDORS with substrings that match
its IEEE "Organization Name", then regenerate.

Usage:
    # download the registry (a few MB) once:
    curl -sSLo oui.csv https://standards-oui.ieee.org/oui/oui.csv
    python tools/gen_oui_table.py oui.csv src/oui_vendors_data.h

Only the MA-L registry (24-bit OUIs) is used. Entries are emitted sorted by OUI
so oui_vendor() can binary-search them.
"""
import csv
import sys

# Display name -> lowercase substrings that identify the vendor in the IEEE
# "Organization Name" column. Order here is the vendor index order in the output.
VENDORS = {
    "Espressif":      ["espressif"],
    "TP-Link":        ["tp-link", "tplink"],
    "Netgear":        ["netgear"],
    "Cisco":          ["cisco system", "cisco,"],
    "Linksys":        ["linksys"],
    "Meraki":         ["meraki"],
    "Ubiquiti":       ["ubiquiti"],
    "Apple":          ["apple, inc", "apple inc"],
    "Samsung":        ["samsung"],
    "Xiaomi":         ["xiaomi"],
    "Huawei":         ["huawei"],
    "Intel":          ["intel corp"],
    "Amazon":         ["amazon tech", "amazon.com", "amazon dev"],
    "Google":         ["google"],
    "ASUS":           ["asustek", "asus"],
    "D-Link":         ["d-link", "dlink"],
    "Zyxel":          ["zyxel"],
    "MikroTik":       ["mikrotik", "routerboard"],
    "Aruba/HPE":      ["aruba", "hewlett packard"],
    "Ruckus":         ["ruckus"],
    "Fortinet":       ["fortinet"],
    "Belkin":         ["belkin"],
    "Arris":          ["arris", "commscope"],
    "Sony":           ["sony"],
    "LG":             ["lg electronics", "lg innotek"],
    "Realtek":        ["realtek"],
    "Roku":           ["roku"],
    "Nintendo":       ["nintendo"],
    "Sonos":          ["sonos"],
    "Tuya/SmartLife": ["tuya"],
}


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: gen_oui_table.py <oui.csv> <out.h>")
    csv_path, out_path = sys.argv[1], sys.argv[2]

    names = list(VENDORS.keys())
    idx_of = {n: i for i, n in enumerate(names)}
    rows = []  # (oui_int, vendor_index)

    with open(csv_path, newline="", encoding="utf-8") as f:
        for r in csv.reader(f):
            if len(r) < 3 or r[0] != "MA-L":
                continue
            oui = r[1].strip().upper()
            org = r[2].lower()
            for label, pats in VENDORS.items():
                if any(p in org for p in pats):
                    rows.append((int(oui, 16), idx_of[label]))
                    break

    # dedup by OUI (first match wins) and sort
    seen = {}
    for oui, vi in rows:
        seen.setdefault(oui, vi)
    rows = sorted(seen.items())

    L = []
    L.append("#pragma once")
    L.append("#include <stdint.h>")
    L.append("")
    L.append("// AUTO-GENERATED from the IEEE OUI registry by tools/gen_oui_table.py.")
    L.append("// Do NOT hand-edit; add or override vendors in oui_vendors.cpp (USER_OUI)")
    L.append("// or edit tools/gen_oui_table.py and regenerate.")
    L.append("//")
    L.append("// Complete set of OUIs for the common vendors, not the whole IEEE registry.")
    L.append(f"// Entries: {len(rows)}")
    L.append("")
    L.append("static const char* const OUI_DATA_VENDORS[] = {")
    for n in names:
        L.append(f'    "{n}",')
    L.append("};")
    L.append("")
    L.append("// OUI keys, sorted ascending (binary-searched).")
    L.append("static const uint32_t OUI_DATA_KEYS[] = {")
    for i in range(0, len(rows), 8):
        chunk = "".join("0x%06X," % k for k, _ in rows[i:i + 8])
        L.append("    " + chunk)
    L.append("};")
    L.append("")
    L.append("// Vendor index into OUI_DATA_VENDORS, parallel to OUI_DATA_KEYS.")
    L.append("static const uint8_t OUI_DATA_VENDOR_IDX[] = {")
    for i in range(0, len(rows), 16):
        chunk = "".join("%d," % v for _, v in rows[i:i + 16])
        L.append("    " + chunk)
    L.append("};")
    L.append("")
    L.append(f"static const uint32_t OUI_DATA_COUNT = {len(rows)};")
    L.append("")

    with open(out_path, "w", newline="\n", encoding="utf-8") as f:
        f.write("\n".join(L))
    print(f"wrote {out_path}: {len(rows)} OUIs, {len(names)} vendors")


if __name__ == "__main__":
    main()
