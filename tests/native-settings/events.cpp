// Exercise the production snapshot reader against the actual event bus. No
// renderer, ROM, save, textures, simulator or application context is needed.
#include "port/ui/NativeEventSnapshot.h"
#include <iostream>
#include <stdexcept>

namespace N = Paperboat::NativeSettings;
static void Require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
int main() {
    Require(!N::CaptureEvents(nullptr).available, "Missing bus was reported as available");
    Ship::EventSystem bus;
    Require(N::CaptureEvents(&bus).events.empty(), "Empty bus contains invented events");
    char eventName[] = "OnMapLoad";
    char callerPath[] = "src/map.c";
    const auto first = bus.RegisterEvent(eventName);
    const auto second = bus.RegisterEvent();
    auto* registration = bus.GetEventRegistration(first);
    registration->NextListenerID = int64_t(1) << 40;
    const auto listener = bus.RegisterListener(first, [](IEvent*) { }, EVENT_PRIORITY_HIGH, "src/listener.cpp", 87);
    bus.RegisterListener(first, [](IEvent*) { }, EVENT_PRIORITY_LOW, nullptr, 0);
    IEvent payload { false };
    bus.CallEvent(first, &payload, callerPath, 418, "first");
    bus.CallEvent(first, &payload, callerPath, 418, "first");
    auto snapshot = N::CaptureEvents(&bus);
    Require(snapshot.available && snapshot.events.size() == 2, "Registered events missing");
    Require(snapshot.events[0].id == first && snapshot.events[1].id == second, "Event order is unstable");
    Require(snapshot.events[0].name == "OnMapLoad", "Registered event name missing");
#ifdef _DEBUG
    Require(
        snapshot.recordsCallSites && snapshot.events[0].callers.size() == 1 && snapshot.events[0].callers[0].calls == 2,
        "Instrumented call sites/counts missing"
    );
#else
    Require(
        !snapshot.recordsCallSites && snapshot.events[0].callers.empty(),
        "Release snapshot claims unrecorded call sites"
    );
#endif
    registration->Callers["first"] = { callerPath, 418, UINT64_MAX };
    registration->Callers["second"] = { "src/alpha.c", 1, 3 };
    snapshot = N::CaptureEvents(&bus);
    Require(
        snapshot.events[0].callers[0].path == "src/alpha.c" && snapshot.events[0].callers[1].calls == UINT64_MAX,
        "Caller sorting or full-width call counts lost"
    );
    Require(snapshot.events[0].listeners.size() == registration->Listeners.size(), "Listeners missing");
    for (size_t i = 0; i < registration->Listeners.size(); ++i) {
        const auto& expected = registration->Listeners[i];
        const auto& actual = snapshot.events[0].listeners[i];
        Require(actual.id == expected.ID && actual.line == expected.Metadata.Line, "Listener order/metadata changed");
        Require(
            actual.priority
                == (expected.Priority == EVENT_PRIORITY_HIGH ? N::EventPriority::High : N::EventPriority::Low),
            "Listener priority changed"
        );
    }
    eventName[0] = 'X';
    callerPath[0] = 'X';
    bus.UnregisterListener(first, listener);
    Require(
        snapshot.events[0].name == "OnMapLoad" && snapshot.events[0].callers[1].path == "src/map.c"
            && snapshot.events[0].listeners.size() == 2,
        "Snapshot retained mutable engine data"
    );
    const auto next = N::CaptureEvents(&bus);
    Require(
        next.events[0].listeners.size() == 1 && next != snapshot, "Unregistered listener retained in fresh snapshot"
    );
    bus.GetEventRegistration(second)->Name = nullptr;
    bus.GetEventRegistration(second)->Callers["unknown"] = { nullptr, 0, 1 };
    snapshot = N::CaptureEvents(&bus);
    Require(
        !snapshot.events[1].name.empty() && snapshot.events[1].callers[0].path.empty(),
        "Missing diagnostic metadata was not handled"
    );
    std::cout << "Event snapshot checks passed\n";
}
