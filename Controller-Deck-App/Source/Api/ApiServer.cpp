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

    // GET /state
    m_srv->Get("/state", [this](const httplib::Request&, httplib::Response& res) {
        if (!m_cbs.getStateJson) { fail(res, 500, "not_available"); setCORSHeaders(res); return; }
        ok(res, m_cbs.getStateJson());
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

    // POST /actions/button/{i}
    m_srv->Post(R"(/actions/button/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        if (!m_cbs.pressButton) { fail(res, 500, "not_available"); setCORSHeaders(res); return; }
        int idx = std::stoi(req.matches[1]);
        std::string err;
        if (!m_cbs.pressButton(idx, err)) { fail(res, 400, err.empty() ? "action_failed" : err); }
        else { ok(res, Json{ {"pressed", idx} }); }
        setCORSHeaders(res);
        });

    // POST /actions/slider/{i}  body: {"value": 0..1023}  (accettiamo anche 0..1 float: viene scalato)
    m_srv->Post(R"(/actions/slider/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        if (!m_cbs.applySliderRaw) { fail(res, 500, "not_available"); setCORSHeaders(res); return; }
        try {
            int idx = std::stoi(req.matches[1]);
            auto j = Json::parse(req.body);
            std::string err;
            int raw = -1;
            if (j.contains("value")) {
                if (j["value"].is_number_integer()) {
                    raw = j["value"].get<int>(); // 0..1023
                }
                else if (j["value"].is_number_float()) {
                    double v = j["value"].get<double>(); // 0..1
                    if (v < 0.0) v = 0.0; if (v > 1.0) v = 1.0;
                    raw = static_cast<int>(v * 1023.0 + 0.5);
                }
            }
            if (raw < 0 || raw > 1023) { fail(res, 400, "value_out_of_range"); setCORSHeaders(res); return; }
            if (!m_cbs.applySliderRaw(idx, raw, err)) { fail(res, 400, err.empty() ? "action_failed" : err); }
            else { ok(res, Json{ {"slider", idx}, {"value_raw", raw} }); }
        }
        catch (...) {
            fail(res, 400, "bad_json_or_index");
        }
        setCORSHeaders(res);
        });
}
