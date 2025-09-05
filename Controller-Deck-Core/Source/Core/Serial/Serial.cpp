#include "Serial.hpp"
#include <fmt/core.h>

SerialReader::SerialReader(const std::string& port, unsigned int baud, LineCallback onLine)
    : m_port(port), m_baud(baud), m_onLine(std::move(onLine)) {
}

SerialReader::~SerialReader() { stop(); }

void SerialReader::start() {
    if (m_running.exchange(true)) return;

    m_serial = std::make_unique<asio::serial_port>(m_io);
    asio::error_code ec;
    m_serial->open(m_port, ec);
    if (ec) {
        fmt::print("Errore apertura {}: {}\n", m_port, ec.message());
        m_running = false;
        return;
    }

    m_serial->set_option(asio::serial_port_base::baud_rate(m_baud));
    m_serial->set_option(asio::serial_port_base::character_size(8));
    m_serial->set_option(asio::serial_port_base::parity(asio::serial_port_base::parity::none));
    m_serial->set_option(asio::serial_port_base::stop_bits(asio::serial_port_base::stop_bits::one));
    m_serial->set_option(asio::serial_port_base::flow_control(asio::serial_port_base::flow_control::none));

    doRead();
    m_thread = std::make_unique<std::thread>([this] { m_io.run(); });
    fmt::print("Seriale {} @ {} avviata.\n", m_port, m_baud);
}

void SerialReader::stop() {
    if (!m_running.exchange(false)) return;

    asio::post(m_io, [this] {
        if (m_serial && m_serial->is_open()) {
            asio::error_code ignored;
            m_serial->cancel(ignored);
            m_serial->close(ignored);
        }
        });

    if (m_thread && m_thread->joinable()) {
        m_thread->join();
    }
    m_serial.reset();
    m_thread.reset();
    m_io.restart(); // pronto per un eventuale riavvio
}

void SerialReader::doRead() {
    if (!m_running) return;
    asio::async_read_until(*m_serial, m_buffer, '\n',
        [this](const asio::error_code& ec, std::size_t bytes) {
            if (!ec) {
                std::istream is(&m_buffer);
                std::string line;
                std::getline(is, line);
                if (m_onLine) m_onLine(line);
                doRead(); // continua
            }
            else if (ec != asio::error::operation_aborted) {
                fmt::print("Errore IO: {}\n", ec.message());
                stop();
            }
        });
}
