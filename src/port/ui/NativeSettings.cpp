#include "NativeSettings.h"
#include "PaperboatMenu.h"
#include "port/ShipInit.hpp"
#include "fast/Fast3dWindow.h"
#include "fast/resource/type/Texture.h"
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <ship/window/gui/ConsoleWindow.h>
#include <algorithm>
#include <cmath>
#include <deque>
#include <mutex>

namespace Paperboat::NativeSettings {
namespace {
    std::mutex queueMutex;
    std::deque<std::function<void()>> actions;
    PaperboatGui::PaperboatMenu* owner = nullptr;
    bool classic = false;
    constexpr int kInputBlock = 0x50425549;
    bool blocked = false;
}

void Enqueue(std::function<void()> action) {
    std::lock_guard<std::mutex> lock(queueMutex);
    actions.push_back(std::move(action));
}

void Frame(PaperboatGui::PaperboatMenu& menu) {
    owner = &menu;
    std::deque<std::function<void()>> ready;
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        ready.swap(actions);
    }
    for (auto& action : ready)
        action();
#if defined(__IOS__)
    const bool wanted = menu.IsVisible() && !classic && CVarGetInteger("gSettings.Menu.UseUIKit", 1);
    auto deck = Ship::Context::GetRawInstance()->GetControlDeck();
    if (wanted && !blocked) {
        deck->BlockGameInput(kInputBlock);
        blocked = true;
    }
    if (!wanted && blocked) {
        deck->UnblockGameInput(kInputBlock);
        blocked = false;
    }
    if (!menu.IsVisible())
        classic = false;
    if (wanted != IsPresented())
        SetPresented(wanted, wanted ? BuildNavigation() : std::vector<Group> { });
#endif
}

void Stop() {
    SetPresented(false);
    std::lock_guard<std::mutex> lock(queueMutex);
    actions.clear();
    owner = nullptr;
}
void Close() {
    if (owner)
        owner->Hide();
}
std::string InitialPage() {
    const std::string group = CVarGetString("gSettings.Menu.ActiveHeader", "Settings");
    for (const auto& entry : BuildNavigation())
        if (entry.title == group) {
            const std::string cvar =
                "gSettings.Menu." + std::string(group == "Dev Tools" ? "DevTools" : group) + "SidebarSection";
            const std::string key = "menu/" + group + "/" + CVarGetString(cvar.c_str(), "General");
            for (const auto& page : entry.pages)
                if (page.key == key)
                    return key;
            if (!entry.pages.empty())
                return entry.pages.front().key;
        }
    return "menu/Settings/General";
}
void RememberPage(const std::string& key) {
    const auto separator = key.find('/', 5);
    if (!key.starts_with("menu/") || separator == std::string::npos)
        return;
    const std::string group = key.substr(5, separator - 5), page = key.substr(separator + 1);
    const std::string cvar =
        "gSettings.Menu." + std::string(group == "Dev Tools" ? "DevTools" : group) + "SidebarSection";
    CVarSetString("gSettings.Menu.ActiveHeader", group.c_str());
    CVarSetString(cvar.c_str(), page.c_str());
    SaveSettings();
}

void OpenClassic(const std::string& group, const std::string& page) {
    classic = true;
    CVarSetString("gSettings.Menu.ActiveHeader", group.c_str());
    const char* key = group == "Settings" ? "gSettings.Menu.SettingsSidebarSection"
        : group == "Shaders"              ? "gSettings.Menu.ShadersSidebarSection"
                                          : "gSettings.Menu.DevToolsSidebarSection";
    CVarSetString(key, page.c_str());
    SetPresented(false);
}
void ReturnToNative() {
    classic = false;
    CVarSetInteger("gSettings.Menu.UseUIKit", 1);
}
bool ClassicToolActive() {
    return classic;
}

Page BuildPage(const std::string& key) {
    if (key.starts_with("save/"))
        return SaveEditorPage(key);
    if (key.starts_with("blueprint/"))
        return BlueprintEditorPage(key);
    if (key.starts_with("mods/"))
        return ModPage(key);
    if (key.starts_with("viewer/"))
        return InputViewerPage(key);
    if (key == "shaders")
        return ShaderPage();
    return owner ? owner->NativePage(key) : Page { key, "Settings", "", { } };
}
std::vector<Group> BuildNavigation() {
    return owner ? owner->NativeNavigation() : std::vector<Group> { };
}

void SaveSettings(const std::string& cvar) {
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    if (!cvar.empty())
        ShipInit::Init(cvar.c_str());
}

Row Toggle(std::string title, std::string cvar, bool defaultValue, std::string detail) {
    Row row;
    row.title = std::move(title);
    row.id = cvar;
    row.kind = Kind::Toggle;
    row.value = CVarGetInteger(cvar.c_str(), defaultValue);
    row.detail = std::move(detail);
    row.change = [cvar](double value) {
        CVarSetInteger(cvar.c_str(), value != 0);
        SaveSettings(cvar);
    };
    return row;
}
Row Number(
    std::string title,
    std::string cvar,
    bool integer,
    double min,
    double max,
    double step,
    double defaultValue,
    std::string detail
) {
    Row row;
    row.title = std::move(title);
    row.id = cvar;
    row.kind = integer ? Kind::Integer : Kind::Decimal;
    row.minimum = min;
    row.maximum = max;
    row.step = step;
    row.detail = std::move(detail);
    row.value = integer ? CVarGetInteger(cvar.c_str(), defaultValue) : CVarGetFloat(cvar.c_str(), defaultValue);
    row.change = [cvar, integer, min, max, step](double value) {
        if (!std::isfinite(value))
            return;
        value = std::clamp(value, min, max);
        if (step > 0)
            value = std::clamp(min + std::round((value - min) / step) * step, min, max);
        if (integer)
            CVarSetInteger(cvar.c_str(), std::lround(value));
        else
            CVarSetFloat(cvar.c_str(), value);
        SaveSettings(cvar);
    };
    return row;
}
Row Select(std::string title, std::string cvar, int defaultValue, std::vector<Choice> choices) {
    Row row;
    row.title = std::move(title);
    row.id = cvar;
    row.kind = Kind::Choice;
    row.value = CVarGetInteger(cvar.c_str(), defaultValue);
    row.choices = std::move(choices);
    row.change = [cvar, choices = row.choices](double value) {
        if (!std::isfinite(value))
            return;
        const int selected = std::lround(value);
        if (std::none_of(choices.begin(), choices.end(), [selected](const auto& choice) {
                return choice.value == selected;
            }))
            return;
        CVarSetInteger(cvar.c_str(), selected);
        SaveSettings(cvar);
    };
    return row;
}
Row Link(std::string title, std::string target, std::string symbol) {
    Row row;
    row.title = std::move(title);
    row.id = target;
    row.kind = Kind::Link;
    row.target = std::move(target);
    row.symbol = std::move(symbol);
    return row;
}

Image LoadImage(const char* raster, const char* palette) {
    Image image;
    if (!raster || !*raster)
        return image;
    auto manager = Ship::Context::GetRawInstance()->GetResourceManager();
    if (!manager)
        return image;
    std::string path(raster);
    if (path.starts_with("__OTR__"))
        path.erase(0, 7);
    auto archive = manager->GetArchiveManager();
    if (!archive || (!archive->HasFile(path) && !(manager->IsAltAssetsEnabled() && archive->HasFile("alt/" + path))))
        return image;
    auto texture = std::dynamic_pointer_cast<Fast::Texture>(manager->LoadResource(raster));
    if (!texture || !texture->ImageData)
        texture = std::dynamic_pointer_cast<Fast::Texture>(manager->LoadResource(raster, true));
    if (!texture || !texture->ImageData || !texture->Width || !texture->Height)
        return image;
    const size_t count = size_t(texture->Width) * texture->Height;
    if (count > 4096 * 4096)
        return image;
    image.width = texture->Width;
    image.height = texture->Height;
    image.rgba.resize(count * 4);
    if ((texture->Flags & TEX_FLAG_LOAD_AS_RAW) || texture->Type == Fast::TextureType::RGBA32bpp) {
        if (texture->ImageDataSize < count * 4)
            return { };
        std::copy_n(texture->ImageData, count * 4, image.rgba.data());
        return image;
    }
    std::shared_ptr<Fast::Texture> colors;
    if (texture->Type == Fast::TextureType::Palette4bpp && palette && *palette) {
        colors = std::dynamic_pointer_cast<Fast::Texture>(manager->LoadResource(palette));
        if (!colors || colors->Type != Fast::TextureType::RGBA16bpp || colors->ImageDataSize < 32)
            colors = std::dynamic_pointer_cast<Fast::Texture>(manager->LoadResource(palette, true));
    }
    if (texture->Type != Fast::TextureType::RGBA16bpp && !colors)
        return { };
    if (texture->Type == Fast::TextureType::RGBA16bpp && texture->ImageDataSize < count * 2)
        return { };
    if (texture->Type == Fast::TextureType::Palette4bpp && texture->ImageDataSize < (count + 1) / 2)
        return { };
    if (colors && (!colors->ImageData || colors->ImageDataSize < 32))
        return { };
    for (size_t i = 0; i < count; ++i) {
        const uint8_t index = texture->Type == Fast::TextureType::Palette4bpp
            ? ((i & 1) ? texture->ImageData[i / 2] & 15 : texture->ImageData[i / 2] >> 4)
            : 0;
        const uint8_t* p = colors ? colors->ImageData + index * 2 : texture->ImageData + i * 2;
        const uint16_t value = (p[0] << 8) | p[1];
        image.rgba[i * 4] = ((value >> 11) & 31) * 255 / 31;
        image.rgba[i * 4 + 1] = ((value >> 6) & 31) * 255 / 31;
        image.rgba[i * 4 + 2] = ((value >> 1) & 31) * 255 / 31;
        image.rgba[i * 4 + 3] = (value & 1) ? 255 : 0;
    }
    return image;
}
} // namespace Paperboat::NativeSettings
