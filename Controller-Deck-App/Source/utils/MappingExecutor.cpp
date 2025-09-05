#include "utils/MappingExecutor.hpp"
#include "Core/Actions/TextInput.hpp"
#include <cmath>
#include <windows.h> // Sleep

void MappingExecutor::preapply(const AppConfig& cfg, AudioController& master, AudioSessionController& sessions, const DeckState& initial) {
    for (int i = 0; i < 5; ++i) {
        if (!cfg.sliderMap[i].has_value()) continue;
        const auto& tgt = *cfg.sliderMap[i];
        float v01 = initial.sliders[i] / 1023.0f;

        if (tgt.isMaster) {
            master.setMasterVolume(v01);
        }
        else {
            for (const auto& exe : tgt.exes) {
                sessions.setAppVolume(exe, v01);
            }
        }
    }
}

void MappingExecutor::applyChanges(const AppConfig& cfg, AudioController& master, AudioSessionController& sessions, const DeckState& s, DeckState& prev) {
    // SLIDERS → volume
    for (int i = 0; i < 5; ++i) {
        if (!cfg.sliderMap[i].has_value()) continue;

        float v01 = s.sliders[i] / 1023.0f;
        float p01 = prev.sliders[i] / 1023.0f;
        if (std::abs(v01 - p01) < m_sliderDeltaThreshold) continue;

        const auto& tgt = *cfg.sliderMap[i];
        if (tgt.isMaster) {
            master.setMasterVolume(v01);
        }
        else {
            for (const auto& exe : tgt.exes) {
                sessions.setAppVolume(exe, v01);
            }
        }
    }

    // BUTTONS → azioni (in ordine) sul rising edge
    for (int i = 0; i < 5; ++i) {
        bool was = prev.buttons[i];
        bool now = s.buttons[i];
        if (!was && now) {
            for (const auto& act : cfg.buttonActions[i]) {
                switch (act.kind) {
                case BtnActKind::ToggleMuteMaster:
                    master.toggleMasterMute();
                    break;
                case BtnActKind::ToggleMuteApp:
                    sessions.toggleAppMute(act.payload);
                    break;
                case BtnActKind::Hotkey:
                case BtnActKind::Media:
                    SendHotkeyTap(act.chord);
                    break;
                case BtnActKind::Text:
                    SendTextUTF8(act.payload);
                    break;
                case BtnActKind::Delay:
                    Sleep(act.delayMs);
                    break;
                }
            }
        }
    }

    prev = s; // aggiorna "prev" a fine ciclo
}