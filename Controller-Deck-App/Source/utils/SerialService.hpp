#pragma once
#include "Core/Serial/SerialController.hpp"
#include "Core/DeckState.hpp"
#include <mutex>
#include <memory>
#include <string>

class SerialService {
public:
    ~SerialService();

    // Apre (o riapre) la seriale in modo atomico.
    // outErr (opzionale) riceve "open_failed" in caso di eccezioni.
    bool open(const std::string& port, unsigned baud, std::string* outErr = nullptr);

    // Chiude la seriale. Se non aperta, ritorna false e imposta outErr="not_connected".
    bool close(std::string* outErr = nullptr);

    // Stato thread-safe
    [[nodiscard]] bool isConnected() const;
    [[nodiscard]] DeckState readState() const;

    // Ultima configurazione nota (utile per /serial/status)
    [[nodiscard]] std::string port() const;
    [[nodiscard]] unsigned baud() const;

private:
    mutable std::mutex m_mx;
    std::unique_ptr<SerialController> m_serial;
    std::string m_port;
    unsigned m_baud{ 0 };
};
