#include "utils/MainApp.hpp"
#include "utils/ConfigLoader.hpp"
#include "utils/Utils.hpp"
#include "utils/ProcessUtils.hpp"       
#include "utils/AudioDiscovery.hpp"     
#include "../Api/ApiWiring.hpp" 

#include <fmt/core.h>
#include <Windows.h>
#include <fstream>
#include <cstdio>
#include <vector>

// solo per auto-pick porta
#include "Core/Serial/SerialPortEnumerator.hpp"

using Json = nlohmann::json;

// -----------------------------------------------------------------------------
// Helpers thread-safe (locali al file)
static DeckState ReadDeckStateSafe(SerialController* sc, std::mutex& serialMtx) {
    std::lock_guard<std::mutex> lk(serialMtx);
    if (!sc) return DeckState{};
    return sc->store().get();
}
static bool IsSerialConnected(SerialController* sc, std::mutex& serialMtx) {
    std::lock_guard<std::mutex> lk(serialMtx);
    return sc != nullptr;
}
std::vector<std::string> MainApp::listSerialPorts() {
    return ListSerialPorts();
}
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
    return ports.back(); // la più “recente”
}

// Euristica fullscreen per “giochi”
bool MainApp::IsProcessLikelyFullscreen(unsigned long pid) {
    struct Ctx { DWORD pid; bool fs = false; RECT mon{}; } ctx{ (DWORD)pid };
    ctx.mon = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };

    EnumWindows([](HWND h, LPARAM lp)->BOOL {
        auto* c = reinterpret_cast<Ctx*>(lp);
        DWORD wpid = 0; GetWindowThreadProcessId(h, &wpid);
        if (wpid != c->pid) return TRUE;
        if (!IsWindowVisible(h) || IsIconic(h)) return TRUE;
        RECT r{}; if (!GetWindowRect(h, &r)) return TRUE;
        if (abs((r.right - r.left) - (c->mon.right - c->mon.left)) <= 2 &&
            abs((r.bottom - r.top) - (c->mon.bottom - c->mon.top)) <= 2) {
            c->fs = true; return FALSE;
        }
        return TRUE;
        }, reinterpret_cast<LPARAM>(&ctx));

    return ctx.fs;
}
bool MainApp::isProcessFullscreen(unsigned long pid) const {
    return MainApp::IsProcessLikelyFullscreen(pid);
}

bool MainApp::initControllersOrDie(const std::string& port, unsigned baud) {
    // Seriale
    {
        std::lock_guard<std::mutex> lk(m_serialMtx);
        m_serial = new SerialController(port, baud);
        m_serial->start();
    }
    fmt::print("Seriale {} @ {} avviata.\n", port, baud);

    // Audio master
    if (!m_master.init()) {
        fmt::print("Audio master init fallita.\n");
        WaitForEnterAndExit(4);
        return false;
    }

    // Audio sessions
    if (!m_sessions.init()) {
        fmt::print("Audio session init fallita.\n");
        WaitForEnterAndExit(5);
        return false;
    }
    return true;
}

// -------------------- Thin wrappers per ApiWiring --------------------

Json MainApp::getStateJson() {
    DeckState s = ReadDeckStateSafe(m_serial, m_serialMtx);
    return Json{
        {"sliders", { s.sliders[0], s.sliders[1], s.sliders[2], s.sliders[3], s.sliders[4] }},
        {"buttons", { s.buttons[0], s.buttons[1], s.buttons[2], s.buttons[3], s.buttons[4] }}
    };
}

Json MainApp::getConfigJson() {
    std::lock_guard<std::mutex> lock(m_cfgMtx);
    try {
        std::ifstream f(m_configPath, std::ios::binary);
        if (!f) return Json{ {"error","cannot_open_config"} };
        Json j; f >> j; return j;
    }
    catch (...) {
        return Json{ {"error","read_config_failed"} };
    }
}

bool MainApp::validateConfigJson(const Json& j, std::string& err) {
    // valida NO-APPLY usando file temporaneo
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
    std::remove(tmp.c_str());
    return ok;
}

bool MainApp::setConfigJsonStrict(const Json& j, std::string& err) {
    std::lock_guard<std::mutex> lock(m_cfgMtx);
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

    // Pre-applica i volumi secondo il nuovo mapping solo se abbiamo dati validi
    if (IsSerialConnected(m_serial, m_serialMtx)) {
        auto isLikelyUninitialized = [](const DeckState& s) {
            for (int i = 0; i < 5; ++i) if (s.sliders[i] != 0) return false;
            return true;
            };
        DeckState cur = ReadDeckStateSafe(m_serial, m_serialMtx);
        if (!isLikelyUninitialized(cur)) {
            m_mapper.preapply(m_cfg, m_master, m_sessions, cur);
        }
    }
    return true;
}

bool MainApp::selectSerialPort(const std::string& newPort, unsigned baud, std::string& err) {
    std::lock_guard<std::mutex> lk(m_serialMtx);
    try {
        if (m_serial) { m_serial->stop(); delete m_serial; m_serial = nullptr; }
        m_serial = new SerialController(newPort, baud);
        m_serial->start();

        // aggiorna config (RAM + persistenza)
        {
            std::lock_guard<std::mutex> ck(m_cfgMtx);
            m_cfg.port = newPort;
            m_cfg.baud = baud;

            try {
                nlohmann::json j;
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
                // non fatale
            }
        }
        return true;
    }
    catch (...) {
        err = "open_failed";
        return false;
    }
}

bool MainApp::closeSerialPort(std::string& err) {
    std::lock_guard<std::mutex> lk(m_serialMtx);
    if (m_serial == nullptr) {
        err = "not_connected";
        return false; // coerente con 409 “not_connected”
    }
    try {
        m_serial->stop();
        delete m_serial;
        m_serial = nullptr;
        return true;
    }
    catch (const std::exception& ex) {
        err = ex.what();
        return false;
    }
    catch (...) {
        err = "unknown_error";
        return false;
    }
}

// -------------------- run() --------------------

int MainApp::run() {
    if (!loadConfigStrictOrDie()) return 2;

    std::string port = (m_cfg.port == "auto") ? pickPortAuto() : m_cfg.port;
    if (!initControllersOrDie(port, m_cfg.baud)) return 4;

    // Costruzione callbacks (spostata in ApiWiring)
    ApiServer::Callbacks cbs = ApiWiring::MakeCallbacks(*this);

    m_api = std::make_unique<ApiServer>("127.0.0.1", 8765, cbs, /*enableCORS=*/true);
    m_api->start();

    fmt::print("REST su http://127.0.0.1:8765  |  Premi ESC per uscire.\n");

    auto isLikelyUninitialized = [](const DeckState& s) {
        for (int i = 0; i < 5; ++i) if (s.sliders[i] != 0) return false;
        return true;
        };

    // Attendi (max ~800 ms) un primo campione non-zero; altrimenti niente preapply
    DeckState first = ReadDeckStateSafe(m_serial, m_serialMtx);
    DWORD start = GetTickCount();
    while (isLikelyUninitialized(first) && (GetTickCount() - start) < 800) {
        Sleep(10);
        first = ReadDeckStateSafe(m_serial, m_serialMtx);
    }

    DeckState prev = first;
    if (!isLikelyUninitialized(first)) {
        m_mapper.preapply(m_cfg, m_master, m_sessions, first);
    }

    bool running = true;
    while (running) {
        // ESC solo se la console è in foreground
        HWND fg = GetForegroundWindow();
        HWND con = GetConsoleWindow();
        if (fg == con && (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
            running = false;
        }

        // Applica mapping SOLO se la seriale è connessa
        if (IsSerialConnected(m_serial, m_serialMtx)) {
            DeckState cur = ReadDeckStateSafe(m_serial, m_serialMtx);
            m_mapper.applyChanges(m_cfg, m_master, m_sessions, cur, prev);
        }
        // altrimenti: non toccare l’audio, non aggiornare prev

        Sleep(10);
    }

    if (m_api) { m_api->stop(); m_api.reset(); }

    m_sessions.shutdown();
    m_master.shutdown();
    if (m_serial) { m_serial->stop(); delete m_serial; m_serial = nullptr; }
    return 0;
}
