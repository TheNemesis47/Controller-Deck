#include "Core/Actions/Hotkey.hpp"
#include <windows.h>
#include <algorithm>
#include <cctype>
#include <vector>
#include <string>

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

static unsigned short nameToVk(std::string name) {
    name = toLower(name);

    // Modificatori gestiti altrove
    if (name == "ctrl" || name == "control") return VK_CONTROL;
    if (name == "alt" || name == "menu")    return VK_MENU;
    if (name == "shift")                     return VK_SHIFT;
    if (name == "win" || name == "lwin")   return VK_LWIN;
    if (name == "rwin")                      return VK_RWIN;

    // A-Z
    if (name.size() == 1 && name[0] >= 'a' && name[0] <= 'z') return (unsigned short)std::toupper(name[0]);
    // 0-9
    if (name.size() == 1 && name[0] >= '0' && name[0] <= '9') return (unsigned short)name[0];

    // F1..F24
    if (name.size() >= 2 && name[0] == 'f') {
        int fn = std::atoi(name.c_str() + 1);
        if (fn >= 1 && fn <= 24) return (unsigned short)(VK_F1 + (fn - 1));
    }

    // Tasti speciali comuni
    if (name == "enter" || name == "return") return VK_RETURN;
    if (name == "space" || name == "spacebar") return VK_SPACE;
    if (name == "tab") return VK_TAB;
    if (name == "esc" || name == "escape") return VK_ESCAPE;
    if (name == "backspace") return VK_BACK;
    if (name == "delete" || name == "del") return VK_DELETE;
    if (name == "insert" || name == "ins") return VK_INSERT;
    if (name == "home") return VK_HOME;
    if (name == "end") return VK_END;
    if (name == "pgup" || name == "pageup") return VK_PRIOR;
    if (name == "pgdn" || name == "pagedown") return VK_NEXT;
    if (name == "left") return VK_LEFT;
    if (name == "right") return VK_RIGHT;
    if (name == "up") return VK_UP;
    if (name == "down") return VK_DOWN;

    // Media keys
    if (name == "media_play_pause" || name == "playpause") return VK_MEDIA_PLAY_PAUSE;
    if (name == "media_next" || name == "next") return VK_MEDIA_NEXT_TRACK;
    if (name == "media_prev" || name == "previous" || name == "prev") return VK_MEDIA_PREV_TRACK;
    if (name == "media_stop" || name == "stop") return VK_MEDIA_STOP;

    // Volume
    if (name == "volume_up") return VK_VOLUME_UP;
    if (name == "volume_down") return VK_VOLUME_DOWN;
    if (name == "volume_mute") return VK_VOLUME_MUTE;

    // Browser/launch (facoltativi)
    if (name == "browser_back") return VK_BROWSER_BACK;
    if (name == "browser_forward") return VK_BROWSER_FORWARD;
    if (name == "browser_refresh") return VK_BROWSER_REFRESH;

    return 0; // sconosciuto
}

std::optional<HotkeyChord> ParseHotkey(const std::string& spec) {
    // Split per '+'
    HotkeyChord chord{};
    std::string token;
    token.reserve(spec.size());

    auto flushToken = [&](std::string t) {
        if (t.empty()) return;
        std::string n = toLower(t);
        if (n == "ctrl" || n == "control") { chord.modifiers.push_back(VK_CONTROL); return; }
        if (n == "alt" || n == "menu") { chord.modifiers.push_back(VK_MENU);    return; }
        if (n == "shift") { chord.modifiers.push_back(VK_SHIFT);   return; }
        if (n == "win" || n == "lwin") { chord.modifiers.push_back(VK_LWIN);    return; }
        if (n == "rwin") { chord.modifiers.push_back(VK_RWIN);    return; }
        unsigned short vk = nameToVk(n);
        if (vk != 0) chord.key = vk;
        };

    for (char c : spec) {
        if (c == ' ' || c == '\t') continue;
        if (c == '+') { flushToken(token); token.clear(); }
        else token.push_back(c);
    }
    flushToken(token);

    if (chord.key == 0) return std::nullopt;
    // Dedup modificatori
    std::sort(chord.modifiers.begin(), chord.modifiers.end());
    chord.modifiers.erase(std::unique(chord.modifiers.begin(), chord.modifiers.end()), chord.modifiers.end());
    return chord;
}

static void pushKey(std::vector<INPUT>& ins, unsigned short vk, bool down) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.wScan = 0;
    in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    in.ki.time = 0;
    in.ki.dwExtraInfo = 0;
    ins.push_back(in);
}

bool SendHotkeyTap(const HotkeyChord& chord) {
    std::vector<INPUT> ins;
    ins.reserve(chord.modifiers.size() * 2 + 2);

    // Premi i modificatori
    for (auto vk : chord.modifiers) pushKey(ins, vk, true);
    // Premi il tasto principale
    pushKey(ins, chord.key, true);
    // Rilascia il tasto principale
    pushKey(ins, chord.key, false);
    // Rilascia i modificatori in ordine inverso
    for (auto it = chord.modifiers.rbegin(); it != chord.modifiers.rend(); ++it) pushKey(ins, *it, false);

    UINT sent = SendInput((UINT)ins.size(), ins.data(), sizeof(INPUT));
    return sent == ins.size();
}
