// Runs with port resources only. Synthetic save values never leave the temporary test directory.
#include "port/Engine.h"
#include "port/ui/PaperboatGui.hpp"
#include "port/ui/NativeSettings.h"
#include "port/ui/NativeTheme.h"
#include "port/ui/devtools/valueviewer/ValueViewer.h"
#include "port/FilePicker.h"
#include "port/enhancements/gameplay/ActorBlueprints.h"
#include <fast/resource/ResourceType.h>
#include <fast/resource/factory/TextureFactory.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <ship/controller/controldevice/controller/mapping/keyboard/KeyboardKeyToButtonMapping.h>
#include <ship/controller/controldevice/controller/mapping/ControllerRumbleMapping.h>
#include "common_structs.h"
#include <cmath>
#include <iostream>
#include <fstream>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#undef End
extern std::shared_ptr<Fast::Fast3dWindow> gsFast3dWindow;
extern "C" {
#include "dx/versioning.h"
extern SaveData gCurrentSaveFile;
extern PlayerData gPlayerData;
}
namespace N = Paperboat::NativeSettings;
// The test runner compiles only FilePicker.cpp with __IOS__, using these system
// picker stubs. No Apple framework, game file, or external file service is used.
namespace Paperboat {
static std::function<void(std::string)> pickedFile;
static std::function<void(bool)> exportedFile;
static std::string exportedPath;
void IosPickFile(std::function<void(std::string)> callback) {
    pickedFile = std::move(callback);
}
void IosExportFile(std::string path, std::function<void(bool)> callback) {
    exportedPath = std::move(path);
    exportedFile = std::move(callback);
}
}
static void Require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
static N::Row Find(const N::Page& page, const std::string& title) {
    for (const auto& section : page.sections)
        for (const auto& row : section.rows)
            if (row.title == title)
                return row;
    throw std::runtime_error("Missing row: " + title);
}
class TestRumbleMapping final : public Ship::ControllerRumbleMapping {
public:
    TestRumbleMapping()
        : ControllerRumbleMapping(Ship::PhysicalDeviceType::SDLGamepad, 3, 100, 100) {
    }
    bool started = false, stopped = false;
    void StartRumble() override {
        started = true;
    }
    void StopRumble() override {
        stopped = true;
    }
    std::string GetRumbleMappingId() override {
        return "native-fixture-rumble";
    }
    void SaveToConfig() override {
    }
    void EraseFromConfig() override {
    }
};
static void CheckControllerBindings(PaperboatGui::PaperboatMenu& menu) {
    auto controls = menu.NativePage("menu/Settings/Controls");
    const auto entry = Find(controls, "Controller Bindings");
    Require(
        entry.kind == N::Kind::Link && entry.target == "bindings/root" && !entry.activate,
        "Bindings still open or modify the classic controller window"
    );
    auto deck = Ship::Context::GetRawInstance()->GetControlDeck();
    auto controller = deck->GetControllerByPort(3);
    const auto buttonCount = controller->GetAllButtons().size();
    N::ControllerBindingsPage("bindings/3/button/not-a-mask");
    Require(
        controller->GetAllButtons().size() == buttonCount, "Malformed button page inserted a null controller button"
    );
    const int device = SDL_JoystickAttachVirtual(
        SDL_JOYSTICK_TYPE_GAMECONTROLLER, SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0
    );
    Require(device >= 0, "Could not create a synthetic controller");
    SDL_Joystick* joystick = SDL_JoystickOpen(device);
    Require(joystick != nullptr, "Could not open synthetic controller");
    char guid[33];
    SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joystick), guid, sizeof(guid));
    const auto mapping = std::string(guid)
        + ",Native Binding Fixture,a:b0,b:b1,x:b2,y:b3,start:b6,back:b4,"
          "leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,";
    Require(SDL_GameControllerAddMapping(mapping.c_str()) >= 0, "Synthetic gamepad mapping rejected");
    SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, -32768);
    SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, -32768);
    auto devices = deck->GetConnectedPhysicalDeviceManager();
    devices->HandlePhysicalDeviceConnect(device);
    const auto instance = SDL_JoystickInstanceID(joystick);
    devices->UnignoreInstanceIdForPort(3, instance);
    Find(N::BuildPage("bindings/3"), "Refresh Controllers").activate();
    Require(devices->GetConnectedSDLGamepadsForPort(3).contains(instance), "Synthetic controller not routed to port");
    auto button = controller->GetButton(BTN_A);
    auto keyboard = std::make_shared<Ship::KeyboardKeyToButtonMapping>(3, BTN_A, Ship::LUS_KB_X);
    button->AddButtonMapping(keyboard);
    keyboard->SaveToConfig();
    button->SaveButtonMappingIdsToConfig();
    const auto buttonKey = "bindings/3/button/" + std::to_string(BTN_A);
    auto add = Find(N::BuildPage(buttonKey), "Add Binding");
    add.activate();
    SDL_JoystickSetVirtualButton(joystick, 0, 1);
    SDL_Delay(260);
    N::UpdateControllerBindings(true);
    Require(button->GetAllButtonMappings().size() == 1, "Held input was captured while opening the binding page");
    SDL_JoystickSetVirtualButton(joystick, 0, 0);
    N::UpdateControllerBindings(true);
    SDL_JoystickSetVirtualButton(joystick, 0, 1);
    N::UpdateControllerBindings(true);
    Require(
        button->GetAllButtonMappings().size() == 2 && button->GetButtonMappingById(keyboard->GetButtonMappingId()),
        "Capturing a gamepad input discarded another binding"
    );
    Require(N::BuildPage(add.target).returnToParent, "Successful capture did not return to the native editor");
    controller->ReloadAllMappingsFromConfig();
    Require(button->GetAllButtonMappings().size() == 2, "Captured binding was not persisted");
    std::string gamepadId;
    for (const auto& [id, input] : button->GetAllButtonMappings())
        if (input->GetPhysicalDeviceType() == Ship::PhysicalDeviceType::SDLGamepad)
            gamepadId = id;
    Require(!gamepadId.empty(), "No gamepad binding saved");
    N::Row change;
    for (const auto& section : N::BuildPage(buttonKey).sections)
        for (const auto& row : section.rows)
            if (row.title == "Change Binding" && row.target.ends_with("/" + gamepadId))
                change = row;
    Require(bool(change.activate), "Native binding editor has no replace action");
    change.activate();
    SDL_JoystickSetVirtualButton(joystick, 0, 0);
    SDL_Delay(260);
    N::UpdateControllerBindings(true);
    SDL_JoystickSetVirtualButton(joystick, 1, 1);
    N::UpdateControllerBindings(true);
    Require(
        button->GetAllButtonMappings().size() == 2 && !button->GetButtonMappingById(gamepadId),
        "Replacing a binding kept the old input or lost an unrelated mapping"
    );
    controller->ReloadAllMappingsFromConfig();
    Require(
        button->GetAllButtonMappings().size() == 2 && !button->GetButtonMappingById(gamepadId),
        "Replaced binding was not persisted"
    );
    add.activate();
    N::CancelBindingCapture(add.target);
    SDL_JoystickSetVirtualButton(joystick, 1, 0);
    SDL_JoystickSetVirtualButton(joystick, 0, 1);
    N::UpdateControllerBindings(true);
    Require(button->GetAllButtonMappings().size() == 2, "Cancelled capture still edited bindings");
    SDL_JoystickSetVirtualButton(joystick, 0, 0);
    const auto axisKey = "bindings/3/stick/0/" + std::to_string(int(Ship::UP));
    auto axis = Find(N::BuildPage(axisKey), "Add Binding");
    axis.activate();
    SDL_Delay(260);
    N::UpdateControllerBindings(true);
    SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_LEFTY, -30000);
    N::UpdateControllerBindings(true);
    auto stick = controller->GetLeftStick();
    Require(
        stick->GetAllAxisDirectionMappingByDirection(Ship::UP).size() == 1,
        "Native stick binding did not capture a controller axis"
    );
    auto remove = Find(N::BuildPage(axisKey), "Remove Binding");
    remove.activate();
    remove.activate(); // A stale UI callback must be harmless.
    Require(
        stick->GetAllAxisDirectionMappingByDirection(Ship::UP).empty(), "Native remove did not erase the axis mapping"
    );
    const auto stickPage = N::BuildPage("bindings/3/stick/0");
    auto deadzone = Find(stickPage, "Dead Zone (%)");
    deadzone.change(500);
    Require(stick->GetDeadzonePercentage() == 100, "Stick dead zone was not clamped");
    deadzone.change(NAN);
    Require(stick->GetDeadzonePercentage() == 100, "Non-finite slider input changed the stick");
    Find(stickPage, "Reset Stick Response").activate();
    Require(stick->GetDeadzonePercentage() == DEFAULT_STICK_DEADZONE_PERCENTAGE, "Native stick reset failed");
    auto rumble = std::make_shared<TestRumbleMapping>();
    controller->GetRumble()->AddRumbleMapping(rumble);
    Find(N::BuildPage("bindings/3/rumble"), "Test Rumble").activate();
    Require(rumble->started, "Native rumble test did not start");
    add.activate();
    N::UpdateControllerBindings(false);
    Require(rumble->stopped, "Leaving native settings did not stop the rumble test");
    Require(N::BuildPage(add.target).returnToParent, "Closing settings left a binding capture active");
    devices->HandlePhysicalDeviceDisconnect(instance);
    SDL_JoystickClose(joystick);
    Require(SDL_JoystickDetachVirtual(device) == 0, "Could not remove synthetic controller");
    Require(!N::ClassicToolActive(), "Native controller editing opened a classic tool");
    controller->ClearAllMappings();
}
static void CheckConsole() {
    N::ClearConsole();
    const auto revision = N::ReadConsole().revision;
    N::SubmitConsole("   ");
    Require(N::ReadConsole().revision == revision, "Blank console command changed output");
    N::SubmitConsole("help");
    Require(
        N::ReadConsole().lines.back().text.find("get: Gets a console variable") != std::string::npos,
        "Native help lost registered commands"
    );
    N::SubmitConsole("set test.NativeConsole 23");
    N::SubmitConsole("get test.NativeConsole");
    Require(
        N::ReadConsole().lines.back().text.find("23") != std::string::npos,
        "Console did not execute actual settings commands"
    );
    N::SubmitConsole("not-a-command");
    Require(N::ReadConsole().lines.back().error, "Unknown console command not identified as an error");
    N::SubmitConsole("get");
    Require(
        N::ReadConsole().lines.back().text.find("Usage:") != std::string::npos,
        "Missing console arguments lost usage help"
    );
    N::SubmitConsole("set test.NativeConsole 9999999999999999999999999999999");
    Require(N::ReadConsole().lines.back().error, "Invalid numeric command escaped error handling");
    N::SubmitConsole("set test.NativeConsole 1\nreset");
    Require(CVarGetInteger("test.NativeConsole", 0) == 23, "Console executed a multiline paste");
    for (int i = 0; i < 125; ++i)
        N::SubmitConsole("unknown-" + std::to_string(i));
    Require(N::ReadConsole().history.size() == 100, "Console history is unbounded");
    auto console = Ship::Context::GetRawInstance()->GetConsole();
    console->AddCommand(
        "test-console-large",
        { [](auto, auto, std::string* output) {
             *output = std::string(40000, 'X');
             return 0;
         },
          "Synthetic output bound check" }
    );
    const auto logLevel = spdlog::get_level();
    spdlog::set_level(spdlog::level::off);
    for (int i = 0; i < 12; ++i)
        N::SubmitConsole("test-console-large");
    spdlog::set_level(logLevel);
    const auto bounded = N::ReadConsole();
    size_t bytes = 0;
    for (const auto& line : bounded.lines)
        bytes += line.text.size();
    Require(bytes <= 256 * 1024 && bounded.lines.size() <= 500, "Console output is unbounded");
    N::SubmitConsole("clear");
    Require(
        N::ReadConsole().lines.empty() && !N::ReadConsole().history.empty(), "Clear lost history or retained output"
    );
}
static void CheckSettingHelp(PaperboatGui::PaperboatMenu& menu) {
    size_t copied = 0;
    for (const auto& group : N::BuildNavigation())
        for (const auto& destination : group.pages)
            for (const auto& section : menu.NativePage(destination.key).sections)
                for (const auto& row : section.rows) {
                    const auto* widget = menu.NativeWidget(row.id);
                    if (!widget || !widget->options->tooltip || row.title == "Menu Theme"
                        || row.title == "Controller Bindings" || row.title == "Audio API")
                        continue;
                    Require(row.help == widget->options->tooltip, "Native setting lost its original tooltip");
                    ++copied;
                }
    Require(copied > 50, "Too few upstream setting descriptions were checked");
    const auto controls = menu.NativePage("menu/Settings/Controls");
    const auto stick = Find(controls, "Hide Joystick When Idle");
    Require(!stick.help.empty() && stick.detail.empty(), "Description still occupies the setting's value subtitle");
    const auto slider = N::Number("Help fixture", "test.NativeHelp", false, 0, 1, .1, .5, "A slider description.");
    Require(slider.help == "A slider description." && slider.detail.empty(), "Custom slider help was not preserved");
    const auto before = CVarGetInteger("gEnhancements.SprintButton", 0);
    CVarSetInteger("gSettings.DisableChanges", 1);
    const auto locked = Find(menu.NativePage("menu/Enhancements/Gameplay"), "Sprint Button");
    Require(!locked.enabled && !locked.help.empty(), "Disabled setting lost access to its description");
    Require(CVarGetInteger("gEnhancements.SprintButton", 0) == before, "Reading help changed a setting");
    CVarSetInteger("gSettings.DisableChanges", 0);
}
static void CheckMenuTheme(PaperboatGui::PaperboatMenu& menu) {
    const auto page = menu.NativePage("menu/Settings/General");
    const auto row = Find(page, "Menu Theme");
    Require(row.kind == N::Kind::Choice && row.choices.size() == 14, "Native Menu Theme choices are missing");
    const int original = CVarGetInteger("gSettings.Menu.Theme", UIWidgets::Colors::LightBlue);
    for (const auto& choice : row.choices) {
        const int before = CVarGetInteger("gSettings.Menu.Theme", UIWidgets::Colors::LightBlue);
        N::Enqueue([row, value = choice.value] { row.change(value); });
        Require(
            CVarGetInteger("gSettings.Menu.Theme", UIWidgets::Colors::LightBlue) == before,
            "Theme changed outside the frame queue"
        );
        N::Frame(menu);
        menu.UpdateElement();
        const auto theme = N::ReadThemeColor();
        const auto cursor = UIWidgets::ColorValues.at(menu.GetMenuThemeColor());
        Require(
            CVarGetInteger("gSettings.Menu.Theme", -1) == choice.value
                && theme == N::ThemeColor { cursor.x, cursor.y, cursor.z },
            "Native theme and touch-stick palette diverged"
        );
        Require(
            N::ThemeContrast(theme, N::ThemeForeground(theme)) >= 4.5, "Filled theme controls have unreadable labels"
        );
        for (const N::ThemeColor background : { N::ThemeColor { .09f, .12f, .17f }, N::ThemeColor { 1, 1, 1 } })
            Require(
                N::ThemeContrast(N::AccessibleThemeColor(theme, background), background) >= 4.5,
                "Theme accent is unreadable in light/dark appearance"
            );
    }
    const int selected = CVarGetInteger("gSettings.Menu.Theme", -1);
    row.change(999);
    Require(CVarGetInteger("gSettings.Menu.Theme", -1) == selected, "Theme selector accepted an invalid choice");
    CVarSetInteger("gSettings.Menu.Theme", -1);
    const auto fallback = UIWidgets::ColorValues.at(UIWidgets::Colors::LightBlue);
    Require(
        N::ReadThemeColor() == N::ThemeColor { fallback.x, fallback.y, fallback.z },
        "Invalid saved theme did not fall back safely"
    );
    CVarSetInteger("gSettings.Menu.Theme", original);
    menu.UpdateElement();
}
static void CheckValueViewer(PaperboatGui::PaperboatMenu& menu) {
    extern std::vector<ValueViewerTypes> enabledSettingsList;
    extern ImVec4 valueViewerBG;
    extern float valueViewerScale;
    extern ImGuiWindowFlags valueViewerWindowFlags;
    CVarSetFloat("gValueViewer.Scale", .9f);
    const auto page = menu.NativePage("menu/Dev Tools/Value Viewer");
    Require(page.id == "values/root" && page.sections.size() == 3, "Value Viewer still opens a classic tool");
    Require(CVarGetFloat("gValueViewer.Scale", 0) == .9f, "Opening Value Viewer changed the overlay size");
    auto change = [&](const char* title, double value) {
        N::Enqueue([row = Find(page, title), value] { row.change(value); });
        N::Frame(menu);
    };
    change("Show Value Viewer", 1);
    Require(CVarGetInteger("gWindows.ValueViewer", 0) == 1, "Value Viewer toggle did not enable the overlay");
    change("Floating Window", 1);
    Require(valueViewerWindowFlags & ImGuiWindowFlags_NoTitleBar, "Floating toggle did not update overlay flags");
    change("Floating Window", 0);
    Require(!(valueViewerWindowFlags & ImGuiWindowFlags_NoTitleBar), "Floating toggle did not restore title bar");
    change("Background Opacity", .73);
    Require(fabs(valueViewerBG.w - .73f) < .001, "Opacity did not reach the existing overlay");
    change("Overlay Size", 99);
    Require(fabs(valueViewerScale - 2.5f) < .001, "Overlay size was not clamped/applied");
    change("Show Current Map", 1);
    change("Show Mario's Position", 1);
    Require(enabledSettingsList.size() == 2, "Value toggles did not refresh overlay values");
    change("Show Current Map", 0);
    Require(
        enabledSettingsList.size() == 1 && enabledSettingsList[0] == VALUE_TYPE_POSITION,
        "Map toggle changed the wrong overlay value"
    );
    change("Show Value Viewer", 0);
    Require(!CVarGetInteger("gWindows.ValueViewer", 0), "Value Viewer toggle did not hide the overlay");
    Require(!N::ClassicToolActive(), "Editing Value Viewer opened classic settings");
}
static void CheckIosPickerBridge(PaperboatGui::PaperboatMenu& menu) {
    namespace fs = std::filesystem;
    Ship::FileBrowserRequest request;
    request.Title = "Import synthetic file";
    int callbacks = 0;
    const auto source = fs::temp_directory_path() / "paperreality-synthetic-picker.txt";
    std::ofstream(source) << "synthetic import";
    Paperboat::PickFile(request, [&](auto path) {
        Require(path.has_value() && fs::is_regular_file(*path), "Import path was removed before the reader ran");
        std::ifstream input(*path);
        std::string contents;
        std::getline(input, contents);
        Require(contents == "synthetic import", "Picked file contents changed");
        ++callbacks;
    });
    Require(
        bool(Paperboat::pickedFile) && !Ship::FileBrowserWindow::IsOpen(), "iOS import opened the hidden ImGui browser"
    );
    Paperboat::pickedFile(source.string());
    Require(callbacks == 0, "iOS picker callback ran outside the game queue");
    N::Frame(menu);
    Require(callbacks == 1 && !fs::exists(source), "Imported private copy was not cleaned up");
    Paperboat::PickFile(request, [&](auto path) {
        Require(!path, "Picker cancellation fabricated a path");
        ++callbacks;
    });
    Paperboat::pickedFile("");
    N::Frame(menu);
    Require(callbacks == 2, "Picker cancellation was not delivered exactly once");
    request.Save = true;
    request.DefaultName = "preset.json";
    Paperboat::PickFile(request, [&](auto path) {
        Require(path.has_value(), "Export did not provide a staging path");
        std::ofstream(*path) << "{}";
    });
    const fs::path exported(Paperboat::exportedPath);
    Require(
        exported.filename() == "preset.json" && fs::is_regular_file(exported), "Export did not stage the generated file"
    );
    Require(bool(Paperboat::exportedFile), "Export did not open the native Files picker");
    Paperboat::exportedFile(false);
    N::Frame(menu);
    Require(!fs::exists(exported.parent_path()), "Cancelled export left its private staging file behind");
}
static void CheckTextureSelection() {
    auto manager = Ship::Context::GetRawInstance()->GetResourceManager();
    manager->GetResourceLoader()->RegisterResourceFactory(
        std::make_shared<Fast::ResourceFactoryBinaryTextureV1>(), RESOURCE_FORMAT_BINARY, "Texture",
        static_cast<uint32_t>(Fast::ResourceType::Texture), 1
    );
    Require(manager->GetArchiveManager()->AddArchive("synthetic-textures.zip") != nullptr, "Fixture not mounted");
    const auto expectBase = [&](const char* name) {
        const std::string path = "__OTR__test-native/" + std::string(name);
        const auto image = N::LoadImage(path.c_str(), "__OTR__test-native/colors");
        Require(
            image.width == 2 && image.height == 1
                && image.rgba == std::vector<uint8_t> { 255, 0, 0, 255, 0, 255, 0, 255 },
            "Original indexed sprite or palette fallback incorrect"
        );
    };
    const auto expectHD = [&](const char* name, int width, int height, std::vector<uint8_t> color) {
        const std::string path = "__OTR__test-native/" + std::string(name);
        const auto image = N::LoadImage(path.c_str(), "__OTR__test-native/colors");
        Require(image.width == width && image.height == height, "HD sprite dimensions lost");
        Require(image.rgba.size() == size_t(width * height * 4), "HD pixel count incorrect");
        for (size_t i = 0; i < image.rgba.size(); ++i)
            Require(image.rgba[i] == color[i % 4], "HD sprite pixels changed");
    };
    manager->SetAltAssetsEnabled(false);
    expectBase("cached"); // Warm the base cache before enabling HD.
    manager->SetAltAssetsEnabled(true);
    expectHD("cached", 4, 2, { 0, 0, 255, 255 });
    expectHD("variant", 8, 4, { 255, 192, 0, 255 });
    expectHD("alias-only", 12, 6, { 255, 0, 255, 255 });
    expectHD("variant-alias", 12, 6, { 255, 0, 255, 255 });
    expectHD("raw-ci", 4, 2, { 0, 255, 255, 128 });
    for (const auto* name :
         { "partial", "unsupported", "short", "claimed", "zero", "oversized", "missing-target", "bad-meta" })
        expectBase(name);
    manager->SetAltAssetsEnabled(false);
    expectBase("cached"); // A cached replacement must not survive the toggle.
    expectBase("variant");
    Require(N::LoadImage("__OTR__test-native/alias-only").rgba.empty(), "Disabled HD alias still displayed");
    expectHD("base-alias", 12, 6, { 255, 0, 255, 255 });
    const auto ci8 = N::LoadImage("__OTR__test-native/ci8", "__OTR__test-native/ci8-colors");
    Require(ci8.rgba == std::vector<uint8_t> { 255, 255, 255, 255 }, "CI8 palette decoding incorrect");
    manager->SetAltAssetsEnabled(true);
    expectHD("cached", 4, 2, { 0, 0, 255, 255 });
    const auto report = N::SpriteTextureReport();
    Require(
        !manager->GetCachedResource("alt/ui/input/report-test", true),
        "Full presence audit loaded HD texture pixels into the resource cache"
    );
    Require(manager->IsAltAssetsEnabled(), "Exporting coverage changed the pack selection");
    Require(
        report.find("Selected replacement: alt/icons/badge/PowerJump") != std::string::npos,
        "Coverage report did not identify the selected HD sprite"
    );
    Require(
        report.find("Selected original: icons/badge/AutoJump") != std::string::npos,
        "Coverage report did not identify the original fallback"
    );
    Require(
        report.find("Rejected: alt/icons/badge/Berserker") != std::string::npos,
        "Coverage report did not explain an unusable replacement"
    );
    Require(
        report.find("alt/icons/badge/SuperJump -> test-native/aliases/target") != std::string::npos
            && report.find("[synthetic-textures.zip]; 12x6") != std::string::npos,
        "Coverage report lost the alias, pack name or replacement dimensions"
    );
    Require(
        report.find("Selected original: icons/badge/Kaiden") != std::string::npos
            && report.find("ui/input/report-test: HD candidate present: alt/ui/input/report-test") != std::string::npos,
        "Complete coverage missed Kaiden or a UI image outside the detailed sample"
    );
    Require(
        report.find("Registered Save Editor images (presence only; includes absent base files)") != std::string::npos
            && report.find("icons/badge/DeepFocus: no matching HD raster entry") != std::string::npos,
        "Registered editor images missing from the archive were not checked"
    );
    const auto catalog = ActorBlueprints::GetCatalog();
    Require(
        report.find("Blueprint characters: " + std::to_string(catalog.size())) != std::string::npos,
        "Blueprint coverage did not include the full catalog"
    );
    Require(
        report.find("Albino Dino\n  Sprite assets unavailable; no components inspected.") != std::string::npos,
        "Report did not explain unavailable Albino Dino components without game assets"
    );
    std::ostringstream probe;
    N::InspectImagePresence("__OTR__test-native/variant", "__OTR__test-native/colors", probe);
    N::InspectImagePresence("__OTR__test-native/alias-only", nullptr, probe);
    N::InspectImagePresence("__OTR__test-native/partial", "__OTR__test-native/colors", probe);
    Require(
        probe.str().find("HD candidate present: alt/test-native/variant@colors") != std::string::npos
            && probe.str().find("HD candidate present: alt/test-native/alias-only") != std::string::npos
            && probe.str().find("test-native/partial: no matching HD raster entry") != std::string::npos,
        "Presence checks did not respect palette variants, aliases or missing replacements"
    );
    Require(
        report.find("/tmp/") == std::string::npos && report.size() < 1024 * 1024,
        "Coverage report includes device directories or unexpected payload data"
    );
    manager->SetAltAssetsEnabled(false);
    const auto disabled = N::SpriteTextureReport();
    Require(
        !manager->IsAltAssetsEnabled() && disabled.find("Alternate assets: disabled") != std::string::npos
            && disabled.find("Selected replacement:") == std::string::npos,
        "Coverage report did not respect disabled alternate assets"
    );
}
int main() {
    try {
        GameEngine::Instance = new GameEngine();
        auto context = Ship::Context::GetRawInstance();
        context->InitAudio({ .SampleRate = 32000, .SampleLength = 1024, .DesiredBuffered = 1680 });
        PaperboatGui::SetupGuiElements();
        auto& menu = *PaperboatGui::mPaperboatMenu;
        N::Frame(menu);
        CheckSettingHelp(menu);
        CheckMenuTheme(menu);
        CheckConsole();
        CheckValueViewer(menu);
        CheckIosPickerBridge(menu);
        CheckControllerBindings(menu);
        int queued = 0;
        std::thread producer([&] {
            N::Enqueue([&] {
                ++queued;
                N::Enqueue([&] { ++queued; });
            });
        });
        producer.join();
        Require(queued == 0, "UIKit queue executed on producer thread");
        N::Frame(menu);
        Require(queued == 1, "Queue must defer nested actions to next frame");
        N::Frame(menu);
        Require(queued == 2, "Nested action lost");
        auto visibility = Find(N::InputViewerPage("viewer/root"), "Show Input Viewer");
        auto viewer = context->GetWindow()->GetGui()->GetGuiWindow("Input Viewer");
        visibility.change(1);
        Require(viewer->IsVisible(), "Input viewer window was not shown");
        visibility.change(0);
        Require(!viewer->IsVisible(), "Input viewer window was not hidden");
        const auto navigation = N::BuildNavigation();
        Require(navigation.size() == 4, "Expected four navigation groups");
        const auto empty = N::SaveEditorPage("save/root");
        Require(!Find(empty, "Player").enabled, "Save editing enabled without a save");
        Require(N::LoadImage("__OTR__sprites/not-present").rgba.empty(), "Missing sprite must remain blank");
        N::RememberPage("menu/Enhancements/Gameplay");
        Require(N::InitialPage() == "menu/Enhancements/Gameplay", "Navigation selection not restored");
        N::RememberPage("save/items");
        Require(N::InitialPage() == "menu/Enhancements/Gameplay", "Nested page overwrote navigation preference");
        gCurrentSaveFile.magicString[0] = 'T';
        gPlayerData.hardMaxHP = gPlayerData.curMaxHP = gPlayerData.curHP = 10;
        gPlayerData.hardMaxFP = gPlayerData.curMaxFP = gPlayerData.curFP = 5;
        gPlayerData.starPower = 257;
        gPlayerData.maxStarPower = 2;
        auto player = N::SaveEditorPage("save/player");
        Require(gPlayerData.starPower == 257, "Reading editor rounded fractional energy");
        auto coins = Find(player, "Coins");
        coins.change(9999);
        Require(gPlayerData.coins == 999, "Coins not clamped");
        gCurrentSaveFile.magicString[0] = 0;
        coins.change(0);
        Require(gPlayerData.coins == 999, "Stale editor wrote after save unloaded");
        gCurrentSaveFile.magicString[0] = 'T';
        auto gameplay = menu.NativePage("menu/Enhancements/Gameplay");
        auto sprint = Find(gameplay, "Sprint Button");
        sprint.change(1);
        Require(CVarGetInteger("gEnhancements.SprintButton", 0) == 1, "Native control did not write its setting");
        CVarSetInteger("gSettings.DisableChanges", 1);
        sprint.change(0);
        Require(CVarGetInteger("gEnhancements.SprintButton", 0) == 1, "Stale callback bypassed race lock");
        CVarSetInteger("gSettings.DisableChanges", 0);
        sprint.change(0);
        auto graphics = menu.NativePage("menu/Settings/Graphics");
        CVarSetInteger("gSettings.AdvancedResolution.Enabled", 1);
        graphics = menu.NativePage("menu/Settings/Graphics");
        auto width = Find(graphics, "Width");
        width.change(16);
        Require(CVarGetFloat("gSettings.AdvancedResolution.AspectRatioX", 0) == 16, "Custom aspect ratio not saved");
        std::queue<std::string> pending;
        std::set<std::string> visited;
        for (const auto& group : navigation)
            for (const auto& page : group.pages)
                pending.push(page.key);
        size_t rows = 0, portraits = 0;
        while (!pending.empty()) {
            auto key = pending.front();
            pending.pop();
            if (!visited.insert(key).second)
                continue;
            const auto page = N::BuildPage(key);
            Require(!page.title.empty(), "Missing page title");
            for (const auto& section : page.sections)
                for (const auto& row : section.rows) {
                    ++rows;
                    Require(!row.title.empty(), "Empty row title");
                    if (row.kind == N::Kind::Link) {
                        Require(!row.target.empty(), "Missing destination");
                        pending.push(row.target);
                    }
                    if (row.kind == N::Kind::Integer || row.kind == N::Kind::Decimal) {
                        Require(
                            std::isfinite(row.minimum) && std::isfinite(row.maximum) && row.minimum <= row.maximum,
                            "Invalid slider range"
                        );
                        Require(row.step > 0, "Invalid slider step");
                    }
                    if (row.portrait && portraits < 4) {
                        Require(row.portrait().rgba.empty(), "Unexpected game assets in fixture");
                        ++portraits;
                    }
                }
        }
        const auto catalog = ActorBlueprints::GetCatalog();
        Require(!catalog.empty(), "Blueprint catalog empty");
        const auto bp =
            N::BlueprintEditorPage("blueprint/actor/" + std::to_string(catalog.front().actorType) + "/combat");
        Require(!bp.sections.empty(), "Missing actor combat fields");
        const auto characters = N::BlueprintEditorPage("blueprint/category/-1");
        for (const auto* name : { "Fake Bowser", "Toy Tank", "Light Bulb" }) {
            const auto model = Find(characters, name);
            Require(
                !model.portrait && !model.symbol.empty() && model.detail.ends_with(" · Model"),
                "Model actors must have labeled icons, never an unrelated NPC sprite or blank portrait"
            );
            const auto detail = N::BuildPage(model.target);
            const auto info = Find(detail, name);
            Require(info.symbol == model.symbol && !info.portrait, "Model icon differs between list and detail");
        }
        CheckTextureSelection();
        std::cout << "PASS: " << visited.size() << " native pages, " << rows
                  << " rows; queued callbacks, race lock, navigation, save guards, resolution persistence, blank "
                     "portraits, HD texture selection, console, iOS file routing, controller capture and persistence\n";
        N::Stop();
        PaperboatGui::Destroy();
        gsFast3dWindow.reset();
        Ship::Context::DestroyInstance();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
