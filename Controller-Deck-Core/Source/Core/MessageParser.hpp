#pragma once
#include <optional>
#include <string>
#include "DeckState.hpp"

// Parsea una linea nel formato "v0|v1|v2|v3|v4|mask"
// - Compatibile col DeeJ (se arrivano solo 5 valori → mask=0)
// - v* in [0..1023], mask in [0..31] (bit0..bit4 = B1..B5)
std::optional<DeckState> ParseDeckLine(const std::string& line);
