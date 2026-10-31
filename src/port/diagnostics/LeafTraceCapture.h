#pragma once

#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>

namespace Paperboat::Diagnostics {
// Commands run on the render queue; samples arrive on the game thread. Only
// immutable text crosses that boundary. Never retain engine objects in reports.
class LeafTraceCapture {
public:
    enum class Request {
        None,
        Start,
        Stop
    };
    struct Snapshot {
        std::string text;
        size_t frames = 0;
        bool active = false, limited = false, pending = false;
        unsigned generation = 0;
    };
    explicit LeafTraceCapture(size_t maxFrames = 600, size_t maxBytes = 6 * 1024 * 1024)
        : maxFrames(maxFrames)
        , maxBytes(maxBytes) {
    }
    void Start() {
        request.store(Request::Start);
    }
    void Stop() {
        request.store(Request::Stop);
    }
    bool Begin(bool eligible) {
        const auto next = request.exchange(Request::None);
        std::lock_guard lock(mutex);
        if (next == Request::Start) {
            state = {};
            state.generation = ++generation;
            state.active = true;
        } else if (next == Request::Stop) {
            state.active = false;
        }
        return state.active && eligible;
    }
    bool AppendFrame(const std::string& text) {
        std::lock_guard lock(mutex);
        if (!state.active)
            return false;
        if (text.size() > maxBytes - state.text.size()) {
            state.active = false;
            state.limited = true;
            return false;
        }
        state.text += text;
        ++state.frames;
        if (state.frames >= maxFrames)
            state.active = false;
        return true;
    }
    Snapshot Read() const {
        std::lock_guard lock(mutex);
        auto result = state;
        result.pending = request.load() != Request::None;
        return result;
    }
    unsigned Generation() const {
        std::lock_guard lock(mutex);
        return state.generation;
    }

private:
    mutable std::mutex mutex;
    std::atomic<Request> request { Request::None };
    Snapshot state;
    unsigned generation = 0;
    const size_t maxFrames, maxBytes;
};
} // namespace Paperboat::Diagnostics
