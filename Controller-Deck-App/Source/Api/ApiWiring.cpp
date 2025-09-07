#include "Api/ApiWiring.hpp"
#include "Api/ApiServer.hpp"
#include "utils/AudioDiscovery.hpp"
#include "utils/MainApp.hpp"

ApiServer::Callbacks ApiWiring::MakeCallbacks(MainApp& app) {
    ApiServer::Callbacks cbs;

    cbs.getStateJson = [&app]() { return app.getStateJson(); };
    cbs.getConfigJson = [&app]() { return app.getConfigJson(); };
    cbs.validateConfigJson = [&app](const nlohmann::json& j, std::string& err) { return app.validateConfigJson(j, err); };
    cbs.setConfigJsonStrict = [&app](const nlohmann::json& j, std::string& err) { return app.setConfigJsonStrict(j, err); };
    cbs.listSerialPorts = [&app]() { return app.listSerialPorts(); };
    cbs.selectSerialPort = [&app](const std::string& port, unsigned baud, std::string& err) {
        return app.selectSerialPort(port, baud, err);
        };
    cbs.closeSerialPort = [&app](std::string& err) { return app.closeSerialPort(err); };
    cbs.getAudioDevicesJson = []() {
        return AudioDiscovery::EnumerateDevicesJson();
        };
    cbs.getAudioProcessesJson = [&app]() {
        return AudioDiscovery::EnumerateProcessesJson(
            [&app](DWORD pid) { return app.isProcessFullscreen(pid); }
        );
        };
    cbs.getVersionJson = []() {
        return nlohmann::json{ {"app","Controller-Deck"}, {"api","1.0.0"}, {"build","dev"} };
        };
    cbs.getLayoutJson = [&app]() { return app.getLayoutJson(); };
    cbs.getStateJsonVerbose = [&app]() { return app.getStateJson(/*verbose*/true); };
    cbs.popNextStateEvent = [&app](nlohmann::json& out, int timeoutMs) {
        return app.popNextStateEventBlocking(out, std::chrono::milliseconds(timeoutMs));
    };
    cbs.getSerialStatusJson = [&app]() { return app.getSerialStatusJson(); };
    return cbs;
    cbs.requestShutdown = [&app]() { app.requestShutdown(); };

}
