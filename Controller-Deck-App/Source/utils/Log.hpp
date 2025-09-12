// utils/Log.hpp
#pragma once
#include <fmt/core.h>
#include <windows.h>
#include <string>

inline void LogDebugString(const std::string& s) {
    OutputDebugStringA(s.c_str());
    OutputDebugStringA("\n");
}

#if defined(_DEBUG)
#define LOGF(...) do { fmt::print(__VA_ARGS__); fmt::print("\n"); } while(0)
#else
#define LOGF(...) do { auto _s = fmt::format(__VA_ARGS__); LogDebugString(_s); } while(0)
#endif
