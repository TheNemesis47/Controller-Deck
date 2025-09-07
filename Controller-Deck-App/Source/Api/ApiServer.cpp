#include "ApiServer.hpp"
#include <fmt/core.h>
#include <regex>
#include <cstring>

using Json = nlohmann::json;

ApiServer::ApiServer(std::string host, int port, Callbacks cbs, bool enableCORS)
    : m_host(std::move(host)), m_port(port), m_cbs(std::move(cbs)), m_cors(enableCORS) {
}

ApiServer::~ApiServer() { stop(); }

bool ApiServer::start() {
    if (m_running.exchange(true)) return false;
    m_srv = std::make_unique<httplib::Server>();
    installRoutes();
    try {
        m_thr = std::thread(&ApiServer::run, this);   // <<-- può lanciare std::system_error
    }
    catch (const std::system_error& e) {
        fmt::print("[API] FATAL: cannot start server thread: {}\n", e.what());
        m_srv.reset();
        m_running.store(false);
        return false;
    }
    return true;
}

void ApiServer::stop() {
    if (!m_running.exchange(false)) return;
    if (m_srv) m_srv->stop();
    if (m_thr.joinable()) m_thr.join();
    m_srv.reset();
}

void ApiServer::run() {
    try {
        fmt::print("[API] Listening http://{}:{} (CORS: {})\n", m_host, m_port, m_cors ? "on" : "off");
        if (!m_srv->listen(m_host.c_str(), m_port)) {
            fmt::print("[API] listen() failed or stopped\n");
        }
    }
    catch (const std::exception& e) {
        fmt::print("[API] FATAL in server thread: {}\n", e.what());
    }
    catch (...) {
        fmt::print("[API] FATAL in server thread: unknown exception\n");
    }
}



void ApiServer::setCORSHeaders(httplib::Response& res) const {
    if (!m_cors) return;
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET,PUT,POST,OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

void ApiServer::ok(httplib::Response& res, const Json& result) {
    res.status = 200;
    Json env = { {"ok", true}, {"result", result} };
    res.set_content(env.dump(), "application/json");
}

void ApiServer::fail(httplib::Response& res, int status, const std::string& msg) {
    res.status = status;
    Json env = { {"ok", false}, {"error", msg} };
    res.set_content(env.dump(), "application/json");
}

void ApiServer::installRoutes() {
    m_srv->set_logger([](const httplib::Request& req, const httplib::Response& res) {
        fmt::print("[HTTP] {} {} -> {}\n", req.method, req.path, res.status);
        });

    // 404 JSON
    m_srv->set_error_handler([this](const httplib::Request&, httplib::Response& res) {
        fail(res, 404, "not_found");
        setCORSHeaders(res);
        });

    // Preflight CORS
    m_srv->Options(R"(.*)", [this](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
        setCORSHeaders(res);
        });

    // GET /health
    m_srv->Get("/health", [this](const httplib::Request&, httplib::Response& res) {
        Json payload = { {"alive", true} };
        if (m_cbs.getSerialStatusJson) {
            try { payload["serial"] = m_cbs.getSerialStatusJson(); }
            catch (...) { /* se fallisce, lasciamo solo alive */ }
        }
        ok(res, payload);
        setCORSHeaders(res);
        });


    // GET /version
    m_srv->Get("/version", [this](const httplib::Request&, httplib::Response& res) {
        if (!m_cbs.getVersionJson) { fail(res, 500, "not_available"); setCORSHeaders(res); return; }
        ok(res, m_cbs.getVersionJson());
        setCORSHeaders(res);
        });

    // GET /config
    m_srv->Get("/config", [this](const httplib::Request&, httplib::Response& res) {
        if (!m_cbs.getConfigJson) { fail(res, 500, "not_available"); setCORSHeaders(res); return; }
        ok(res, m_cbs.getConfigJson());
        setCORSHeaders(res);
        });

    // PUT /config (apply)
    m_srv->Put("/config", [this](const httplib::Request& req, httplib::Response& res) {
        if (!m_cbs.setConfigJsonStrict) { fail(res, 500, "not_available"); setCORSHeaders(res); return; }
        try {
            auto j = Json::parse(req.body);
            std::string err;
            if (!m_cbs.setConfigJsonStrict(j, err)) { fail(res, 400, err.empty() ? "invalid_config" : err); }
            else { ok(res, Json{ {"applied", true} }); }
        }
        catch (const std::exception& ex) {
            fail(res, 400, ex.what());
        }
        setCORSHeaders(res);
        });

    // PUT /config/validate (no-apply)
    m_srv->Put("/config/validate", [this](const httplib::Request& req, httplib::Response& res) {
        if (!m_cbs.validateConfigJson) { fail(res, 500, "not_available"); setCORSHeaders(res); return; }
        try {
            auto j = Json::parse(req.body);
            std::string err;
            if (!m_cbs.validateConfigJson(j, err)) { fail(res, 400, err.empty() ? "invalid_config" : err); }
            else { ok(res, Json{ {"valid", true} }); }
        }
        catch (const std::exception& ex) {
            fail(res, 400, ex.what());
        }
        setCORSHeaders(res);
        });

    // GET /state  (supporta ?verbose=1)
    m_srv->Get("/state", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            const bool verbose = (req.has_param("verbose") && req.get_param_value("verbose") == "1");
            nlohmann::json out;
            if (verbose && m_cbs.getStateJsonVerbose) out = m_cbs.getStateJsonVerbose();
            else if (m_cbs.getStateJson)             out = m_cbs.getStateJson();
            else { fail(res, 500, "not_available"); setCORSHeaders(res); return; }

            ok(res, out);
        } catch (const std::exception& e) {
            fail(res, 500, e.what());
        }
        setCORSHeaders(res);
    });

    // GET /layout
    m_srv->Get("/layout", [this](const httplib::Request&, httplib::Response& res) {
        if (!m_cbs.getLayoutJson) { fail(res, 404, "not_supported"); setCORSHeaders(res); return; }
        try {
            ok(res, m_cbs.getLayoutJson());
        } catch (const std::exception& e) {
            fail(res, 500, e.what());
        }
        setCORSHeaders(res);
    });

    // SSE: GET /events/state
    m_srv->Get("/events/state", [this](const httplib::Request&, httplib::Response& res) {
        if (!m_cbs.popNextStateEvent) { fail(res, 404, "not_supported"); setCORSHeaders(res); return; }

        res.set_header("Content-Type", "text/event-stream");
        res.set_header("Cache-Control", "no-cache, no-transform");
        res.set_header("Connection", "keep-alive");
        if (m_cors) res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("X-Accel-Buffering", "no");

        res.set_chunked_content_provider("text/event-stream",
            [this](size_t, httplib::DataSink& sink) -> bool {
                try {
                    const char* ping = ": connected\n\n";
                    sink.write(ping, std::strlen(ping));

                    if (m_cbs.getStateJsonVerbose) {
                        auto snap = m_cbs.getStateJsonVerbose();
                        std::string line = "event: snapshot\ndata: " + snap.dump() + "\n\n";
                        sink.write(line.c_str(), line.size());
                    }

                    while (sink.is_writable()) {
                        nlohmann::json ev;
                        const bool ok = m_cbs.popNextStateEvent(ev, /*timeoutMs*/1000);
                        if (ok) {
                            std::string line = "event: stateChanged\ndata: " + ev.dump() + "\n\n";
                            if (!sink.write(line.c_str(), line.size())) break;
                        }
                        else {
                            const char* hb = ": heartbeat\n\n";
                            sink.write(hb, std::strlen(hb));
                        }
                    }
                    sink.done();
                    return true;
                }
                catch (const std::exception& e) {
                    fmt::print("[SSE] provider error: {}\n", e.what());
                    try { sink.done(); }
                    catch (...) {}
                    return false;
                }
                catch (...) {
                    fmt::print("[SSE] provider unknown error\n");
                    try { sink.done(); }
                    catch (...) {}
                    return false;
                }
            },
            [](bool) {}
        );

        });


    // GET /serial/status
    m_srv->Get("/serial/status", [this](const httplib::Request&, httplib::Response& res) {
        if (!m_cbs.getSerialStatusJson) { fail(res, 500, "not_available"); setCORSHeaders(res); return; }
        ok(res, m_cbs.getSerialStatusJson());
        setCORSHeaders(res);
        });

    // GET /serial/ports
    m_srv->Get("/serial/ports", [this](const httplib::Request&, httplib::Response& res) {
        if (!m_cbs.listSerialPorts) { fail(res, 500, "not_available"); setCORSHeaders(res); return; }
        auto v = m_cbs.listSerialPorts();
        Json out = { {"ports", Json::array()} };
        for (auto& p : v) out["ports"].push_back(p);
        ok(res, out);
        setCORSHeaders(res);
        });

    // POST /serial/select
    m_srv->Post("/serial/select", [this](const httplib::Request& req, httplib::Response& res) {
        if (!m_cbs.selectSerialPort) { fail(res, 500, "not_available"); setCORSHeaders(res); return; }
        try {
            auto j = Json::parse(req.body);
            std::string port = j.value("port", "");
            unsigned baud = j.value("baud", 115200u);
            if (port.empty()) { fail(res, 400, "missing_port"); setCORSHeaders(res); return; }
            std::string err;
            if (!m_cbs.selectSerialPort(port, baud, err)) { fail(res, 400, err.empty() ? "select_failed" : err); }
            else { ok(res, Json{ {"selected", port}, {"baud", baud} }); }
        }
        catch (...) {
            fail(res, 400, "bad_json");
        }
        setCORSHeaders(res);
        });

    // POST /serial/close
    m_srv->Post("/serial/close", [this](const httplib::Request&, httplib::Response& res) {
        if (!m_cbs.closeSerialPort) { fail(res, 500, "not_available"); setCORSHeaders(res); return; }

        std::string err;
        try {
            if (m_cbs.closeSerialPort(err)) {
                ok(res, Json{ {"closed", true}, {"message", "Connessione seriale chiusa; in attesa di nuova selezione"} });
            }
            else {
                // Convenzione: err == "not_connected" -> noop/già chiuso
                if (err == "not_connected") {
                    fail(res, 409, "not_connected");
                }
                else {
                    fail(res, 400, err.empty() ? "close_failed" : err);
                }
            }
        }
        catch (const std::exception& ex) {
            fail(res, 500, ex.what());
        }
        setCORSHeaders(res);
        });

	// GET /audio/devices
    m_srv->Get("/audio/devices", [this](const httplib::Request&, httplib::Response& res) {
        if (!m_cbs.getAudioDevicesJson) { fail(res, 500, "not_available"); setCORSHeaders(res); return; }
        ok(res, m_cbs.getAudioDevicesJson());
        setCORSHeaders(res);
        });

    // GET /audio/processes
    m_srv->Get("/audio/processes", [this](const httplib::Request&, httplib::Response& res) {
        if (!m_cbs.getAudioProcessesJson) { fail(res, 500, "not_available"); setCORSHeaders(res); return; }
        ok(res, m_cbs.getAudioProcessesJson());
        setCORSHeaders(res);
        });

    // POST /control/shutdown  { "confirm": "SHUTDOWN" }
    m_srv->Post("/control/shutdown", [this](const httplib::Request& req, httplib::Response& res) {
        if (!m_cbs.requestShutdown) { fail(res, 500, "not_available"); setCORSHeaders(res); return; }
        try {
            Json j = Json::parse(req.body);
            const std::string confirm = j.value("confirm", "");
            if (confirm != "SHUTDOWN") { fail(res, 400, "confirmation_required"); setCORSHeaders(res); return; }
            ok(res, Json{ {"shutting_down", true} }); setCORSHeaders(res);
            std::thread([cb = m_cbs.requestShutdown] {
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(100ms);
                cb();
                }).detach();
        }
        catch (...) {
            fail(res, 400, "bad_json"); setCORSHeaders(res);
        }
        });

    // alias GET dal browser: /control/quit?confirm=SHUTDOWN
    m_srv->Get("/control/quit", [this](const httplib::Request& req, httplib::Response& res) {
        if (!m_cbs.requestShutdown) { fail(res, 500, "not_available"); setCORSHeaders(res); return; }
        const std::string confirm = req.has_param("confirm") ? req.get_param_value("confirm") : "";
        if (confirm != "SHUTDOWN") { fail(res, 400, "confirmation_required"); setCORSHeaders(res); return; }
        ok(res, Json{ {"shutting_down", true} }); setCORSHeaders(res);
        std::thread([cb = m_cbs.requestShutdown] {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(100ms);
            cb();
            }).detach();
        });

    // GET /__routes  -> per vedere se questa build è effettiva
    m_srv->Get("/__routes", [this](const httplib::Request&, httplib::Response& res) {
        ok(res, Json{
            {"routes", Json::array({
                "/health", "/version", "/config", "/state", "/layout",
                "/events/state", "/serial/ports", "/serial/select", "/serial/close",
                "/audio/devices", "/audio/processes",
                "/control/shutdown (POST)", "/control/quit (GET)"
            })}
            });
        setCORSHeaders(res);
        });

    // GET /__whoami  -> tag build (data/ora della TUA installRoutes)
    m_srv->Get("/__whoami", [this](const httplib::Request&, httplib::Response& res) {
        ok(res, Json{ {"build", std::string(__DATE__) + " " + __TIME__} });
        setCORSHeaders(res);
        });

}
