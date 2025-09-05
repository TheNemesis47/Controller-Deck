#include "Core/Actions/TextInput.hpp"
#include <windows.h>
#include <vector>

// Converte UTF-8 -> UTF-16 con WinAPI
static bool Utf8ToUtf16(const std::string& in, std::u16string& out) {
    int n = MultiByteToWideChar(CP_UTF8, 0, in.c_str(), (int)in.size(), nullptr, 0);
    if (n <= 0) return false;
    std::wstring w; w.resize(n);
    n = MultiByteToWideChar(CP_UTF8, 0, in.c_str(), (int)in.size(), w.data(), n);
    if (n <= 0) return false;
    out.assign((char16_t*)w.data(), (char16_t*)(w.data() + w.size()));
    return true;
}

bool SendTextUTF8(const std::string& text) {
    std::u16string u16;
    if (!Utf8ToUtf16(text, u16)) return false;

    std::vector<INPUT> ins;
    ins.reserve(u16.size() * 2);

    for (char16_t ch : u16) {
        // key down (Unicode)
        INPUT down{}; down.type = INPUT_KEYBOARD;
        down.ki.wVk = 0;
        down.ki.wScan = (WORD)ch;
        down.ki.dwFlags = KEYEVENTF_UNICODE;
        ins.push_back(down);
        // key up (Unicode)
        INPUT up = down; up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        ins.push_back(up);
    }

    UINT sent = SendInput((UINT)ins.size(), ins.data(), sizeof(INPUT));
    return sent == ins.size();
}
