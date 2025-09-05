#include "Core/Serial/SerialController.hpp"

SerialController::SerialController(const std::string& port, unsigned int baud)
    : m_port(port), m_baud(baud) {
}

SerialController::~SerialController() { stop(); }

void SerialController::start() {
    if (m_reader) return;

    // Crea il SerialReader con callback che parsea e aggiorna lo store
    m_reader = std::make_unique<SerialReader>(m_port, m_baud,
        [this](const std::string& line) {
            if (auto st = ParseDeckLine(line)) {
                if (m_store.updateIfChanged(*st, /*sliderThreshold*/ 2)) {
                    // Log minimale ogni cambiamento
                    const auto s = m_store.get();
                    fmt::print("BTN mask={}  SLD=[{}, {}, {}, {}, {}]\n",
                        s.buttonsMask(),
                        s.sliders[0], s.sliders[1], s.sliders[2], s.sliders[3], s.sliders[4]);
                }
            }
            else {
                // linea malformata: ignora (puoi aggiungere un contatore errori)
            }
        });

    m_reader->start();
}

void SerialController::stop() {
    if (!m_reader) return;
    m_reader->stop();
    m_reader.reset();
}
