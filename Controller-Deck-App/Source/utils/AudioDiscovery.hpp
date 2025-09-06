#pragma once
#include <nlohmann/json.hpp>
#include <Windows.h>
#include <functional>

namespace AudioDiscovery {
    nlohmann::json EnumerateDevicesJson();
    nlohmann::json EnumerateProcessesJson(
        std::function<bool(DWORD)> isFullscreen   // <<--- CAMBIA QUI
    );
}
