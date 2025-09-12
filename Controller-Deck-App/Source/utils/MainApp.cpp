#include "utils/MainApp.hpp"
#include "utils/ConfigLoader.hpp"
#include "utils/Utils.hpp"
#include "utils/ProcessUtils.hpp"
#include "utils/AudioDiscovery.hpp"
#include "api/ApiWiring.hpp"          // usa path coerente in minuscolo
#include "utils/TrayIcon.hpp"

#include <fmt/core.h>
#include <Windows.h>

#include <fstream>
#include <vector>
#include <cstdio>                     // std::remove
#include <ctime>
#include "Core/Serial/SerialPortEnumerator.hpp" // ListSerialPorts()

using Json = nlohmann::json;

// -----------------------------------------------------------------------------
// Helpers generali
// -----------------------------------------------------------------------------
std::string MainApp::NowIsoUtc() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto tt = system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%FT%TZ", &tm);
    return buf;
}

// elenco porte seriali disponibili
std::vector<std::string> MainApp::listSerialPorts() {
    return ListSerialPorts();
}

// -----------------------------------------------------------------------------
// Costruzione/distruzione
// -----------------------------------------------------------------------------
MainApp::MainApp(std::string configPath)
    : m_configPath(std::move(configPath)) {
}

// -----------------------------------------------------------------------------
// Caricamento config + bootstrap dei controller
// -----------------------------------------------------------------------------
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
    // euristica semplice: usa l'ultima (spesso la più “recente”/plugged)
    return ports.back();
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
    // Seriale (ora via SerialService)
    {
        std::string err;
        if (!m_serial.open(port, baud, &err)) {
            fmt::print("Seriale {} @ {} non avviata: {}.\n", port, baud, err);
            WaitForEnterAndExit(4);
            return false;
        }
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

    // Controller endpoint device specifico (default device)
    if (!m_deviceCtrl.init()) {
        fmt::print("Audio endpoint controller init fallita.\n");
    }

    return true;
}

// -----------------------------------------------------------------------------
// Thin wrappers per ApiWiring (REST)
// -----------------------------------------------------------------------------
Json MainApp::getSerialStatusJson() {
    // Nota: la porta/baud “correnti” sono in m_cfg (protetti da m_cfgMtx)
    const bool connected = m_serial.isConnected();

    std::string port;
    unsigned baud = 0;
    {   // sezione protetta per leggere la config persistita
        std::lock_guard<std::mutex> lock(m_cfgMtx);
        port = m_cfg.port;
        baud = m_cfg.baud;
    }

    Json out = { {"connected", connected} };
    if (connected) {
        out["port"] = port;
        out["baud"] = baud;
    }
    return out;
}

nlohmann::json MainApp::getStateJson(bool verbose) const {
    Json base = const_cast<MainApp*>(this)->getStateJson();
    if (!verbose) return base;

    Json out = {
        {"buttons",   base["buttons"]},
        {"sliders",   base["sliders"]},
        {"timestamp", NowIsoUtc()},
        {"meta",      { {"source","controller-deck"}, {"verbose",true} }},
        {"layout",    getLayoutJson()},
        {"serial",    const_cast<MainApp*>(this)->getSerialStatusJson()}
    };
    return out;
}

nlohmann::json MainApp::getStateJson() {
    DeckState s = m_serial.readState();
    return nlohmann::json{
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
    // Scrivi file config con lock per coerenza
    {
        std::lock_guard<std::mutex> lock(m_cfgMtx);
        try {
            std::ofstream f(m_configPath, std::ios::binary | std::ios::trunc);
            if (!f) { err = "cannot_write_config_file"; return false; }
            f << j.dump(2);
            f.close();
        }
        catch (...) { err = "cannot_write_config_file"; return false; }
    }

    // Ricarica in RAM verificando la validità
    AppConfig newCfg;
    if (!LoadConfigStrict(newCfg, err, m_configPath)) return false;

    {
        std::lock_guard<std::mutex> lock(m_cfgMtx);
        m_cfg = newCfg;
    }

    // Pre-applica i volumi secondo il nuovo mapping solo se abbiamo dati validi
    if (m_serial.isConnected()) {
        auto isLikelyUninitialized = [](const DeckState& s) {
            for (int i = 0; i < 5; ++i) if (s.sliders[i] != 0) return false;
            return true;
            };
        DeckState cur = m_serial.readState();
        if (!isLikelyUninitialized(cur)) {
            m_mapper.preapply(m_cfg, m_master, m_sessions, cur);
        }
    }
    return true;
}

bool MainApp::selectSerialPort(const std::string& newPort, unsigned baud, std::string& err) {
    if (!m_serial.open(newPort, baud, &err)) return false;

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

bool MainApp::closeSerialPort(std::string& err) {
    return m_serial.close(&err);
}

// Layout di esempio (compatibile con il FE attuale)
Json MainApp::getLayoutJson() const {
    Json controls = Json::array();
    for (int i = 0; i < 5; ++i) {
        controls.push_back({
            {"id",    fmt::format("btn_{:02d}", i + 1)},
            {"type",  "button"},
            {"label", fmt::format("Button {}", i + 1)},
            {"row",   i / 3},
            {"col",   i % 3},
            {"x",     (i % 3) * 90 + 10},
            {"y",     (i / 3) * 90 + 10},
            {"w",     80},
            {"h",     80}
            });
    }
    Json sliders = Json::array();
    for (int i = 0; i < 5; ++i) {
        sliders.push_back({
            {"id",    fmt::format("slider_{:02d}", i + 1)},
            {"type",  "slider"},
            {"label", fmt::format("Slider {}", i + 1)},
            {"orientation", "vertical"},
            {"range", {{"min",0.0},{"max",1.0},{"step",0.01}}},
            {"x", 300},
            {"y", 10 + i * 220},
            {"w", 20},
            {"h", 200}
            });
    }
    return Json{ {"controls", controls}, {"sliders", sliders} };
}

// -----------------------------------------------------------------------------
// Event Bus (SSE) — wrapper sul componente estratto
// -----------------------------------------------------------------------------
void MainApp::publishStateChange(const Json& ev) {
    m_events.publish(ev);
}

bool MainApp::popNextStateEventBlocking(Json& out, std::chrono::milliseconds timeout) {
    try {
        return m_events.popNext(out, timeout);
    }
    catch (const std::exception& e) {
        fmt::print("[EVT] wait error: {}\n", e.what());
        return false;
    }
    catch (...) {
        fmt::print("[EVT] wait unknown error\n");
        return false;
    }
}

void MainApp::requestShutdown() {
    // segnale di uscita (consumato nel loop main)
    m_shouldExit.store(true, std::memory_order_relaxed);
}

// -----------------------------------------------------------------------------
// API lato audio device
// -----------------------------------------------------------------------------
bool MainApp::selectAudioDeviceById(const std::string& idUtf8, std::string& err) {
    // Converte UTF-8 -> UTF-16
    int len = MultiByteToWideChar(CP_UTF8, 0, idUtf8.c_str(), -1, nullptr, 0);
    if (len <= 1) { err = "bad_device_id"; return false; }
    std::wstring wid(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, idUtf8.c_str(), -1, wid.data(), len);

    std::wstring werr;
    if (!m_deviceCtrl.setActiveEndpointById(wid, werr)) {
        err = "select_failed";
        return false;
    }
    return true;
}

bool MainApp::setAudioDeviceVolume(float scalar01, std::string& err) {
    std::wstring werr;
    if (!m_deviceCtrl.setVolumeScalar(scalar01, werr)) {
        err = "set_volume_failed";
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------------
// run() — ciclo di vita principale dell'app
// -----------------------------------------------------------------------------
int MainApp::run() {
    if (!loadConfigStrictOrDie()) return 2;

    std::string port = (m_cfg.port == "auto") ? pickPortAuto() : m_cfg.port;
    if (!initControllersOrDie(port, m_cfg.baud)) return 4;

#if !defined(_DEBUG)
    // In Release, se il target è ConsoleApp, nascondi la console
    FreeConsole();
#endif

    // smoothing dei fader (parametri iniziali)
    m_smoother.reset();
    m_smoother.setParamsAll(FaderSmoothingParams{ /*deadband*/ 2, /*alpha*/ 0.20f });

    // Callbacks REST (wiring separato)
    ApiServer::Callbacks cbs = ApiWiring::MakeCallbacks(*this);
    m_api = std::make_unique<ApiServer>("127.0.0.1", 8765, cbs, /*enableCORS=*/true);
    m_api->start();

    // Tray icon con callback di quit che fa shutdown pulito
    std::wstring uiUrl = L"http://localhost:5173/";
    auto tray = std::make_unique<TrayIcon>(
        L"Controller-Deck",
        uiUrl,
        /*onQuit=*/[this]() { this->requestShutdown(); }
    );
    tray->start();

    fmt::print("REST su http://127.0.0.1:8765  |  Premi ESC per uscire.\n");

    auto isLikelyUninitialized = [](const DeckState& s) {
        for (int i = 0; i < 5; ++i) if (s.sliders[i] != 0) return false;
        return true;
        };

    // Attendi (max ~800 ms) un primo campione non-zero; altrimenti niente preapply
    DeckState first = m_serial.readState();
    m_smoother.apply(first);
    DWORD start = GetTickCount();
    while (isLikelyUninitialized(first) && (GetTickCount() - start) < 800) {
        Sleep(10);
        first = m_serial.readState();
    }

    DeckState prev = first;
    if (!isLikelyUninitialized(first)) {
        m_mapper.preapply(m_cfg, m_master, m_sessions, first);
    }

    // Loop principale
    bool running = true;
    while (running) {
        if (m_shouldExit.load(std::memory_order_relaxed)) {
            running = false; // esce e fa teardown ordinato
            break;
        }

#ifdef _DEBUG
        // In debug: ESC per uscire quando la console è in foreground
        HWND fg = GetForegroundWindow();
        HWND con = GetConsoleWindow();
        if (fg == con && (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
            running = false;
        }
#endif

        if (m_serial.isConnected()) {
            DeckState cur = m_serial.readState();
            m_smoother.apply(cur);

            // --- Pubblica eventi per il FE ---
            // Sliders
            for (int i = 0; i < 5; ++i) {
                if (cur.sliders[i] != prev.sliders[i]) {
                    publishStateChange(Json{
                        {"type","slider"},
                        {"id",   fmt::format("slider_{:02d}", i + 1)},
                        {"value", cur.sliders[i]},
                        {"prev",  prev.sliders[i]},
                        {"timestamp", NowIsoUtc()}
                        });
                }
            }
            // Buttons
            for (int i = 0; i < 5; ++i) {
                if (cur.buttons[i] != prev.buttons[i]) {
                    publishStateChange(Json{
                        {"type","button"},
                        {"id",   fmt::format("btn_{:02d}", i + 1)},
                        {"pressed", cur.buttons[i] != 0},
                        {"prev",    prev.buttons[i] != 0},
                        {"timestamp", NowIsoUtc()}
                        });
                }
            }

            // Applica mapping e aggiorna prev
            m_mapper.applyChanges(m_cfg, m_master, m_sessions, cur, prev);
            prev = cur;
        }

        Sleep(10); // throttling minimo
    }

    // Teardown ordinato
    if (m_api) { m_api->stop(); m_api.reset(); }
    if (tray) { tray->stop(); tray.reset(); }

    m_sessions.shutdown();
    m_master.shutdown();
    std::string _; (void)m_serial.close(&_); // opzionale: garantisce chiusura immediata

    return 0;
}
