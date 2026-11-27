#pragma once
#include <atomic>
#include <cstring>
#include <mutex>
#include <utility>
#include <vector>

// The producer publishes under the same lock used to clear the notification.
// An empty poll can therefore skip locking without losing a queued event.
template<class Event>
class RawInputQueue {
    std::mutex Mutex;
    std::vector<Event> Events;
    std::atomic_bool Pending = false;
public:
    void Push(Event event) {
        std::lock_guard lock(Mutex);
        Events.push_back(std::move(event));
        Pending.store(true, std::memory_order_release);
    }
    void Drain(std::vector<Event>& out) {
        out.clear();
        if (!Pending.load(std::memory_order_acquire)) { return; }
        std::lock_guard lock(Mutex);
        out.swap(Events);
        Pending.store(false, std::memory_order_release);
    }
};

inline bool ConsumeNewKeyboardPress(const unsigned char (&current)[256], unsigned char (&previous)[256]) {
    if (std::memcmp(current, previous, sizeof(previous)) == 0) { return false; }
    bool pressed = false;
    for (int key = 1; key < 256; ++key) { pressed |= current[key] != 0 && previous[key] == 0; }
    std::memcpy(previous, current, sizeof(previous));
    return pressed;
}
