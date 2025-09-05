#pragma once
#include <string>
#include "utils/Config.hpp"

// Carica e valida il JSON (strict). Ritorna true se valido.
// "configPath" può essere, ad esempio, "Source/config.json" o "config.json".
bool LoadConfigStrict(AppConfig& cfg, std::string& outErr, const std::string& configPath);