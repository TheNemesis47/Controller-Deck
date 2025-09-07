#include "Core/Serial/InputSmoother.hpp"

InputSmoother::InputSmoother() {
    setParamsAll(FaderSmoothingParams{}); // default
}

void InputSmoother::setParams(int i, FaderSmoothingParams p) {
    if (i < 0 || i >= 5) return;
    if (p.deadband_counts < 0) p.deadband_counts = 0;
    if (p.alpha < 0.f) p.alpha = 0.f;
    if (p.alpha > 1.f) p.alpha = 1.f;
    m_params[static_cast<size_t>(i)] = p;
}

void InputSmoother::setParamsAll(FaderSmoothingParams p) {
    for (int i = 0; i < 5; ++i) setParams(i, p);
}

void InputSmoother::reset() {
    m_hasLast.fill(false);
    m_lastFiltered.fill(0);
}

void InputSmoother::apply(DeckState& s) {
    for (int i = 0; i < 5; ++i) {
        const auto  raw = s.sliders[i];
        auto& lastF = m_lastFiltered[static_cast<size_t>(i)];
        auto& has = m_hasLast[static_cast<size_t>(i)];
        const auto& prm = m_params[static_cast<size_t>(i)];

        if (!has) {
            // primo campione: “aggancia” per evitare salto iniziale
            lastF = raw;
            has = true;
            s.sliders[i] = raw;
            continue;
        }

        // 1) DEADBAND (in conteggi): ignora micro variazioni
        const int delta = raw - lastF;
        const int ad = delta >= 0 ? delta : -delta;
        const int step = (ad <= prm.deadband_counts) ? 0 : delta;
        const int target = lastF + step;

        // 2) SMOOTH (EMA): new = last + alpha*(target-last)
        const float lf = static_cast<float>(lastF);
        const float tg = static_cast<float>(target);
        const float nf = lf + prm.alpha * (tg - lf);

        lastF = static_cast<int>(nf + (nf >= 0 ? 0.5f : -0.5f)); // arrotonda
        s.sliders[i] = lastF;
    }
}
