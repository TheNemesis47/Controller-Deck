#pragma once
#include <optional>
#include <string>
#include "DeckState.hpp"

// Parsea una linea nel formato "v0|v1|v2|v3|v4|mask"
// - Compatibile col DeeJ (se arrivano solo 5 valori → mask=0)
// - v* in [0..1023], mask in [0..31] (bit0..bit4 = B1..B5)
std::optional<DeckState> ParseDeckLine(const std::string& line);
#include "MessageParser.hpp"
#include <vector>
#include <charconv>
#include <algorithm>

static inline std::string trim(const std::string& s) {
    auto b = s.find_first_not_of(" \t\r\n");
    auto e = s.find_last_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    return s.substr(b, e - b + 1);
}

static bool toInt(const std::string& s, int& out) {
    auto sv = std::string_view(s);
    auto first = sv.data();
    auto last = sv.data() + sv.size();
    std::from_chars_result r = std::from_chars(first, last, out);
    return r.ec == std::errc() && r.ptr == last;
}

std::optional<DeckState> ParseDeckLine(const std::string& line) {
    // split su '|'
    std::vector<std::string> tok;
    std::string cur;
    cur.reserve(line.size());
    for (char c : line) {
        if (c == '\n' || c == '\r') continue;
        if (c == '|') { tok.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    if (!cur.empty()) tok.push_back(cur);

    if (tok.size() != 5 && tok.size() != 6) return std::nullopt;

    DeckState st{};
    // sliders
    for (int i = 0; i < 5; ++i) {
        int v = 0;
        if (!toInt(trim(tok[i]), v)) return std::nullopt;
        if (v < 0) v = 0;
        if (v > 1023) v = 1023;
        st.sliders[i] = v;
    }

    // mask
    int mask = 0;
    if (tok.size() == 6) {
        if (!toInt(trim(tok[5]), mask)) return std::nullopt;
        if (mask < 0) mask = 0;
        if (mask > 31) mask = 31;
    }
    for (int i = 0; i < 5; ++i) st.buttons[i] = ((mask >> i) & 1) != 0;

    return st;
}
