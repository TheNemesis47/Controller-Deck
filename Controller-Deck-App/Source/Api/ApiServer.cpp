#include "ApiServer.hpp"
#include <fmt/core.h>
#include <regex>

using Json = nlohmann::json;

ApiServer::ApiServer(std::string host, int port, Callbacks cbs, bool enableCORS)
    : m_host(std::move(host)), m_port(port), m_cbs(std::move(cbs)), m_cors(enableCORS) {
}

ApiServer::~ApiServer() { stop(); }

bool ApiServer::start() {
    if (m_running.exchange(true)) return false;
    m_srv = std::make_unique<httplib::Server>();
    installRoutes();
    m_thr = std::thread(&ApiServer::run, this);
    return true;
}

void ApiServer::stop() {
    if (!m_running.exchange(false)) return;
    if (m_srv) m_srv->stop();
    if (m_thr.joinable()) m_thr.join();
    m_srv.reset();
}

void ApiServer::run() {
    fmt::print("[API] Listening http://{}:{} (CORS: {})\n", m_host, m_port, m_cors ? "on" : "off");
    m_srv->listen(m_host.c_str(), m_port);
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
        ok(res, Json{ {"alive", true} });
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
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");
        if (m_cors) res.set_header("Access-Control-Allow-Origin", "*"); // coerente con CORS glob.

        res.set_chunked_content_provider("text/event-stream",
            // on data
            [this](size_t, httplib::DataSink& sink) {
                sink.write(": connected\n\n");
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
                    } else {
                        sink.write(": heartbeat\n\n"); // keep-alive
                    }
                }
                sink.done();
                return true;
            },
            // on done
            [](bool) {}
        );
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

}
