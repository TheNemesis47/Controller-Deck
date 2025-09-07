#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include <deque>
#include <condition_variable>
#include <chrono>

// Assicurati che in premake ci sia: includedirs { "ThirdParty/cpp-httplib" }
#include <../ThirdParty/cpp-httplib/httplib.h>

class ApiServer {
public:
    using Json = nlohmann::json;

    struct Callbacks {
        // Letture
        std::function<Json()> getStateJson;
        std::function<Json()> getConfigJson;
        std::function<Json()> getVersionJson;

        // Config
        std::function<bool(const Json& j, std::string& err)> setConfigJsonStrict;   // valida + applica
        std::function<bool(const Json& j, std::string& err)> validateConfigJson;    // valida soltanto

        // Seriale
        std::function<std::vector<std::string>()> listSerialPorts;
        std::function<bool(const std::string& port, unsigned baud, std::string& err)> selectSerialPort;

        // chiusura seriale
        std::function<bool(std::string& err)> closeSerialPort;

        // processi/dispositivi aufio info
        std::function<nlohmann::json()> getAudioDevicesJson;
		std::function<nlohmann::json()> getAudioProcessesJson;
        
        std::function<nlohmann::json()> getSerialStatusJson;
        std::function<nlohmann::json()> getLayoutJson;               // /layout
        std::function<nlohmann::json()> getStateJsonVerbose;         // /state?verbose=1
        std::function<bool(nlohmann::json&, int /*timeoutMs*/)> popNextStateEvent; // SSE

        std::function<void()> requestShutdown;
    };

    ApiServer(std::string host, int port, Callbacks cbs, bool enableCORS = false);
    ~ApiServer();

    bool start();
    void stop();

private:
    void run();
    void installRoutes();

    // Envelope helpers
    void setCORSHeaders(httplib::Response& res) const;
    static void ok(httplib::Response& res, const Json& result);
    static void fail(httplib::Response& res, int status, const std::string& msg);

    std::string   m_host;
    int           m_port;
    Callbacks     m_cbs;
    bool          m_cors{ false };

    std::unique_ptr<httplib::Server> m_srv;
    std::thread       m_thr;
    std::atomic<bool> m_running{ false };
};
