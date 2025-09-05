#pragma once
#include <string>
#include <vector>
#include <optional>

// Rappresenta una combinazione da inviare: es. CTRL+ALT+K
struct HotkeyChord {
    std::vector<unsigned short> modifiers; // VK_CONTROL, VK_MENU (ALT), VK_SHIFT, VK_LWIN
    unsigned short key = 0;                // VK_* principale, es. 'K' (0x4B) o VK_MEDIA_PLAY_PAUSE
};

// Converte una stringa tipo "CTRL+ALT+K" o "MEDIA_PLAY_PAUSE" in HotkeyChord
// Ritorna std::nullopt se il token principale non è riconosciuto.
std::optional<HotkeyChord> ParseHotkey(const std::string& spec);

// Invia una combinazione come TAP (press+release di mod → key → release key → release mod invertiti)
bool SendHotkeyTap(const HotkeyChord& chord);
