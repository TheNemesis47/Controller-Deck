#pragma once
#include <nlohmann/json.hpp>
#include <mutex>
#include <deque>
#include <condition_variable>
#include <chrono>

class EventBus {
public:
    using Json = nlohmann::json;

    // Pubblica un evento (thread-safe). Mantiene al max 1024 eventi.
    void publish(const Json& ev);

    // Estrae il prossimo evento, con timeout. Ritorna false su timeout.
    bool popNext(Json& out, std::chrono::milliseconds timeout);

    // Utilities
    void clear();
    [[nodiscard]] size_t size() const;

private:
    mutable std::mutex m_mx;
    std::condition_variable m_cv;
    std::deque<Json> m_q;
    static constexpr size_t kMax = 1024;
};
