#pragma once

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

namespace PaperboatGui {
class PaperboatMenu;
}
struct ActorBlueprint;

namespace Paperboat::NativeSettings {
enum class Kind {
    Info,
    Toggle,
    Integer,
    Decimal,
    Choice,
    Action,
    Link,
    Color,
    Text
};
struct Choice {
    int value;
    std::string title;
};
struct Image {
    int width = 0, height = 0;
    std::vector<uint8_t> rgba;
};
struct ThemeColor {
    float red = 0, green = .24f, blue = .8f;
    bool operator==(const ThemeColor&) const = default;
};
// Read the existing menu/touch-stick palette on the game-frame queue.
ThemeColor ReadThemeColor();
struct Row {
    std::string id, title, detail, symbol, target, text;
    Kind kind = Kind::Info;
    double value = 0, minimum = 0, maximum = 1, step = 1;
    bool enabled = true, destructive = false, percentage = false, alpha = true;
    std::vector<Choice> choices;
    // These callbacks are only executed by the render-frame queue, never UIKit.
    std::function<void(double)> change;
    std::function<void()> activate;
    std::function<void(std::string)> submit;
    std::function<Image()> portrait;
};
struct Section {
    std::string title, footer;
    std::vector<Row> rows;
};
struct Page {
    std::string id, title, subtitle;
    std::vector<Section> sections;
    bool searchable = false;
    bool returnToParent = false;
    bool capturesControllerInput = false;
};
struct Destination {
    std::string key, title, symbol;
};
struct Group {
    std::string title, symbol;
    std::vector<Destination> pages;
};
struct ConsoleLine {
    std::string text;
    bool command = false, error = false;
};
struct ConsoleSnapshot {
    uint64_t revision = 0;
    std::vector<ConsoleLine> lines;
    std::vector<std::string> history;
};
// Read and execute on the game-frame queue; UIKit receives immutable snapshots.
ConsoleSnapshot ReadConsole();
void SubmitConsole(std::string command);
void ClearConsole();

enum class EventPriority {
    Low,
    Normal,
    High,
    Unknown
};
struct EventCaller {
    std::string path;
    int line = 0;
    uint64_t calls = 0;
    bool operator==(const EventCaller&) const = default;
};
struct EventListener {
    int64_t id = 0;
    std::string path;
    int line = 0;
    EventPriority priority = EventPriority::Unknown;
    bool operator==(const EventListener&) const = default;
};
struct EventInfo {
    int32_t id = 0;
    std::string name;
    std::vector<EventCaller> callers;
    std::vector<EventListener> listeners;
    bool operator==(const EventInfo&) const = default;
};
struct EventSnapshot {
    bool available = false, recordsCallSites = false;
    std::vector<EventInfo> events;
    bool operator==(const EventSnapshot&) const = default;
};
// Copy event metadata on the game-frame queue. No engine pointers cross to UIKit.
EventSnapshot ReadEvents();

void Enqueue(std::function<void()> action);
void Frame(PaperboatGui::PaperboatMenu& menu);
void Stop();
void Close();
std::string InitialPage();
void RememberPage(const std::string& key);
Page BuildPage(const std::string& key);
std::vector<Group> BuildNavigation();
Row Toggle(std::string title, std::string cvar, bool defaultValue, std::string detail = "");
Row Number(
    std::string title,
    std::string cvar,
    bool integer,
    double minimum,
    double maximum,
    double step,
    double defaultValue,
    std::string detail = ""
);
Row Select(std::string title, std::string cvar, int defaultValue, std::vector<Choice> choices);
Row Link(std::string title, std::string target, std::string symbol = "chevron.right");
void SaveSettings(const std::string& cvar = "");
Image LoadImage(const char* raster, const char* palette = "");
// Uses the same decoder as thumbnails, recording resolution/selection metadata only.
Image InspectImage(const char* raster, const char* palette, std::ostream& report);
// Presence checks do not decode or cache HD texture pixels.
void InspectImagePresence(const char* raster, const char* palette, std::ostream& report);
void AppendBlueprintTextureCoverage(std::ostream& report);
void AppendSaveEditorTextureCoverage(std::ostream& report);
// On-demand metadata only; never includes image bytes or device folder paths.
std::string SpriteTextureReport();
Image LoadBlueprintPortrait(const ActorBlueprint* blueprint);
Page SaveEditorPage(const std::string& key);
Page BlueprintEditorPage(const std::string& key);
Page ModPage(const std::string& key);
Page InputViewerPage(const std::string& key);
Page ValueViewerPage();
Page ShaderPage();
Page ControllerBindingsPage(const std::string& key);
void UpdateControllerBindings(bool visible);
void CancelBindingCapture(const std::string& key = "");

#if defined(__IOS__)
void SetThemeColor(ThemeColor color);
void SetPresented(bool presented, std::vector<Group> navigation = { });
bool IsPresented();
// Main/render thread only. Services UIKit during an existing game-frame wait;
// returns false when the native menu is inactive so normal pacing is retained.
bool WaitForUi(double seconds);
#if defined(PAPERREALITY_UI_DIAGNOSTICS) && PAPERREALITY_UI_DIAGNOSTICS
void BeginRenderTiming(int targetFps, bool metalPacing);
void EndRenderTiming();
void RecordDrawableWait(double seconds);
#endif
bool Alert(
    std::string title,
    std::string message,
    std::string first,
    std::string second,
    std::function<void()> firstAction,
    std::function<void()> secondAction
);
#else
inline void SetPresented(bool, std::vector<Group> = { }) {
}
inline bool IsPresented() {
    return false;
}
inline bool Alert(std::string, std::string, std::string, std::string, std::function<void()>, std::function<void()>) {
    return false;
}
#endif
} // namespace Paperboat::NativeSettings
