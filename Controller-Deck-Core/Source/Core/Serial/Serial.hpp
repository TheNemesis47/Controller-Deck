#pragma once
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <asio.hpp>

// Semplice reader di linee terminate da '\n'.
// Chiama onLine(data) sul thread di IO interno.
class SerialReader {
public:
    using LineCallback = std::function<void(const std::string&)>;

    SerialReader(const std::string& port, unsigned int baud, LineCallback onLine);
    ~SerialReader();

    void start();
    void stop();

private:
    void doRead();

    std::string m_port;
    unsigned int m_baud;
    LineCallback m_onLine;

    asio::io_context m_io;
    std::unique_ptr<asio::serial_port> m_serial;
    std::unique_ptr<std::thread> m_thread;
    std::atomic<bool> m_running{ false };
    asio::streambuf m_buffer;
};
