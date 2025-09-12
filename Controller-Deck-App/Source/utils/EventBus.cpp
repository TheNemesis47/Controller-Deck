#include "utils/EventBus.hpp"

void EventBus::publish(const Json& ev) {
    {
        std::lock_guard<std::mutex> lk(m_mx);
        m_q.push_back(ev);
        if (m_q.size() > kMax) m_q.pop_front();
    }
    m_cv.notify_all();
}

bool EventBus::popNext(Json& out, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lk(m_mx);
    if (m_q.empty()) {
        if (!m_cv.wait_for(lk, timeout, [&] { return !m_q.empty(); })) return false;
    }
    out = std::move(m_q.front());
    m_q.pop_front();
    return true;
}

void EventBus::clear() {
    std::lock_guard<std::mutex> lk(m_mx);
    m_q.clear();
}

size_t EventBus::size() const {
    std::lock_guard<std::mutex> lk(m_mx);
    return m_q.size();
}
