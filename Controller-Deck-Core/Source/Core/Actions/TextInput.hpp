#pragma once
#include <string>

// Invia testo Unicode (UTF-8) alla finestra attiva usando SendInput/KEYEVENTF_UNICODE.
// Ritorna true se tutti i caratteri sono stati inviati.
bool SendTextUTF8(const std::string& text);
