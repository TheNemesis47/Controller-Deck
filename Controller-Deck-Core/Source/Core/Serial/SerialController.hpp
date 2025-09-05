#pragma once
#include <string>
#include <memory>
#include <fmt/core.h>
#include "Core/DeckState.hpp"
#include "Core/MessageParser.hpp"
#include "Core/Serial/Serial.hpp"  // il tuo SerialReader

// Incapsula SerialReader + parser + storage
class SerialController {
public:
    SerialController(const std::string& port, unsigned int baud);
    ~SerialController();

    void start();
    void stop();

    DeckStateStore& store() { return m_store; }

private:
    std::string m_port;
    unsigned int m_baud;
    DeckStateStore m_store;
    std::unique_ptr<SerialReader> m_reader;
};
