#pragma once
#include <string>
#include <array>
#include <vector>
#include <optional>
#include "Core/Actions/Hotkey.hpp"

// Target di uno slider
struct SliderTarget {
    bool isMaster = false;                 // true -> controlla master volume
    std::vector<std::string> exes;         // altrimenti elenco exe (lower-case)
};

// Tipi di azione per i bottoni (in ordine di esecuzione)
enum class BtnActKind {
    ToggleMuteMaster,     // toggle mute sul master
    ToggleMuteApp,        // toggle mute su app specifica (payload = exe)
    Hotkey,               // invia un chord (CTRL+V, F5, MEDIA_PLAY_PAUSE, ...)
    Text,                 // scrive testo (Unicode)
    Delay,                // attende N ms (delayMs)
    Media                 // alias: convertito internamente in Hotkey
};

struct ButtonAction {
    BtnActKind     kind{};
    std::string    payload;     // exe per ToggleMuteApp, testo per Text, nome media per Media
    HotkeyChord    chord{};     // valido per Hotkey/Media
    unsigned       delayMs = 0; // valido per Delay
};

struct AppConfig {
    std::string port = "auto";
    unsigned baud = 115200;

    // SLIDERS (5 canali)
    std::array<std::optional<SliderTarget>, 5> sliderMap;

    // BUTTONS (5 canali) — lista ordinata di azioni
    std::array<std::vector<ButtonAction>, 5> buttonActions;
};
