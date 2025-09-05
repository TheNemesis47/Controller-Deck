#pragma once
#include <array>
#include <cstdint>
#include <mutex>

// Rappresenta lo stato "grezzo" letto dalla seriale
struct DeckState {
    std::array<int, 5> sliders{};   // 0..1023
    std::array<bool, 5> buttons{};  // true = premuto

    // Bitmask dei bottoni (bit 0 = B1, bit 4 = B5)
    uint32_t buttonsMask() const {
        uint32_t m = 0;
        for (int i = 0; i < 5; ++i) if (buttons[i]) m |= (1u << i);
        return m;
    }
};

// Storage thread-safe dell'ultimo stato
class DeckStateStore {
public:
    // Sostituisce lo stato corrente
    void set(const DeckState& s) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = s;
    }

    // Restituisce una copia dello stato corrente
    DeckState get() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_state;
    }

    // Aggiorna solo se ci sono cambi significativi (riduce lo spam)
    // Ritorna true se qualcosa è cambiato.
    bool updateIfChanged(const DeckState& s, int sliderThreshold = 2) {
        std::lock_guard<std::mutex> lock(m_mutex);
        bool changed = false;

        // Bottoni: qualsiasi variazione è "cambio"
        for (int i = 0; i < 5; ++i) {
            if (m_state.buttons[i] != s.buttons[i]) {
                m_state.buttons[i] = s.buttons[i];
                changed = true;
            }
        }

        // Slider: applica soglia per il rumore
        for (int i = 0; i < 5; ++i) {
            if (std::abs(m_state.sliders[i] - s.sliders[i]) >= sliderThreshold) {
                m_state.sliders[i] = s.sliders[i];
                changed = true;
            }
        }

        return changed;
    }

private:
    mutable std::mutex m_mutex;
    DeckState m_state{};
};
