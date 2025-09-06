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

    // 👇 QUI: niente ListSerialPorts “free”, chiama il wrapper dell’app
    cbs.listSerialPorts = [&app]() { return app.listSerialPorts(); };

    cbs.selectSerialPort = [&app](const std::string& port, unsigned baud, std::string& err) {
        return app.selectSerialPort(port, baud, err);
        };
    cbs.closeSerialPort = [&app](std::string& err) { return app.closeSerialPort(err); };

    cbs.getAudioDevicesJson = []() {
        return AudioDiscovery::EnumerateDevicesJson();
        };

    // 👇 QUI: cattura app per passare l’euristica fullscreen
    cbs.getAudioProcessesJson = [&app]() {
        return AudioDiscovery::EnumerateProcessesJson(
            [&app](DWORD pid) { return app.isProcessFullscreen(pid); }
        );
        };

    // se non lo imposti altrove
    cbs.getVersionJson = []() {
        return nlohmann::json{ {"app","Controller-Deck"}, {"api","1.0.0"}, {"build","dev"} };
        };

    return cbs;
}
