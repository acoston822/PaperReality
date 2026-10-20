#pragma once

#include <cstdint>
#include <functional>
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
};
struct Destination {
    std::string key, title, symbol;
};
struct Group {
    std::string title, symbol;
    std::vector<Destination> pages;
};

void Enqueue(std::function<void()> action);
void Frame(PaperboatGui::PaperboatMenu& menu);
void Stop();
void Close();
std::string InitialPage();
void RememberPage(const std::string& key);
void OpenClassic(const std::string& group, const std::string& page);
void ReturnToNative();
bool ClassicToolActive();
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
// On-demand metadata only; never includes image bytes or device folder paths.
std::string SpriteTextureReport();
Image LoadBlueprintPortrait(const ActorBlueprint* blueprint);
Page SaveEditorPage(const std::string& key);
Page BlueprintEditorPage(const std::string& key);
Page ModPage(const std::string& key);
Page InputViewerPage(const std::string& key);
Page ShaderPage();

#if defined(__IOS__)
void SetPresented(bool presented, std::vector<Group> navigation = { });
bool IsPresented();
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
