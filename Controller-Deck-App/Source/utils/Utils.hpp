#pragma once
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fmt/core.h>

// minuscole
static inline std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

// attende INVIO ed esce
static inline void WaitForEnterAndExit(int code) {
    fmt::print("Premi INVIO per uscire...\n");
    (void)getchar();
    std::exit(code);
}
