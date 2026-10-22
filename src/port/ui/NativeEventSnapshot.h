#pragma once

#include "NativeSettings.h"
#include <ship/events/EventSystem.h>
#include <algorithm>
#include <tuple>

namespace Paperboat::NativeSettings {
inline EventSnapshot CaptureEvents(Ship::EventSystem* system) {
    EventSnapshot snapshot;
    snapshot.available = system != nullptr;
#ifdef _DEBUG
    // Match libultraship's CallEvent instrumentation. Release still exposes
    // events/listeners; it must not claim that an empty caller map means zero calls.
    snapshot.recordsCallSites = true;
#endif
    if (!system)
        return snapshot;
    for (const auto& [id, registration] : system->GetEventRegistrations()) {
        EventInfo event;
        event.id = id;
        event.name = registration.Name ? registration.Name : "Unknown event";
        for (const auto& [key, caller] : registration.Callers)
            event.callers.push_back({ caller.Path ? caller.Path : "", caller.Line, caller.Count });
        // The caller registry is unordered. Keep rows stable as counts change.
        std::sort(event.callers.begin(), event.callers.end(), [](const auto& left, const auto& right) {
            return std::tie(left.path, left.line) < std::tie(right.path, right.line);
        });
        // Preserve the engine's listener order and actual listener IDs.
        for (const auto& listener : registration.Listeners) {
            const auto priority = listener.Priority == EVENT_PRIORITY_LOW ? EventPriority::Low
                : listener.Priority == EVENT_PRIORITY_NORMAL              ? EventPriority::Normal
                : listener.Priority == EVENT_PRIORITY_HIGH                ? EventPriority::High
                                                                          : EventPriority::Unknown;
            event.listeners.push_back(
                { listener.ID, listener.Metadata.Path ? listener.Metadata.Path : "", listener.Metadata.Line, priority }
            );
        }
        snapshot.events.push_back(std::move(event));
    }
    std::sort(snapshot.events.begin(), snapshot.events.end(), [](const auto& left, const auto& right) {
        return left.id < right.id;
    });
    return snapshot;
}
} // namespace Paperboat::NativeSettings
