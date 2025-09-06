#pragma once
#include <string>
#include <mutex>
#include <memory>
#include <nlohmann/json.hpp>

#include "utils/Config.hpp"
#include "utils/MappingExecutor.hpp"
#include "Core/Serial/SerialController.hpp"
#include "Core/Audio/AudioController.hpp"
#include "Core/Audio/AudioSessionController.hpp"
#include "Api/ApiServer.hpp"

class MainApp {
public:
    explicit MainApp(std::string configPath = "Source/config.json");
    int run();

    // ---- Thin wrappers usati dal wiring API ----
    nlohmann::json getStateJson();                                      // /state
    nlohmann::json getConfigJson();                                     // /config (GET)
    bool validateConfigJson(const nlohmann::json& j, std::string& err); // /config/validate (PUT)
    bool setConfigJsonStrict(const nlohmann::json& j, std::string& err);// /config (PUT)
    bool selectSerialPort(const std::string& port, unsigned baud, std::string& err); // /serial/select (POST)
    bool closeSerialPort(std::string& err);                              // /serial/close(POST)
    std::vector<std::string> listSerialPorts();

    // Euristica “fullscreen” esposta al wiring audio/processes
    bool isProcessFullscreen(unsigned long pid) const;

private:
    // ---- setup di base ----
    bool loadConfigStrictOrDie();
    bool initControllersOrDie(const std::string& port, unsigned baud);
    std::string pickPortAuto();

    // ---- stato app ----
    std::string m_configPath;
    AppConfig   m_cfg;

    // componenti runtime
    SerialController* m_serial = nullptr;
    AudioController         m_master;
    AudioSessionController  m_sessions;
    MappingExecutor         m_mapper{ 0.01f };

    // API
    std::unique_ptr<ApiServer> m_api;

    // sync (proteggono file config e seriale)
    std::mutex m_cfgMtx;
    std::mutex m_serialMtx;

    // ---- helper interni ----
    static bool IsProcessLikelyFullscreen(unsigned long pid);
};
