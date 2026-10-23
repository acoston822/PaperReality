#include "PaperboatMenu.h"
#include "PaperboatGui.hpp"
#include "NativeSettings.h"
#include <ship/window/gui/ConsoleWindow.h>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace Paperboat::NativeSettings {
void AppendResolutionRows(Page& page);
}

namespace PaperboatGui {
namespace N = Paperboat::NativeSettings;
namespace {
    std::vector<std::string> Split(const std::string& value) {
        std::stringstream stream(value);
        std::vector<std::string> result;
        std::string part;
        while (std::getline(stream, part, '/'))
            result.push_back(part);
        return result;
    }
    bool MobileSetting(const WidgetInfo& widget) {
        static const std::vector<std::string> hidden = { "Cursor Always Visible",
                                                         "ImGui Menu Scaling",
                                                         "Menu Background Opacity",
                                                         "Menu Controller Navigation",
                                                         "Search In Sidebar",
                                                         "Search Input Autofocus",
                                                         "Toggle Fullscreen",
                                                         "Renderer API (Needs reload)",
                                                         "Windowed Fullscreen",
                                                         "Allow multi-windows",
                                                         "Popout Menu",
                                                         "EXPERIMENTAL" };
        return std::find(hidden.begin(), hidden.end(), widget.name) == hidden.end();
    }
    std::string PageSymbol(const std::string& page) {
        if (page == "General")
            return "gearshape";
        if (page == "Audio")
            return "speaker.wave.2";
        if (page == "Controls")
            return "gamecontroller";
        if (page == "Graphics")
            return "display";
        if (page == "Input Viewer")
            return "waveform.path";
        if (page == "Notifications")
            return "bell";
        if (page == "Mod Menu")
            return "puzzlepiece.extension";
        if (page == "Cheats")
            return "star";
        if (page == "Gameplay")
            return "gamecontroller";
        if (page == "Status Bar" || page == "Stats")
            return "chart.bar";
        if (page == "Console")
            return "terminal";
        if (page == "Event Debugger")
            return "ladybug";
        if (page == "Value Viewer")
            return "eye";
        if (page == "Save Editor")
            return "externaldrive";
        if (page == "Blueprint Editor")
            return "square.stack.3d.up";
        return "slider.horizontal.3";
    }
    std::string CleanTitle(std::string title) {
        // Remove the legacy printf value suffix; UIKit has its own value label.
        const auto percent = title.find('%');
        if (percent != std::string::npos)
            title.erase(percent);
        while (!title.empty() && (title.back() == ':' || title.back() == ' '))
            title.pop_back();
        // FontAwesome prefix glyphs do not belong in the system font.
        if (!title.empty() && static_cast<unsigned char>(title[0]) >= 0xE0) {
            const auto space = title.find(' ');
            if (space != std::string::npos)
                title.erase(0, space + 1);
        }
        return title;
    }
}

std::vector<N::Group> PaperboatMenu::NativeNavigation() {
    std::vector<N::Group> result;
    for (const auto& name : menuOrder) {
        N::Group group { name,
                         name == "Settings"           ? "gearshape"
                             : name == "Enhancements" ? "sparkles"
                             : name == "Shaders"      ? "slider.horizontal.3"
                                                      : "wrench.and.screwdriver",
                         { } };
        for (const auto& page : menuEntries.at(name).sidebarOrder) {
            if (page == "Search")
                continue;
            group.pages.push_back(
                { "menu/" + name + "/" + page, name == "Enhancements" && page == "Graphics" ? "Visuals" : page,
                  PageSymbol(page) }
            );
        }
        result.push_back(std::move(group));
    }
    return result;
}

bool PaperboatMenu::PrepareNativeWidget(WidgetInfo& widget) {
    if (widget.preFunc) {
        widget.ResetDisables();
        widget.preFunc(widget);
    }
    if (widget.isHidden || !MobileSetting(widget))
        return false;
    return true;
}

WidgetInfo* PaperboatMenu::NativeWidget(const std::string& key) {
    auto parts = Split(key);
    if (parts.size() != 5 || parts[0] != "widget" || !menuEntries.contains(parts[1]))
        return nullptr;
    auto& group = menuEntries.at(parts[1]);
    if (!group.sidebars.contains(parts[2]))
        return nullptr;
    try {
        const size_t column = std::stoul(parts[3]), index = std::stoul(parts[4]);
        auto& columns = group.sidebars.at(parts[2]).columnWidgets;
        return column < columns.size() && index < columns[column].size() ? &columns[column][index] : nullptr;
    } catch (...) {
        return nullptr;
    }
}

void PaperboatMenu::ApplyNativeWidget(const std::string& key, double value) {
    for (auto& [reason, info] : disabledMap)
        info.active = info.evaluation(info);
    WidgetInfo* ptr = NativeWidget(key);
    if (!ptr || !PrepareNativeWidget(*ptr))
        return;
    auto& w = *ptr;
    if (w.options->disabled || !w.activeDisables.empty()
        || (w.raceDisable && CVarGetInteger(CVAR_SETTING("DisableChanges"), 0)))
        return;
    const bool cvar = w.type == WIDGET_CVAR_CHECKBOX || w.type == WIDGET_CVAR_COMBOBOX
        || w.type == WIDGET_CVAR_SLIDER_INT || w.type == WIDGET_CVAR_SLIDER_FLOAT || w.type == WIDGET_CVAR_COLOR_PICKER;
    if (!std::isfinite(value))
        return;
    switch (w.type) {
        case WIDGET_CHECKBOX:
        case WIDGET_CVAR_CHECKBOX:
            if (cvar)
                CVarSetInteger(w.cVar, value != 0);
            else
                *std::get<bool*>(w.valuePointer) = value != 0;
            break;
        case WIDGET_COMBOBOX:
        case WIDGET_CVAR_COMBOBOX: {
            const auto options = std::static_pointer_cast<UIWidgets::ComboboxOptions>(w.options);
            const int selected = std::lround(value);
            if (!options->comboMap.contains(selected))
                return;
            if (cvar)
                CVarSetInteger(w.cVar, selected);
            else
                *std::get<int32_t*>(w.valuePointer) = selected;
            break;
        }
        case WIDGET_SLIDER_INT:
        case WIDGET_CVAR_SLIDER_INT: {
            const auto o = std::static_pointer_cast<UIWidgets::IntSliderOptions>(w.options);
            int v = std::clamp(int(std::lround(value)), o->min, o->max);
            if (o->step > 1)
                v = std::clamp(o->min + int(std::lround(double(v - o->min) / o->step)) * o->step, o->min, o->max);
            if (cvar)
                CVarSetInteger(w.cVar, v);
            else
                *std::get<int32_t*>(w.valuePointer) = v;
            break;
        }
        case WIDGET_SLIDER_FLOAT:
        case WIDGET_CVAR_SLIDER_FLOAT: {
            const auto o = std::static_pointer_cast<UIWidgets::FloatSliderOptions>(w.options);
            float v = std::clamp(float(value), o->min, o->max);
            if (cvar)
                CVarSetFloat(w.cVar, v);
            else
                *std::get<float*>(w.valuePointer) = v;
            break;
        }
        case WIDGET_CVAR_COLOR_PICKER: {
            const uint32_t rgba = static_cast<uint32_t>(value);
            CVarSetColor(w.cVar, { uint8_t(rgba >> 24), uint8_t(rgba >> 16), uint8_t(rgba >> 8), uint8_t(rgba) });
            break;
        }
        case WIDGET_WINDOW_BUTTON: {
            auto window = Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetGuiWindow(w.windowName);
            if (window) {
                if (value)
                    window->Show();
                else
                    window->Hide();
            }
            break;
        }
        case WIDGET_BUTTON:
            break;
        default:
            return;
    }
    if (cvar)
        N::SaveSettings(w.cVar);
    if (w.callback)
        w.callback(w);
    if (w.postFunc)
        w.postFunc(w);
}

N::Page PaperboatMenu::NativePage(const std::string& key) {
    const auto parts = Split(key);
    if (parts.size() != 3 || parts[0] != "menu" || !menuEntries.contains(parts[1]))
        return { key, "Settings", "", { } };
    const auto& group = parts[1];
    const auto& page = parts[2];
    if (!menuEntries.at(group).sidebars.contains(page))
        return { key, "Settings", "", { } };
    if (group == "Settings" && page == "Input Viewer")
        return N::InputViewerPage("viewer/root");
    if (group == "Settings" && page == "Mod Menu")
        return N::ModPage("mods/root");
    if (group == "Shaders")
        return N::ShaderPage();
    if (group == "Dev Tools") {
        if (page == "Save Editor")
            return N::SaveEditorPage("save/root");
        if (page == "Blueprint Editor")
            return N::BlueprintEditorPage("blueprint/root");
        if (page == "Event Debugger")
            return { key, "Event Debugger", "", { } };
        if (page == "Value Viewer")
            return N::ValueViewerPage();
        if (page == "Stats") {
            N::Row fps;
            fps.title = "Frame Rate";
            fps.detail = std::to_string(int(ImGui::GetIO().Framerate)) + " FPS";
            N::Row size;
            size.title = "Display";
            size.detail = std::to_string(int(ImGui::GetIO().DisplaySize.x)) + " × "
                + std::to_string(int(ImGui::GetIO().DisplaySize.y));
            return { key, "Stats", "", { { "Display", "", { fps, size } } } };
        }
        if (page == "Console") {
            return { key, "Console", "Enter help to list all available commands.", { } };
        }
    }

    for (auto& [reason, info] : disabledMap)
        info.active = info.evaluation(info);
    auto& updates = MenuInit::GetUpdateFuncs();
    if (updates.contains(group) && updates.at(group).contains(page))
        for (auto& update : updates.at(group).at(page))
            update();
    N::Page result { key, group == "Enhancements" && page == "Graphics" ? "Visuals" : page, group, { } };
    N::Section section;
    auto finish = [&] {
        if (!section.rows.empty())
            result.sections.push_back(std::move(section));
        section = { };
    };
    auto& columns = menuEntries.at(group).sidebars.at(page).columnWidgets;
    for (size_t c = 0; c < columns.size(); ++c)
        for (size_t i = 0; i < columns[c].size(); ++i) {
            auto& w = columns[c][i];
            if (!PrepareNativeWidget(w))
                continue;
            if (w.type == WIDGET_SEPARATOR || w.type == WIDGET_SEPARATOR_TEXT) {
                finish();
                if (w.type == WIDGET_SEPARATOR_TEXT)
                    section.title = CleanTitle(w.name);
                continue;
            }
            if (w.type == WIDGET_SEARCH || w.type == WIDGET_VIDEO_BACKEND)
                continue;
            if (w.type == WIDGET_CUSTOM) {
                if (w.name == "AspectRatioCustom") {
                    finish();
                    N::AppendResolutionRows(result);
                }
                continue;
            }
            N::Row row;
            row.id = "widget/" + group + "/" + page + "/" + std::to_string(c) + "/" + std::to_string(i);
            row.title = CleanTitle(w.name);
            row.detail = w.options->tooltip ? w.options->tooltip : "";
            if (w.name == "Menu Theme")
                row.detail = "Changes the settings accent color and touch-stick cursor color.";
            else if (w.name == "DX Debug Menu")
                row.detail =
                    "Uses the game's mapped N64 buttons:\n"
                    "Open: D-pad Left (main menu), Right (script debugger).\n"
                    "D-pad Up/Down: navigate or change a selected digit.\n"
                    "D-pad Left/Right: adjust values or select columns/digits in submenus.\n"
                    "R: tap and release to enter or confirm.\n"
                    "L: go back; closes from the top-level menu.\n\n"
                    "Leave this off to keep the D-pad free for normal play.";
            row.enabled = !w.options->disabled && w.activeDisables.empty()
                && !(w.raceDisable && CVarGetInteger(CVAR_SETTING("DisableChanges"), 0));
            if (!row.enabled) {
                for (const auto reason : w.activeDisables)
                    row.detail += "\n" + std::string(disabledMap.at(reason).reason);
                if (w.options->disabled && w.options->disabledTooltip)
                    row.detail += "\n" + std::string(w.options->disabledTooltip);
            }
            const std::string id = row.id;
            row.change = [this, id](double value) { ApplyNativeWidget(id, value); };
            row.activate = [this, id] { ApplyNativeWidget(id, 0); };
            switch (w.type) {
                case WIDGET_TEXT:
                    row.kind = N::Kind::Info;
                    break;
                case WIDGET_CHECKBOX:
                case WIDGET_CVAR_CHECKBOX: {
                    row.kind = N::Kind::Toggle;
                    row.value = w.type == WIDGET_CVAR_CHECKBOX
                        ? CVarGetInteger(
                              w.cVar, std::static_pointer_cast<UIWidgets::CheckboxOptions>(w.options)->defaultValue
                          )
                        : *std::get<bool*>(w.valuePointer);
                    break;
                }
                case WIDGET_COMBOBOX:
                case WIDGET_CVAR_COMBOBOX: {
                    const auto o = std::static_pointer_cast<UIWidgets::ComboboxOptions>(w.options);
                    row.kind = N::Kind::Choice;
                    row.value = w.type == WIDGET_CVAR_COMBOBOX ? CVarGetInteger(w.cVar, o->defaultIndex)
                                                               : *std::get<int32_t*>(w.valuePointer);
                    for (auto& [value, title] : o->comboMap)
                        row.choices.push_back({ value, title });
                    std::sort(row.choices.begin(), row.choices.end(), [](const auto& a, const auto& b) {
                        return a.value < b.value;
                    });
                    break;
                }
                case WIDGET_SLIDER_INT:
                case WIDGET_CVAR_SLIDER_INT: {
                    const auto o = std::static_pointer_cast<UIWidgets::IntSliderOptions>(w.options);
                    row.kind = N::Kind::Integer;
                    row.minimum = o->min;
                    row.maximum = o->max;
                    row.step = o->step;
                    row.value = w.type == WIDGET_CVAR_SLIDER_INT ? CVarGetInteger(w.cVar, o->defaultValue)
                                                                 : *std::get<int32_t*>(w.valuePointer);
                    break;
                }
                case WIDGET_SLIDER_FLOAT:
                case WIDGET_CVAR_SLIDER_FLOAT: {
                    const auto o = std::static_pointer_cast<UIWidgets::FloatSliderOptions>(w.options);
                    row.kind = N::Kind::Decimal;
                    row.minimum = o->min;
                    row.maximum = o->max;
                    row.step = o->step;
                    row.percentage = o->isPercentage;
                    row.value = w.type == WIDGET_CVAR_SLIDER_FLOAT ? CVarGetFloat(w.cVar, o->defaultValue)
                                                                   : *std::get<float*>(w.valuePointer);
                    break;
                }
                case WIDGET_BUTTON:
                    row.kind = N::Kind::Action;
                    break;
                case WIDGET_WINDOW_BUTTON: {
                    const auto o = std::static_pointer_cast<UIWidgets::WindowButtonOptions>(w.options);
                    if (!o->embedWindow) {
                        row.kind = N::Kind::Toggle;
                        row.title = "Show Input Viewer";
                        row.value = CVarGetInteger(w.cVar, 0);
                    } else {
                        row.kind = N::Kind::Link;
                        row.title = "Controller Bindings";
                        row.symbol = "gamecontroller";
                        row.detail = "Edit buttons, sticks and connected controllers.";
                        row.target = "bindings/root";
                        row.activate = { };
                        row.change = { };
                    }
                    break;
                }
                case WIDGET_AUDIO_BACKEND: {
                    auto audio = Ship::Context::GetRawInstance()->GetAudio();
                    if (!audio)
                        continue;
                    row.kind = N::Kind::Choice;
                    row.title = "Audio API";
                    row.value = static_cast<int>(audio->GetCurrentAudioBackend());
                    for (auto value : *audio->GetAvailableAudioBackends())
                        row.choices.push_back({ static_cast<int>(value), audioBackendsMap.at(value) });
                    row.enabled = row.choices.size() > 1;
                    row.detail = "Changing the audio API requires a relaunch.";
                    row.change = [audio](double value) {
                        const auto available = audio->GetAvailableAudioBackends();
                        const auto selected = static_cast<Ship::AudioBackend>(int(value));
                        if (std::find(available->begin(), available->end(), selected) != available->end())
                            audio->SetCurrentAudioBackend(selected);
                    };
                    break;
                }
                case WIDGET_CVAR_COLOR_PICKER: {
                    const auto o = std::static_pointer_cast<UIWidgets::ColorPickerOptions>(w.options);
                    row.kind = N::Kind::Color;
                    row.alpha = o->useAlpha;
                    auto color = CVarGetColor(w.cVar, o->defaultValue);
                    row.value =
                        (uint32_t(color.r) << 24) | (uint32_t(color.g) << 16) | (uint32_t(color.b) << 8) | color.a;
                    break;
                }
                default:
                    continue;
            }
            section.rows.push_back(std::move(row));
        }
    finish();
    if (group == "Settings" && page == "General") {
        N::Row about;
        about.title = "PaperReality";
        about.detail = "Experimental UIKit interface";
        about.symbol = "paperplane.fill";
        N::Row restart;
        restart.title = "Restart Game";
        restart.kind = N::Kind::Action;
        restart.destructive = true;
        restart.activate = [] {
            RegisterPopup("Restart Game", "Restart the game? Unsaved progress will be lost.", "Restart", "Cancel", [] {
                auto console = std::dynamic_pointer_cast<Ship::ConsoleWindow>(
                    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetGuiWindow("Console")
                );
                if (console)
                    console->Dispatch("reset");
            });
        };
        result.sections.push_back({ "About", "", { about, restart } });
    }
    return result;
}
} // namespace PaperboatGui
