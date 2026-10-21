#include "NativeSettings.h"
#include "PaperboatGui.hpp"
#include <ship/Context.h>
#include <ship/controller/controldeck/ControlDeck.h>
#include <ship/controller/controldevice/controller/Controller.h>
#include <ship/controller/controldevice/controller/mapping/factories/ButtonMappingFactory.h>
#include <ship/controller/controldevice/controller/mapping/factories/AxisDirectionMappingFactory.h>
#include <ship/controller/controldevice/controller/mapping/ControllerRumbleMapping.h>
#include <ship/controller/controldevice/controller/mapping/ControllerGyroMapping.h>
#include <ship/controller/controldevice/controller/mapping/ControllerLEDMapping.h>
#include <libultraship/libultra/controller.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <sstream>
#include <unordered_set>

namespace Paperboat::NativeSettings {
namespace {
    using Clock = std::chrono::steady_clock;
    enum class CaptureKind {
        Button,
        Stick,
        Rumble,
        Gyro,
        LED
    };
    struct Capture {
        std::string key, id, title;
        uint8_t port = 0, stick = 0;
        int target = 0;
        CaptureKind kind = CaptureKind::Button;
        bool active = false, armed = false, finished = false, timedOut = false;
        Clock::time_point ready, deadline;
    } capture;
    std::shared_ptr<Ship::ControllerRumbleMapping> rumbleTest;
    Clock::time_point rumbleUntil;
    const std::vector<std::pair<int, const char*>> buttons = { { BTN_A, "A" },
                                                               { BTN_B, "B" },
                                                               { BTN_START, "Start" },
                                                               { BTN_L, "L" },
                                                               { BTN_R, "R" },
                                                               { BTN_Z, "Z" },
                                                               { BTN_CUP, "C Up" },
                                                               { BTN_CDOWN, "C Down" },
                                                               { BTN_CLEFT, "C Left" },
                                                               { BTN_CRIGHT, "C Right" },
                                                               { BTN_DUP, "D-Pad Up" },
                                                               { BTN_DDOWN, "D-Pad Down" },
                                                               { BTN_DLEFT, "D-Pad Left" },
                                                               { BTN_DRIGHT, "D-Pad Right" } };
    const std::vector<std::pair<Ship::Direction, const char*>> directions = {
        { Ship::UP, "Up" }, { Ship::DOWN, "Down" }, { Ship::LEFT, "Left" }, { Ship::RIGHT, "Right" }
    };
    auto Deck() {
        return Ship::Context::GetRawInstance()->GetControlDeck();
    }
    std::shared_ptr<Ship::Controller> Controller(int port) {
        return port >= 0 && port < MAXCONTROLLERS && Deck() ? Deck()->GetControllerByPort(port) : nullptr;
    }
    auto Stick(const std::shared_ptr<Ship::Controller>& controller, int stick) {
        return stick ? controller->GetRightStick() : controller->GetLeftStick();
    }
    std::shared_ptr<Ship::ControllerButton> Button(const std::shared_ptr<Ship::Controller>& controller, int mask) {
        const auto mappings = controller->GetAllButtons();
        const auto it = mappings.find(mask);
        return it == mappings.end() ? nullptr : it->second;
    }
    std::string Base(int port) {
        return "bindings/" + std::to_string(port);
    }
    Row Info(std::string title, std::string detail = "") {
        Row row;
        row.title = std::move(title);
        row.detail = std::move(detail);
        return row;
    }
    Row Action(std::string title, std::function<void()> callback, bool destructive = false) {
        Row row;
        row.title = std::move(title);
        row.kind = Kind::Action;
        row.activate = std::move(callback);
        row.destructive = destructive;
        return row;
    }
    Row Slider(std::string title, int value, int maximum, std::function<void(uint8_t)> change) {
        Row row;
        row.title = std::move(title);
        row.kind = Kind::Integer;
        row.value = value;
        row.maximum = maximum;
        row.change = [maximum, change = std::move(change)](double value) {
            if (std::isfinite(value))
                change(uint8_t(std::lround(std::clamp(value, 0., double(maximum)))));
        };
        return row;
    }
    std::string InputName(const std::shared_ptr<Ship::ControllerInputMapping>& mapping) {
        std::string text = mapping->GetPhysicalInputName();
        // The shared mapping labels use an ImGui icon font. UIKit uses normal text.
        for (const auto& [icon, plain] : std::vector<std::pair<std::string, std::string>> { { "\uf060", "←" },
                                                                                            { "\uf061", "→" },
                                                                                            { "\uf062", "↑" },
                                                                                            { "\uf063", "↓" },
                                                                                            { "\uf0c9", "Start" },
                                                                                            { "\uf067", "+" },
                                                                                            { "\uf068", "−" } })
        {
            size_t offset = 0;
            while ((offset = text.find(icon, offset)) != std::string::npos) {
                text.replace(offset, icon.size(), plain);
                offset += plain.size();
            }
        }
        return text;
    }
    template <typename Map> std::string Summary(const Map& mappings) {
        std::vector<std::string> names;
        for (const auto& [id, mapping] : mappings)
            if (mapping)
                names.push_back(mapping->GetPhysicalDeviceName() + ": " + InputName(mapping));
        std::sort(names.begin(), names.end());
        std::string result;
        for (const auto& name : names) {
            if (!result.empty())
                result += " · ";
            result += name;
        }
        return result.empty() ? "Not bound" : result;
    }
    void MarkConfigured(int port) {
        const std::string name = CVAR_PREFIX_CONTROLLERS ".Port" + std::to_string(port + 1) + ".HasConfig";
        CVarSetInteger(name.c_str(), 1);
        Ship::Context::GetRawInstance()->GetConsoleVariables()->Save();
    }
    void BeginCapture(
        std::string key, int port, CaptureKind kind, int target, int stick, std::string id, std::string title
    ) {
        capture = { };
        capture.key = std::move(key);
        capture.port = port;
        capture.kind = kind;
        capture.target = target;
        capture.stick = stick;
        capture.id = std::move(id);
        capture.title = std::move(title);
        capture.active = true;
        capture.ready = Clock::now() + std::chrono::milliseconds(250);
        capture.deadline = Clock::now() + std::chrono::seconds(20);
    }
    Row CaptureLink(std::string title, int port, CaptureKind kind, int target = 0, int stick = 0, std::string id = "") {
        const std::string key = "bindings/capture/" + std::to_string(port) + "/" + std::to_string(int(kind)) + "/"
            + std::to_string(target) + "/" + std::to_string(stick) + "/" + id;
        Row row = Link(title, key, "gamecontroller");
        row.activate = [key, port, kind, target, stick, id, title] {
            BeginCapture(key, port, kind, target, stick, id, title);
        };
        return row;
    }
    bool Released(int port) {
        for (const auto& [id, gamepad] :
             Deck()->GetConnectedPhysicalDeviceManager()->GetConnectedSDLGamepadsForPort(port))
        {
            for (int button = 0; button < SDL_CONTROLLER_BUTTON_MAX; ++button)
                if (SDL_GameControllerGetButton(gamepad, SDL_GameControllerButton(button)))
                    return false;
            for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; ++axis)
                if (std::abs(int(SDL_GameControllerGetAxis(gamepad, SDL_GameControllerAxis(axis)))) > 8000)
                    return false;
        }
        return true;
    }
    std::vector<std::string> Parts(const std::string& key) {
        std::vector<std::string> parts;
        std::istringstream stream(key);
        std::string part;
        while (std::getline(stream, part, '/'))
            parts.push_back(part);
        return parts;
    }
    int Index(const std::vector<std::string>& parts, size_t index, int maximum) {
        if (index >= parts.size())
            return -1;
        int value = -1;
        const auto& text = parts[index];
        const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
        return result.ec == std::errc() && result.ptr == text.data() + text.size() && value >= 0 && value <= maximum
            ? value
            : -1;
    }
    void Confirm(std::string title, std::string message, std::function<void()> action) {
        PaperboatGui::RegisterPopup(title, message, title, "Cancel", std::move(action));
    }
    void AddDevices(Page& page, int port) {
        Section devices { "Connected Controllers", "Pair controllers in iOS Settings, then refresh this list.", { } };
        auto manager = Deck()->GetConnectedPhysicalDeviceManager();
        auto names = manager->GetConnectedSDLGamepadNames();
        std::vector<std::pair<int, std::string>> sorted(names.begin(), names.end());
        std::sort(sorted.begin(), sorted.end());
        for (const auto& [id, name] : sorted) {
            Row toggle;
            toggle.title = name;
            toggle.kind = Kind::Toggle;
            toggle.value = !manager->PortIsIgnoringInstanceId(port, id);
            toggle.detail = "Use this controller on Port " + std::to_string(port + 1);
            toggle.change = [manager, port, id](double value) {
                if (!manager->GetConnectedSDLGamepadNames().contains(id))
                    return;
                if (value)
                    manager->UnignoreInstanceIdForPort(port, id);
                else
                    manager->IgnoreInstanceIdForPort(port, id);
            };
            devices.rows.push_back(std::move(toggle));
        }
        if (sorted.empty())
            devices.rows.push_back(Info("No Controller Connected", "Saved bindings can still be viewed and adjusted."));
        devices.rows.push_back(Action("Refresh Controllers", [manager] {
            const auto previous = manager->GetConnectedSDLGamepadNames();
            std::vector<std::unordered_set<int32_t>> ignored(MAXCONTROLLERS);
            for (int port = 0; port < MAXCONTROLLERS; ++port)
                ignored[port] = manager->GetIgnoredInstanceIdsForPort(port);
            manager->RefreshConnectedSDLGamepads();
            const auto connected = manager->GetConnectedSDLGamepadNames();
            // The backend defaults newly scanned devices to Port 1. Preserve the
            // user's existing port choices when refreshing from this screen.
            for (int port = 0; port < MAXCONTROLLERS; ++port)
                for (const auto& [id, name] : previous)
                    if (connected.contains(id) && !ignored[port].contains(id))
                        manager->UnignoreInstanceIdForPort(port, id);
        }));
        page.sections.push_back(std::move(devices));
    }
    void AddPeripheralPages(Page& page, int port) {
        page.sections.push_back(
            { "Controller Features",
              "Availability depends on the connected controller.",
              { Link("Rumble", Base(port) + "/rumble", "waveform"), Link("Gyro", Base(port) + "/gyro", "gyroscope"),
                Link("LED Color", Base(port) + "/led", "lightbulb") } }
        );
    }
}

void CancelBindingCapture(const std::string& key) {
    if (!key.empty() && key != capture.key)
        return;
    capture.active = false;
    capture.finished = true;
}
void UpdateControllerBindings(bool visible) {
    const auto now = Clock::now();
    if (rumbleTest && (!visible || now >= rumbleUntil)) {
        rumbleTest->StopRumble();
        rumbleTest.reset();
    }
    if (!visible) {
        CancelBindingCapture();
        return;
    }
    if (!capture.active || now < capture.ready)
        return;
    if (now >= capture.deadline) {
        capture.active = false;
        capture.timedOut = true;
        return;
    }
    auto controller = Controller(capture.port);
    if (!controller)
        return;
    SDL_GameControllerUpdate();
    if (!capture.armed) {
        capture.armed = Released(capture.port);
        return;
    }
    bool done = false;
    if (capture.kind == CaptureKind::Button) {
        auto button = Button(controller, capture.target);
        if (!button || (!capture.id.empty() && !button->GetButtonMappingById(capture.id))) {
            CancelBindingCapture();
            return;
        }
        auto mapping = Ship::ButtonMappingFactory::CreateButtonMappingFromSDLInput(capture.port, capture.target);
        if (mapping) {
            if (!capture.id.empty())
                button->ClearButtonMapping(capture.id);
            button->AddButtonMapping(mapping);
            mapping->SaveToConfig();
            button->SaveButtonMappingIdsToConfig();
            done = true;
        }
    } else if (capture.kind == CaptureKind::Stick) {
        auto stick = Stick(controller, capture.stick);
        const auto direction = Ship::Direction(capture.target);
        if (!capture.id.empty() && !stick->GetAxisDirectionMappingById(direction, capture.id)) {
            CancelBindingCapture();
            return;
        }
        auto mapping = Ship::AxisDirectionMappingFactory::CreateAxisDirectionMappingFromSDLInput(
            capture.port, Ship::StickIndex(capture.stick), direction
        );
        if (mapping) {
            if (!capture.id.empty())
                stick->ClearAxisDirectionMapping(direction, capture.id);
            stick->AddAxisDirectionMapping(direction, mapping);
            mapping->SaveToConfig();
            stick->SaveAxisDirectionMappingIdsToConfig();
            done = true;
        }
    } else if (capture.kind == CaptureKind::Rumble)
        done = controller->GetRumble()->AddRumbleMappingFromRawPress();
    else if (capture.kind == CaptureKind::Gyro)
        done = controller->GetGyro()->SetGyroMappingFromRawPress();
    else if (capture.kind == CaptureKind::LED)
        done = controller->GetLED()->AddLEDMappingFromRawPress();
    if (done) {
        MarkConfigured(capture.port);
        capture.active = false;
        capture.finished = true;
    }
}

Page ControllerBindingsPage(const std::string& key) {
    Page page { key, "Controller Bindings", "", { } };
    if (key.starts_with("bindings/capture/")) {
        page.title = "Capture Input";
        page.returnToParent = capture.key != key || capture.finished;
        page.sections.push_back(
            { "",
              "Your existing binding stays unchanged until a new input is captured.",
              { Info(
                    capture.timedOut    ? "No Input Received"
                        : capture.armed ? "Press a Button or Move a Stick"
                                        : "Release Buttons and Center Sticks",
                    capture.timedOut
                        ? "Check that the controller is connected and supports this feature, then try again."
                        : capture.title
                ),
                Action("Cancel", [key] { CancelBindingCapture(key); }) } }
        );
        if (capture.timedOut)
            page.sections.front().rows.push_back(Action("Try Again", [key] {
                if (capture.key == key)
                    BeginCapture(
                        key, capture.port, capture.kind, capture.target, capture.stick, capture.id, capture.title
                    );
            }));
        return page;
    }
    if (key == "bindings/root") {
        Section ports {
            "Controller Ports",
            "Paper Mario uses Port 1 for normal play. Saved bindings are shared with the existing controller backend.",
            { }
        };
        for (int port = 0; port < MAXCONTROLLERS; ++port)
            ports.rows.push_back(Link("Port " + std::to_string(port + 1), Base(port), "gamecontroller"));
        page.sections.push_back(std::move(ports));
        return page;
    }
    const auto parts = Parts(key);
    const int port = Index(parts, 1, MAXCONTROLLERS - 1);
    auto controller = Controller(port);
    if (!controller) {
        page.sections.push_back({ "", "", { Info("Controller Unavailable") } });
        return page;
    }
    if (parts.size() == 2) {
        page.title = "Port " + std::to_string(port + 1);
        AddDevices(page, port);
        Section inputs { "Buttons", "Select a button to add, replace or remove a binding.", { } };
        for (const auto& [mask, title] : buttons) {
            Row row = Link(title, Base(port) + "/button/" + std::to_string(mask), "gamecontroller");
            if (auto button = Button(controller, mask))
                row.detail = Summary(button->GetAllButtonMappings());
            inputs.rows.push_back(std::move(row));
        }
        page.sections.push_back(std::move(inputs));
        page.sections.push_back(
            { "Analog Controls",
              "",
              { Link("Analog Stick", Base(port) + "/stick/0", "l.joystick"),
                Link("Additional Right Stick", Base(port) + "/stick/1", "r.joystick"),
                Link("Input Thresholds", Base(port) + "/thresholds", "slider.horizontal.3") } }
        );
        AddPeripheralPages(page, port);
        page.sections.push_back(
            { "Defaults",
              "Gamepad defaults preserve keyboard and mouse bindings.",
              { Action(
                    "Restore Gamepad Defaults",
                    [port] {
                        Confirm(
                            "Restore Defaults", "Replace gamepad bindings on this port with their defaults?", [port] {
                                if (auto controller = Controller(port)) {
                                    controller->ClearAllMappingsForDeviceType(Ship::PhysicalDeviceType::SDLGamepad);
                                    controller->AddDefaultMappings(Ship::PhysicalDeviceType::SDLGamepad);
                                }
                            }
                        );
                    }
                ),
                Action(
                    "Clear All Bindings",
                    [port] {
                        Confirm(
                            "Clear Bindings",
                            "Remove all button, stick and controller-feature bindings on this port?",
                            [port] {
                                if (auto controller = Controller(port))
                                    controller->ClearAllMappings();
                            }
                        );
                    },
                    true
                ) } }
        );
        return page;
    }
    if (parts[2] == "thresholds") {
        page.title = "Input Thresholds";
        auto settings = Deck()->GetGlobalSDLDeviceSettings();
        page.sections.push_back(
            { "All Controllers",
              "How far an axis must move before it counts as a button press. Applies to every port.",
              { Slider(
                    "Stick Axis Threshold (%)",
                    settings->GetStickAxisThresholdPercentage(),
                    100,
                    [settings](uint8_t v) { settings->SetStickAxisThresholdPercentage(v); }
                ),
                Slider(
                    "Trigger Threshold (%)", settings->GetTriggerAxisThresholdPercentage(), 100,
                    [settings](uint8_t v) { settings->SetTriggerAxisThresholdPercentage(v); }
                ) } }
        );
        return page;
    }
    const bool buttonPage = parts[2] == "button";
    const int stickIndex = Index(parts, 3, 1);
    if (buttonPage || (parts[2] == "stick" && stickIndex >= 0)) {
        const int mask = Index(parts, 3, 65535);
        const int direction = Index(parts, 4, 3);
        auto button = buttonPage ? Button(controller, mask) : nullptr;
        auto stick = Stick(controller, std::max(0, stickIndex));
        if (!buttonPage && parts.size() == 4) {
            page.title = stickIndex ? "Right Stick" : "Analog Stick";
            page.sections.push_back(
                { "Response",
                  "",
                  { Slider(
                        "Sensitivity (%)",
                        stick->GetSensitivityPercentage(),
                        200,
                        [stick](uint8_t v) { stick->SetSensitivity(v); }
                    ),
                    Slider(
                        "Dead Zone (%)",
                        stick->GetDeadzonePercentage(),
                        100,
                        [stick](uint8_t v) { stick->SetDeadzone(v); }
                    ),
                    Slider(
                        "Notch Snap (°)",
                        stick->GetNotchSnapAngle(),
                        45,
                        [stick](uint8_t v) { stick->SetNotchSnapAngle(v); }
                    ),
                    Action("Reset Stick Response", [stick] {
                        stick->ResetSensitivityToDefault();
                        stick->ResetDeadzoneToDefault();
                        stick->ResetNotchSnapAngleToDefault();
                    }) } }
            );
            Section axes { "Directions", "Select a direction to edit its physical inputs.", { } };
            for (const auto& [dir, title] : directions) {
                auto row = Link(title, key + "/" + std::to_string(int(dir)));
                row.detail = Summary(stick->GetAllAxisDirectionMappingByDirection(dir));
                axes.rows.push_back(std::move(row));
            }
            page.sections.push_back(std::move(axes));
            return page;
        }
        if ((buttonPage && !button) || (!buttonPage && direction < 0))
            return page;
        if (buttonPage) {
            for (const auto& [value, title] : buttons)
                if (value == mask)
                    page.title = title;
        } else {
            for (const auto& [value, title] : directions)
                if (int(value) == direction)
                    page.title = std::string(stickIndex ? "Right Stick " : "Analog Stick ") + title;
        }
        const auto kind = buttonPage ? CaptureKind::Button : CaptureKind::Stick;
        const int target = buttonPage ? mask : direction;
        page.sections.push_back(
            { "", "", { CaptureLink("Add Binding", port, kind, target, std::max(0, stickIndex)) } }
        );
        // Keep the existing multiple-input model: editing one mapping leaves the others intact.
        auto addMapping = [&](const std::string& id, const auto& mapping) {
            auto change = CaptureLink("Change Binding", port, kind, target, std::max(0, stickIndex), id);
            auto remove = Action(
                "Remove Binding",
                [button, stick, direction, id] {
                    if (button) {
                        if (button->GetButtonMappingById(id))
                            button->ClearButtonMapping(id);
                    } else if (stick->GetAxisDirectionMappingById(Ship::Direction(direction), id))
                        stick->ClearAxisDirectionMapping(Ship::Direction(direction), id);
                },
                true
            );
            page.sections.push_back({ InputName(mapping), mapping->GetPhysicalDeviceName(), { change, remove } });
        };
        if (buttonPage) {
            auto maps = button->GetAllButtonMappings();
            std::vector<std::string> ids;
            for (const auto& [id, mapping] : maps)
                ids.push_back(id);
            std::sort(ids.begin(), ids.end());
            for (const auto& id : ids)
                addMapping(id, maps.at(id));
        } else {
            auto maps = stick->GetAllAxisDirectionMappingByDirection(Ship::Direction(direction));
            std::vector<std::string> ids;
            for (const auto& [id, mapping] : maps)
                ids.push_back(id);
            std::sort(ids.begin(), ids.end());
            for (const auto& id : ids)
                addMapping(id, maps.at(id));
        }
        if (page.sections.size() == 1)
            page.sections.push_back(
                { "", "", { Info("Not Bound", "Use Add Binding to capture a controller input.") } }
            );
        return page;
    }
    if (parts[2] == "rumble") {
        page.title = "Rumble";
        page.sections.push_back(
            { "",
              "Press a button on a controller that supports rumble.",
              { CaptureLink("Add Rumble Controller", port, CaptureKind::Rumble) } }
        );
        auto rumble = controller->GetRumble();
        for (const auto& [id, mapping] : rumble->GetAllRumbleMappings()) {
            std::weak_ptr<Ship::ControllerRumbleMapping> weak = mapping;
            page.sections.push_back(
                { mapping->GetPhysicalDeviceName(),
                  "",
                  { Slider(
                        "Low Frequency (%)",
                        mapping->GetLowFrequencyIntensityPercentage(),
                        100,
                        [weak](uint8_t v) {
                            if (auto m = weak.lock())
                                m->SetLowFrequencyIntensity(v);
                        }
                    ),
                    Slider(
                        "High Frequency (%)",
                        mapping->GetHighFrequencyIntensityPercentage(),
                        100,
                        [weak](uint8_t v) {
                            if (auto m = weak.lock())
                                m->SetHighFrequencyIntensity(v);
                        }
                    ),
                    Action(
                        "Test Rumble",
                        [weak] {
                            if (auto m = weak.lock()) {
                                if (rumbleTest)
                                    rumbleTest->StopRumble();
                                rumbleTest = m;
                                rumbleUntil = Clock::now() + std::chrono::milliseconds(500);
                                m->StartRumble();
                            }
                        }
                    ),
                    Action(
                        "Remove Rumble Controller",
                        [rumble, id] {
                            if (rumble->GetAllRumbleMappings().contains(id))
                                rumble->ClearRumbleMapping(id);
                        },
                        true
                    ) } }
            );
        }
    } else if (parts[2] == "gyro") {
        page.title = "Gyro";
        auto gyro = controller->GetGyro();
        page.sections.push_back(
            { "",
              "Press a button on a controller with a supported motion sensor.",
              { CaptureLink("Set Gyro Controller", port, CaptureKind::Gyro) } }
        );
        if (auto mapping = gyro->GetGyroMapping()) {
            std::weak_ptr<Ship::ControllerGyroMapping> weak = mapping;
            page.sections.push_back(
                { mapping->GetPhysicalDeviceName(),
                  "",
                  { Slider(
                        "Sensitivity (%)",
                        mapping->GetSensitivityPercent(),
                        100,
                        [weak](uint8_t v) {
                            if (auto m = weak.lock())
                                m->SetSensitivity(v);
                        }
                    ),
                    Action(
                        "Recalibrate",
                        [weak] {
                            if (auto m = weak.lock())
                                m->Recalibrate();
                        }
                    ),
                    Action(
                        "Remove Gyro Controller",
                        [gyro, weak] {
                            if (gyro->GetGyroMapping() == weak.lock())
                                gyro->ClearGyroMapping();
                        },
                        true
                    ) } }
            );
        }
    } else if (parts[2] == "led") {
        page.title = "LED Color";
        auto led = controller->GetLED();
        page.sections.push_back(
            { "",
              "Press a button on a controller with a supported LED.",
              { CaptureLink("Add LED Controller", port, CaptureKind::LED) } }
        );
        for (const auto& [id, mapping] : led->GetAllLEDMappings()) {
            std::weak_ptr<Ship::ControllerLEDMapping> weak = mapping;
            Row source;
            source.title = "Color Source";
            source.kind = Kind::Choice;
            source.value = mapping->GetColorSource();
            source.choices = { { LED_COLOR_SOURCE_OFF, "Off" },
                               { LED_COLOR_SOURCE_SET, "Custom Color" },
                               { LED_COLOR_SOURCE_GAME, "Game" } };
            source.change = [weak](double v) {
                if (auto m = weak.lock(); m && std::isfinite(v)
                    && (v == LED_COLOR_SOURCE_OFF || v == LED_COLOR_SOURCE_SET || v == LED_COLOR_SOURCE_GAME))
                    m->SetColorSource(uint8_t(v));
            };
            Row color;
            color.title = "Custom Color";
            color.kind = Kind::Color;
            color.alpha = false;
            const auto saved = mapping->GetSavedColor();
            color.value = (uint32_t(saved.r) << 24) | (uint32_t(saved.g) << 16) | (uint32_t(saved.b) << 8) | 255;
            color.change = [weak](double v) {
                if (auto m = weak.lock(); m && std::isfinite(v) && v >= 0 && v <= UINT32_MAX) {
                    const auto c = uint32_t(v);
                    m->SetSavedColor({ uint8_t(c >> 24), uint8_t(c >> 16), uint8_t(c >> 8) });
                }
            };
            page.sections.push_back(
                { mapping->GetPhysicalDeviceName(),
                  "",
                  { source, color,
                    Action(
                        "Remove LED Controller",
                        [led, id] {
                            if (led->GetAllLEDMappings().contains(id))
                                led->ClearLEDMapping(id);
                        },
                        true
                    ) } }
            );
        }
    }
    return page;
}
} // namespace Paperboat::NativeSettings
