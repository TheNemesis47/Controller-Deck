#pragma once
#include <string>
#include "utils/Config.hpp"
#include "utils/MappingExecutor.hpp"
#include "Core/Serial/SerialController.hpp"
#include "Core/Audio/AudioController.hpp"
#include "Core/Audio/AudioSessionController.hpp"
#include <mutex>
#include <memory>
#include <nlohmann/json.hpp>
#include "Api/ApiServer.hpp"

class MainApp {
public:
    explicit MainApp(std::string configPath = "Source/config.json");
    int run();

private:
    std::string m_configPath;
    AppConfig   m_cfg;

    // componenti runtime
    SerialController* m_serial = nullptr;
    AudioController         m_master;
    AudioSessionController  m_sessions;
    MappingExecutor         m_mapper{ 0.01f };

    // API
    std::unique_ptr<ApiServer> m_api;

    // sync
    std::mutex m_cfgMtx;
    std::mutex m_serialMtx;

    // helper esistenti
    bool loadConfigStrictOrDie();
    bool initControllersOrDie(const std::string& port, unsigned baud);
    std::string pickPortAuto();

    // --- callback per ApiServer ---
    nlohmann::json getStateJson();
    nlohmann::json getConfigJson();
    bool setConfigJsonStrict(const nlohmann::json& j, std::string& err);
    std::vector<std::string> listSerialPorts();
    bool selectSerialPort(const std::string& port, unsigned baud, std::string& err);
};