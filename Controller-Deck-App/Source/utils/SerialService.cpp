#include "utils/SerialService.hpp"

SerialService::~SerialService() {
    std::string dummy;
    (void)close(&dummy);
}

bool SerialService::open(const std::string& port, unsigned baud, std::string* outErr) {
    std::lock_guard<std::mutex> lk(m_mx);
    try {
        if (m_serial) { m_serial->stop(); m_serial.reset(); }
        m_serial = std::make_unique<SerialController>(port, baud);
        m_serial->start();
        m_port = port;
        m_baud = baud;
        return true;
    }
    catch (...) {
        if (outErr) *outErr = "open_failed";
        m_serial.reset();
        return false;
    }
}

bool SerialService::close(std::string* outErr) {
    std::lock_guard<std::mutex> lk(m_mx);
    if (!m_serial) { if (outErr) *outErr = "not_connected"; return false; }
    try {
        m_serial->stop();
        m_serial.reset();
        return true;
    }
    catch (...) {
        if (outErr) *outErr = "unknown_error";
        return false;
    }
}

bool SerialService::isConnected() const {
    std::lock_guard<std::mutex> lk(m_mx);
    return (bool)m_serial;
}

DeckState SerialService::readState() const {
    std::lock_guard<std::mutex> lk(m_mx);
    return m_serial ? m_serial->store().get() : DeckState{};
}

std::string SerialService::port() const { std::lock_guard<std::mutex> lk(m_mx); return m_port; }
unsigned SerialService::baud() const { std::lock_guard<std::mutex> lk(m_mx); return m_baud; }
