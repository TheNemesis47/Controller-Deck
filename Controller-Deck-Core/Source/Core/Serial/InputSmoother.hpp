#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include "Core/DeckState.hpp"   // ha sliders[5], buttons[5]

struct FaderSmoothingParams {
    int   deadband_counts = 2;   // min delta per accettare variazione (anti jitter)
    float alpha = 0.20f;         // 0..1, filtro EMA (0 = no movimento, 1 = nessun filtro)
};

class InputSmoother {
public:
    InputSmoother();

    // Imposta parametri per singolo fader (0..4)
    void setParams(int index, FaderSmoothingParams p);

    // Imposta parametri uguali per tutti
    void setParamsAll(FaderSmoothingParams p);

    // Reset dello stato interno (usa al cambio porta/boot)
    void reset();

    // Applica deadband + smoothing IN-PLACE ai soli sliders
    void apply(DeckState& s);

private:
    std::array<int, 5>   m_lastFiltered{}; // ultimo valore filtrato
    std::array<bool, 5>  m_hasLast{};
    std::array<FaderSmoothingParams, 5> m_params;
};
