#include "utils/MainApp.hpp"
#include "utils/ConfigLoader.hpp"
#include "utils/Utils.hpp"

#include <fmt/core.h>
#include <Windows.h>

#include <nlohmann/json.hpp>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>
#include <memory>
#include <cstdio>

#include "Core/Serial/SerialPortEnumerator.hpp"
#include "api/ApiServer.hpp"     // ← server REST in directory dedicata

using Json = nlohmann::json;

// -----------------------------------------------------------------------------
// Mutex file-scope (niente modifica all'header)
static std::mutex gCfgMtx;     // protegge m_cfg e file di config
static std::mutex gSerialMtx;  // protegge restart della seriale
// -----------------------------------------------------------------------------

MainApp::MainApp(std::string configPath)
    : m_configPath(std::move(configPath)) {
}

bool MainApp::loadConfigStrictOrDie() {
    std::string cfgErr;
    if (!LoadConfigStrict(m_cfg, cfgErr, m_configPath)) {
        fmt::print("ERRORE CONFIG: {}\n", cfgErr);
        WaitForEnterAndExit(2);
        return false;
    }
    return true;
}

std::string MainApp::pickPortAuto() {
    auto ports = ListSerialPorts();
    if (ports.empty()) {
        fmt::print("Nessuna COM trovata.\n");
        WaitForEnterAndExit(3);
    }
    // scegli la più recente (o .front() se preferisci)
    return ports.back();
}

bool MainApp::initControllersOrDie(const std::string& port, unsigned baud) {
    m_serial = new SerialController(port, baud);
    m_serial->start();
    fmt::print("Seriale {} @ {} avviata.\n", port, baud);

    if (!m_master.init()) {
        fmt::print("Audio master init fallita.\n");
        WaitForEnterAndExit(4);
        return false;
    }

    if (!m_sessions.init()) {
        fmt::print("Audio session init fallita.\n");
        WaitForEnterAndExit(5);
        return false;
    }
    return true;
}

#include <cstdio> // std::remove

// ... codice precedente invariato ...

int MainApp::run() {
    if (!loadConfigStrictOrDie()) return 2;

    std::string port = (m_cfg.port == "auto") ? pickPortAuto() : m_cfg.port;
    if (!initControllersOrDie(port, m_cfg.baud)) return 4;

    ApiServer::Callbacks cbs;

    // ----- Letture -----
    cbs.getStateJson = [this]() -> Json {
        DeckState s = m_serial ? m_serial->store().get() : DeckState{};
        return Json{
            {"sliders", { s.sliders[0], s.sliders[1], s.sliders[2], s.sliders[3], s.sliders[4] }},
            {"buttons", { s.buttons[0], s.buttons[1], s.buttons[2], s.buttons[3], s.buttons[4] }}
        };
        };

    cbs.getConfigJson = [this]() -> Json {
        std::lock_guard<std::mutex> lock(gCfgMtx);
        try {
            std::ifstream f(m_configPath, std::ios::binary);
            if (!f) return Json{ {"error","cannot_open_config"} };
            Json j; f >> j; return j;
        }
        catch (...) { return Json{ {"error","read_config_failed"} }; }
        };

    cbs.getVersionJson = []() -> Json {
        // Versione semplice; se vuoi, leggi da macro o file di versione
        return Json{
            {"app", "Controller-Deck"},
            {"api", "1.0.0"},
            {"build", "dev"}
        };
        };

    // ----- Config -----
    cbs.validateConfigJson = [this](const Json& j, std::string& err) -> bool {
        // valida NO-APPLY usando un file temporaneo
        const std::string tmp = m_configPath + ".validate.tmp.json";
        try {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            if (!f) { err = "cannot_write_temp_file"; return false; }
            f << j.dump(2);
            f.close();
        }
        catch (...) { err = "cannot_write_temp_file"; return false; }

        AppConfig testCfg;
        bool ok = LoadConfigStrict(testCfg, err, tmp);
        std::remove(tmp.c_str()); // pulizia best-effort
        return ok;
        };

    cbs.setConfigJsonStrict = [this](const Json& j, std::string& err) -> bool {
        std::lock_guard<std::mutex> lock(gCfgMtx);
        try {
            std::ofstream f(m_configPath, std::ios::binary | std::ios::trunc);
            if (!f) { err = "cannot_write_config_file"; return false; }
            f << j.dump(2);
            f.close();
        }
        catch (...) { err = "cannot_write_config_file"; return false; }

        AppConfig newCfg;
        if (!LoadConfigStrict(newCfg, err, m_configPath)) return false;

        m_cfg = newCfg;

        // Pre-applica i volumi secondo il nuovo mapping
        DeckState cur = m_serial ? m_serial->store().get() : DeckState{};
        m_mapper.preapply(m_cfg, m_master, m_sessions, cur);
        return true;
        };

    // ----- Seriale -----
    cbs.listSerialPorts = []() -> std::vector<std::string> {
        return ListSerialPorts(); // implementa anche rami macOS/Linux
        };

    cbs.selectSerialPort = [this](const std::string& newPort, unsigned baud, std::string& err) -> bool {
        std::lock_guard<std::mutex> lk(gSerialMtx);
        try {
            if (m_serial) { m_serial->stop(); delete m_serial; m_serial = nullptr; }
            m_serial = new SerialController(newPort, baud);
            m_serial->start();

            // aggiorna config in RAM + persistenza su file (senza usare cbs)
            {
                std::lock_guard<std::mutex> ck(gCfgMtx);
                m_cfg.port = newPort;
                m_cfg.baud = baud;

                try {
                    nlohmann::json j;

                    // leggi file esistente (se c’è), altrimenti crea un oggetto vuoto
                    {
                        std::ifstream fin(m_configPath, std::ios::binary);
                        if (fin) fin >> j; else j = nlohmann::json::object();
                    }

                    if (!j.contains("serial") || !j["serial"].is_object())
                        j["serial"] = nlohmann::json::object();

                    j["serial"]["port"] = newPort;
                    j["serial"]["baud"] = baud;

                    std::ofstream fout(m_configPath, std::ios::binary | std::ios::trunc);
                    fout << j.dump(2);
                }
                catch (...) {
                    // non fatale: la seriale è comunque stata aperta
                }
            }
            return true;
        }
        catch (...) {
            err = "open_failed";
            return false;
        }
        };


    // ----- Azioni -----
    cbs.pressButton = [this](int idx, std::string& err) -> bool {
        if (idx < 0 || idx >= 5) { err = "index_out_of_range"; return false; }

        // Prendi lo stato corrente dalla seriale (se assente, usa default)
        DeckState cur = m_serial ? m_serial->store().get() : DeckState{};
        DeckState prev = cur;

        // Simula un rising edge: prima false -> poi true
        prev.buttons[idx] = false;
        cur.buttons[idx] = true;

        try {
            // Applica il mapping esattamente come nel loop principale
            m_mapper.applyChanges(m_cfg, m_master, m_sessions, cur, prev);
            return true;
        }
        catch (...) {
            err = "exec_failed";
            return false;
        }
        };


    cbs.applySliderRaw = [this](int idx, int raw, std::string& err) -> bool {
        if (idx < 0 || idx >= 5) { err = "index_out_of_range"; return false; }
        if (raw < 0 || raw > 1023) { err = "value_out_of_range"; return false; }

        // Stato corrente e precedente
        DeckState cur = m_serial ? m_serial->store().get() : DeckState{};
        DeckState prev = cur;

        // Cambia solo lo slider indicato
        cur.sliders[idx] = raw;

        try {
            m_mapper.applyChanges(m_cfg, m_master, m_sessions, cur, prev);
            return true;
        }
        catch (...) {
            err = "exec_failed";
            return false;
        }
        };


    auto api = std::make_unique<ApiServer>("127.0.0.1", 8765, cbs, /*enableCORS=*/true);
    api->start();

    fmt::print("Lettura in corso. REST su http://127.0.0.1:8765  |  Premi ESC per uscire.\n");

    DeckState prev = m_serial->store().get();
    m_mapper.preapply(m_cfg, m_master, m_sessions, prev);

    bool running = true;
    while (running) {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) running = false;

        DeckState cur = m_serial->store().get();
        m_mapper.applyChanges(m_cfg, m_master, m_sessions, cur, prev);

        Sleep(10);
    }

    api->stop();
    api.reset();

    m_sessions.shutdown();
    m_master.shutdown();
    if (m_serial) { m_serial->stop(); delete m_serial; m_serial = nullptr; }
    return 0;
}
