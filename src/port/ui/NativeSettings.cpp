#include "NativeSettings.h"
#include "PaperboatMenu.h"
#include "port/ShipInit.hpp"
#include "fast/Fast3dWindow.h"
#include "fast/resource/type/Texture.h"
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <ship/resource/archive/Archive.h>
#include <ship/window/gui/ConsoleWindow.h>
#include <algorithm>
#include <cmath>
#include <deque>
#include <exception>
#include <filesystem>
#include <mutex>
#include <sstream>

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

namespace {
    // Decode only validated buffers. A replacement that cannot be displayed must not
    // prevent the original sprite from being used instead.
    bool ValidTextureData(const std::shared_ptr<Fast::Texture>& texture) {
        if (!texture || !texture->ImageData)
            return false;
        if (texture->mImageBuffer) {
            const uintptr_t start = reinterpret_cast<uintptr_t>(texture->mImageBuffer->data());
            const uintptr_t data = reinterpret_cast<uintptr_t>(texture->ImageData);
            if (data < start || data - start > texture->mImageBuffer->size()
                || texture->ImageDataSize > texture->mImageBuffer->size() - (data - start))
                return false;
        }
        return true;
    }

    Image DecodeImage(const std::shared_ptr<Fast::Texture>& texture, const std::shared_ptr<Fast::Texture>& colors) {
        Image image;
        if (!ValidTextureData(texture) || !texture->Width || !texture->Height)
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
        const bool indexed =
            texture->Type == Fast::TextureType::Palette4bpp || texture->Type == Fast::TextureType::Palette8bpp;
        if (texture->Type != Fast::TextureType::RGBA16bpp && !indexed)
            return { };
        if (texture->Type == Fast::TextureType::RGBA16bpp && texture->ImageDataSize < count * 2)
            return { };
        const bool ci4 = texture->Type == Fast::TextureType::Palette4bpp;
        if (indexed && texture->ImageDataSize < (ci4 ? (count + 1) / 2 : count))
            return { };
        if (indexed
            && (!ValidTextureData(colors) || colors->Type != Fast::TextureType::RGBA16bpp
                || colors->ImageDataSize < (ci4 ? 32u : 512u)))
            return { };
        for (size_t i = 0; i < count; ++i) {
            const uint8_t index = ci4 ? ((i & 1) ? texture->ImageData[i / 2] & 15 : texture->ImageData[i / 2] >> 4)
                                      : (indexed ? texture->ImageData[i] : 0);
            const uint8_t* p = indexed ? colors->ImageData + index * 2 : texture->ImageData + i * 2;
            const uint16_t value = (p[0] << 8) | p[1];
            image.rgba[i * 4] = ((value >> 11) & 31) * 255 / 31;
            image.rgba[i * 4 + 1] = ((value >> 6) & 31) * 255 / 31;
            image.rgba[i * 4 + 2] = ((value >> 1) & 31) * 255 / 31;
            image.rgba[i * 4 + 3] = (value & 1) ? 255 : 0;
        }
        return image;
    }
} // namespace

static Image LoadImageImpl(const char* raster, const char* palette, std::ostream* report) {
    if (!raster || !*raster)
        return { };
    auto manager = Ship::Context::GetRawInstance()->GetResourceManager();
    if (!manager || !manager->GetArchiveManager())
        return { };
    const auto normalize = [](const char* name) {
        std::string path = name ? name : "";
        if (path.starts_with("__OTR__"))
            path.erase(0, 7);
        return path;
    };
    const std::string path = normalize(raster), palettePath = normalize(palette);
    const auto load = [&](const std::string& name) -> std::shared_ptr<Fast::Texture> {
        auto archive = manager->GetArchiveManager();
        if (name.empty() || (!archive->HasFile(name) && !archive->HasFile(name + ".meta"))) {
            if (report && !name.empty())
                *report << "  Missing: " << name << '\n';
            return nullptr;
        }
        try {
            // Keep base and HD cache keys separate, including after a pack toggle.
            auto texture = std::dynamic_pointer_cast<Fast::Texture>(manager->LoadResource(name, true));
            if (report) {
                *report << "  Found: " << name;
                if (texture) {
                    const auto resolved = texture->GetInitData()->Path;
                    if (resolved != name)
                        *report << " -> " << resolved;
                    auto source = archive->GetArchiveFromFile(resolved);
                    if (!source)
                        source = archive->GetArchiveFromFile(name + ".meta");
                    if (source)
                        *report << " [" << std::filesystem::path(source->GetPath()).filename().string() << ']';
                    *report << "; " << texture->Width << 'x' << texture->Height << "; type=" << int(texture->Type)
                            << "; flags=" << texture->Flags << "; bytes=" << texture->ImageDataSize;
                } else {
                    *report << "; not a readable texture";
                }
                *report << '\n';
            }
            return texture;
        } catch (const std::exception&) {
            // Malformed pack metadata must still allow a base-game portrait.
            if (report)
                *report << "  Unreadable metadata/resource: " << name << '\n';
            return nullptr;
        }
    };
    const auto decode = [&](const std::string& name) {
        auto texture = load(name);
        std::shared_ptr<Fast::Texture> colors;
        if (texture && !(texture->Flags & TEX_FLAG_LOAD_AS_RAW)
            && (texture->Type == Fast::TextureType::Palette4bpp || texture->Type == Fast::TextureType::Palette8bpp))
        {
            const size_t required = texture->Type == Fast::TextureType::Palette4bpp ? 32 : 512;
            if (manager->IsAltAssetsEnabled() && !palettePath.empty())
                colors = load("alt/" + palettePath);
            if (!ValidTextureData(colors) || colors->Type != Fast::TextureType::RGBA16bpp
                || colors->ImageDataSize < required)
                colors = load(palettePath);
        }
        auto image = DecodeImage(texture, colors);
        if (report && texture && image.rgba.empty())
            *report << "  Rejected: " << name << " (invalid buffer, dimensions, palette or unsupported format)\n";
        return image;
    };
    if (manager->IsAltAssetsEnabled() && !path.starts_with("alt/")) {
        // Match the renderer's per-palette replacement naming convention.
        if (!palettePath.empty()) {
            const std::string variant = "alt/" + path + "@" + palettePath.substr(palettePath.find_last_of('/') + 1);
            if (auto image = decode(variant); !image.rgba.empty()) {
                if (report)
                    *report << "  Selected replacement: " << variant << '\n';
                return image;
            }
        }
        if (auto image = decode("alt/" + path); !image.rgba.empty()) {
            if (report)
                *report << "  Selected replacement: alt/" << path << '\n';
            return image;
        }
    }
    auto image = decode(path);
    if (report)
        *report << (image.rgba.empty() ? "  No readable sprite: " : "  Selected original: ") << path << '\n';
    return image;
}

Image LoadImage(const char* raster, const char* palette) {
    return LoadImageImpl(raster, palette, nullptr);
}

std::string SpriteTextureReport() {
    std::ostringstream report;
    report << "PaperReality sprite texture coverage\n"
              "Metadata only: no images, game contents, saves or device folder paths.\n"
              "Checks the reported badge/item icons, working comparison icons and companions.\n\n";
    auto manager = Ship::Context::GetRawInstance()->GetResourceManager();
    if (!manager || !manager->GetArchiveManager())
        return report.str() + "Resource manager unavailable.\n";
    const bool enabled = manager->IsAltAssetsEnabled();
    report << "Alternate assets: " << (enabled ? "enabled" : "disabled") << '\n';
    if (!enabled)
        report << "Replacements are not selected while disabled. Enable Alternate Assets and export again.\n";
    report << "Mounted archives containing alternate assets:\n";
    size_t packs = 0;
    const auto mounted = manager->GetArchiveManager()->GetArchives();
    for (const auto& archive : *mounted) {
        size_t entries = 0;
        for (const auto& [hash, name] : *archive->ListFiles())
            if (name.starts_with("alt/"))
                ++entries;
        if (entries) {
            ++packs;
            report << "  " << std::filesystem::path(archive->GetPath()).filename().string() << ": " << entries
                   << " alternate entries, priority " << archive->GetPriority() << '\n';
        }
    }
    if (!packs)
        report << "  None\n";
    const char* paths[] = { "icons/badge/AutoJump",
                            "icons/badge/SuperJump",
                            "icons/badge/AutoSmash",
                            "icons/badge/Berserker",
                            "icons/badge/RightOn",
                            "icons/badge/HealthyHealthy",
                            "icons/badge/EarthquakeJump",
                            "icons/battle/InsecticideHerb",
                            "icons/battle/PleaseComeBack",
                            "icons/badge/PowerJump",
                            "icons/badge/ZapTap",
                            "icons/badge/MegaJump",
                            "icons/badge/DamageDodge",
                            "icons/badge/DeepFocus",
                            "ui/goombario",
                            "ui/kooper",
                            "ui/bombette",
                            "ui/parakarry",
                            "ui/bow",
                            "ui/watt",
                            "ui/sushie",
                            "ui/lakilester" };
    for (const char* path : paths) {
        report << '\n' << path << '\n';
        // Use the actual thumbnail resolver, including the original cache and
        // palette-variant/alias rules. Never change the enabled pack selection.
        LoadImageImpl(path, (std::string(path) + ".pal").c_str(), &report);
    }
    report << "\nFake Bowser, Toy Tank and Light Bulb use models, not NPC sprite portraits.\n";
    return report.str();
}
} // namespace Paperboat::NativeSettings
