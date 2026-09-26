// Language registry: add new UiStrings tables to UI_LANGUAGES below.

#include "ui_strings.h"

UiStrings* const UI_LANGUAGES[] = {
    &UI_STRINGS_EN,
    &UI_STRINGS_PT_BR,
};
const size_t UI_LANGUAGE_COUNT = sizeof(UI_LANGUAGES) / sizeof(UI_LANGUAGES[0]);

const UiStrings* g_str = &UI_STRINGS_EN;

// UiStrings is only const char* fields, so it can be walked as an array.
static_assert(sizeof(UiStrings) % sizeof(const char*) == 0, "UiStrings must only contain const char* fields");

void ui_languages_init() {
    const size_t n = sizeof(UiStrings) / sizeof(const char*);
    const char* const* en = reinterpret_cast<const char* const*>(&UI_STRINGS_EN);
    for (size_t l = 0; l < UI_LANGUAGE_COUNT; l++) {
        const char** f = reinterpret_cast<const char**>(UI_LANGUAGES[l]);
        for (size_t i = 0; i < n; i++) {
            if (f[i] == nullptr) f[i] = en[i];
        }
    }
}
