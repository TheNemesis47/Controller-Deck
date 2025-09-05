#pragma once
#include "utils/Config.hpp"
#include "Core/DeckState.hpp"
#include "Core/Audio/AudioController.hpp"
#include "Core/Audio/AudioSessionController.hpp"

// Applica i mapping di slider/bottoni ai controller audio
class MappingExecutor {
public:
    // soglie in percentuale (0..1)
    explicit MappingExecutor(float sliderDeltaThreshold = 0.01f)
        : m_sliderDeltaThreshold(sliderDeltaThreshold) {
    }

    // Applica lo stato iniziale
    void preapply(const AppConfig& cfg, AudioController& master, AudioSessionController& sessions, const DeckState& initial);

    // Applica differenze (usa prev per edge detection e delta slider)
    void applyChanges(const AppConfig& cfg, AudioController& master, AudioSessionController& sessions, const DeckState& current, DeckState& prev);

private:
    float m_sliderDeltaThreshold;
};
