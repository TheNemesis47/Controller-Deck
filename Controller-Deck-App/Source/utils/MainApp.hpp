#pragma once
#include <string>
#include <mutex>
#include <memory>
#include <nlohmann/json.hpp>
#include <chrono>
#include <atomic>
#include "utils/Config.hpp"
#include "utils/MappingExecutor.hpp"
#include "Core/Audio/AudioController.hpp"
#include "Core/Audio/AudioSessionController.hpp"
#include "Core/Serial/InputSmoother.hpp"
#include "Core/Audio/AudioEndpointController.hpp"
#include "api/ApiServer.hpp"

// Nuovo: servizi estratti
#include "utils/EventBus.hpp"
#include "utils/SerialService.hpp"

class MainApp {
public:
    explicit MainApp(std::string configPath = "Source/config.json");
    int run();

    // ---- API (invariata) ----
    nlohmann::json getStateJson();                                      // /state
    nlohmann::json getConfigJson();                                     // /config (GET)
    bool validateConfigJson(const nlohmann::json& j, std::string& err); // /config/validate (PUT)
    bool setConfigJsonStrict(const nlohmann::json& j, std::string& err);// /config (PUT)
    bool selectSerialPort(const std::string& port, unsigned baud, std::string& err); // /serial/select (POST)
    bool closeSerialPort(std::string& err);                              // /serial/close (POST)
    std::vector<std::string> listSerialPorts();
    void requestShutdown();

    bool isProcessFullscreen(unsigned long pid) const;

    nlohmann::json getSerialStatusJson();
    [[nodiscard]] nlohmann::json getLayoutJson() const;
    [[nodiscard]] nlohmann::json getStateJson(bool verbose) const; // /state?verbose=1

    // Event bus per SSE (wrappa EventBus)
    void publishStateChange(const nlohmann::json& ev);
    bool popNextStateEventBlocking(nlohmann::json& out, std::chrono::milliseconds timeout);

    // --- API lato audio device ---
    bool selectAudioDeviceById(const std::string& idUtf8, std::string& err);
    bool setAudioDeviceVolume(float scalar01, std::string& err);

private:
    // ---- setup di base ----
    bool loadConfigStrictOrDie();
    bool initControllersOrDie(const std::string& port, unsigned baud);
    std::string pickPortAuto();
    InputSmoother  m_smoother;

    // ---- stato app ----
    std::string m_configPath;
    AppConfig   m_cfg;

    std::atomic<bool> m_shouldExit{ false };

    // componenti runtime
    SerialService          m_serial;    // << prima era SerialController* + mutex
    AudioController        m_master;
    AudioSessionController m_sessions;
    MappingExecutor        m_mapper{ 0.01f };
    AudioEndpointController m_deviceCtrl;

    // API
    std::unique_ptr<ApiServer> m_api;

    // sync config
    std::mutex m_cfgMtx;

    // Event bus estratto
    EventBus m_events;

    // Helpers
    static std::string NowIsoUtc();
    static bool IsProcessLikelyFullscreen(unsigned long pid);
};
