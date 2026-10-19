#include "PaperboatMenu.h"
#include "SettingsUI.h"
#include "TouchDragScroll.h"
#include "port/Engine.h"
#include "port/ios/IosSettingsInsets.h"
#include <ship/window/gui/ConsoleWindow.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <imgui_internal.h>

#undef End

namespace PaperboatGui {
namespace {
    const char* GroupIcon(const std::string& group) {
        if (group == "Settings")
            return ICON_FA_COG;
        if (group == "Enhancements")
            return ICON_FA_MAGIC;
        if (group == "Shaders")
            return ICON_FA_SLIDERS;
        return ICON_FA_WRENCH;
    }

    const char* PageIcon(const std::string& page) {
        if (page == "General")
            return ICON_FA_FILE_TEXT_O;
        if (page == "Audio")
            return ICON_FA_VOLUME_UP;
        if (page == "Controls")
            return ICON_FA_GAMEPAD;
        if (page == "Graphics")
            return ICON_FA_DESKTOP;
        if (page == "Input Viewer")
            return ICON_FA_HEARTBEAT;
        if (page == "Notifications")
            return ICON_FA_BELL_O;
        if (page == "Mod Menu")
            return ICON_FA_PUZZLE_PIECE;
        if (page == "Cheats")
            return ICON_FA_STAR_O;
        if (page == "Gameplay")
            return ICON_FA_GAMEPAD;
        if (page == "Status Bar" || page == "Stats")
            return ICON_FA_BAR_CHART;
        if (page == "Console")
            return ICON_FA_TERMINAL;
        if (page == "Event Debugger")
            return ICON_FA_BUG;
        if (page == "Value Viewer")
            return ICON_FA_EYE;
        if (page == "Save Editor")
            return ICON_FA_FLOPPY_O;
        if (page == "Blueprint Editor")
            return ICON_FA_TH_LARGE;
        if (page == "Search")
            return ICON_FA_SEARCH;
        return ICON_FA_SLIDERS;
    }

    const char* PageLabel(const std::string& group, const std::string& page) {
        return group == "Enhancements" && page == "Graphics" ? "Visuals" : page.c_str();
    }

    bool MobileWidget(const WidgetInfo& widget) {
        // Keep definitions intact for the classic desktop menu. These controls
        // manage desktop windows or the old menu layout and don't apply on iOS.
        static const std::array<const char*, 14> omitted = { "Cursor Always Visible",
                                                             "ImGui Menu Scaling",
                                                             "Toggle Fullscreen",
                                                             "Renderer API (Needs reload)",
                                                             "Windowed Fullscreen",
                                                             "Allow multi-windows",
                                                             "Popout Menu",
                                                             "Search In Sidebar",
                                                             "Menu Theme",
                                                             "EXPERIMENTAL",
                                                             "Graphics Options",
                                                             "Menu Settings",
                                                             "General Settings",
                                                             "Input Viewer Settings" };
        return std::none_of(omitted.begin(), omitted.end(), [&](const char* name) { return widget.name == name; });
    }

    void CloseMenu(PaperboatMenu& menu) {
        menu.Hide();
        auto gui = Ship::Context::GetRawInstance()->GetWindow()->GetGui();
        if (CVarGetInteger(CVAR_IMGUI_CONTROLLER_NAV, 0) && gui->GetMenuOrMenubarVisible())
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        else
            ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    }
} // namespace

void PaperboatMenu::DrawExperimentalSettings() {
    for (auto& [reason, info] : disabledMap)
        info.active = info.evaluation(info);
    const char* activeCvar = CVAR_SETTING("Menu.ActiveHeader");
    std::string active = CVarGetString(activeCvar, "Settings");
    if (!menuEntries.contains(active))
        active = "Settings";
    if (mExpandedSettingsGroup.empty())
        mExpandedSettingsGroup = active;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float u = std::clamp(std::min(viewport->WorkSize.y / 590.0f, viewport->WorkSize.x / 700.0f), 0.75f, 2.5f);
    SettingsUI::ScopedStyle scoped(u);
    const auto safe = Paperboat::IosSettingsSafeArea();
    const ImVec2 origin = viewport->WorkPos + ImVec2(safe.left, safe.top);
    const ImVec2 size(
        std::max(1.0f, viewport->WorkSize.x - safe.left - safe.right),
        std::max(1.0f, viewport->WorkSize.y - safe.top - safe.bottom)
    );
    ImGui::SetNextWindowPos(origin);
    ImGui::SetNextWindowSize(size);
    const float opacity = std::clamp(CVarGetFloat(CVAR_SETTING("Menu.BackgroundOpacity"), 0.85f), 0.35f, 1.0f);
    ImGui::SetNextWindowBgAlpha(opacity);
    const auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoDocking;
    if (!ImGui::Begin("Main Menu", nullptr, flags)) {
        ImGui::End();
        return;
    }
    ImGui::PushFont(GameEngine::Instance->fontStandardLarger);
    ImGui::SetWindowFontScale(1.0f);

    const bool narrow = size.x < 750 * u;
    if (narrow && ImGui::Button(ICON_FA_BARS, ImVec2(42 * u, 40 * u)))
        mMobileShowSidebar = !mMobileShowSidebar;
    if (narrow)
        ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::PushFont(GameEngine::Instance->fontStandardLargest);
    ImGui::TextUnformatted("PaperReality");
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextDisabled("Settings");
    ImGui::SameLine(std::max(ImGui::GetCursorPosX(), size.x - 190 * u));
    if (ImGui::Button(ICON_FA_SEARCH, ImVec2(42 * u, 40 * u))) {
        mMobileDetailWindow = "##search";
        mMobileShowSidebar = false;
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.02f, 0.42f, 1, 1));
    if (ImGui::Button("Done", ImVec2(100 * u, 40 * u)))
        CloseMenu(*this);
    ImGui::PopStyleColor();
    ImGui::Separator();

    const float bodyHeight = std::max(1.0f, ImGui::GetContentRegionAvail().y);
    const bool showSidebar = !narrow || mMobileShowSidebar;
    const float sidebarWidth =
        narrow ? ImGui::GetContentRegionAvail().x : std::clamp(size.x * 0.245f, 235 * u, 290 * u);
    if (showSidebar) {
        ImGui::BeginChild("##settings-navigation", ImVec2(sidebarWidth, bodyHeight), ImGuiChildFlags_Borders);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 3 * u));
        for (const auto& group : menuOrder) {
            ImGui::PushID(group.c_str());
            const bool open = mExpandedSettingsGroup == group;
            if (SettingsUI::NavigationRow("##group", GroupIcon(group), group.c_str(), false, true, open)) {
                mExpandedSettingsGroup = open ? "##collapsed" : group;
            }
            if (mExpandedSettingsGroup == group) {
                const auto& entry = menuEntries.at(group);
                for (const auto& page : entry.sidebarOrder) {
                    std::string saved = CVarGetString(entry.sidebarCvar, "");
                    if (!entry.sidebars.contains(saved))
                        saved = entry.sidebarOrder.front();
                    if (SettingsUI::NavigationRow(
                            page.c_str(), PageIcon(page), PageLabel(group, page),
                            active == group && saved == page && mMobileDetailWindow.empty()
                        ))
                    {
                        active = group;
                        CVarSetString(activeCvar, group.c_str());
                        CVarSetString(entry.sidebarCvar, page.c_str());
                        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
                        mMobileDetailWindow.clear();
                        if (narrow)
                            mMobileShowSidebar = false;
                    }
                }
            }
            ImGui::Separator();
            ImGui::PopID();
        }
        ImGui::PopStyleVar();
        TouchDragScroll();
        ImGui::EndChild();
        if (!narrow)
            ImGui::SameLine();
    }

    if (!narrow || !showSidebar) {
        auto& entry = menuEntries.at(active);
        std::string page = CVarGetString(entry.sidebarCvar, "");
        if (!entry.sidebars.contains(page))
            page = entry.sidebarOrder.front();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::BeginChild("##settings-content", ImVec2(0, bodyHeight));
        ImGui::PopStyleColor();
        if (!mMobileDetailWindow.empty()) {
            if (ImGui::Button(ICON_FA_CHEVRON_LEFT " Back"))
                mMobileDetailWindow.clear();
        }
        const bool search = mMobileDetailWindow == "##search" || page == "Search";
        const std::string title =
            search ? "Search" : (mMobileDetailWindow.empty() ? PageLabel(active, page) : mMobileDetailWindow);
        ImGui::PushFont(GameEngine::Instance->fontStandardLargest);
        ImGui::TextUnformatted(title.c_str());
        ImGui::PopFont();
        ImGui::TextDisabled("%s  >  %s", active.c_str(), PageLabel(active, page));
        ImGui::Spacing();
        if (search) {
            ImGui::SetNextItemWidth(-1);
            if (CVarGetInteger(CVAR_SETTING("Menu.SearchAutofocus"), 0)
                && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive()
                && !ImGui::IsMouseClicked(0))
                ImGui::SetKeyboardFocusHere();
            if (ImGui::InputTextWithHint(
                    "##search-all", "Search settings...", menuSearch.InputBuf, sizeof(menuSearch.InputBuf)
                ))
                menuSearch.Build();
            if (ImGui::IsItemActive())
                TouchDragScrollBlock();
            const std::string query(menuSearch.InputBuf);
            if (!query.empty()) {
                for (auto& group : menuOrder) {
                    auto& groupEntry = menuEntries.at(group);
                    for (auto& pageName : groupEntry.sidebarOrder) {
                        for (auto& column : groupEntry.sidebars.at(pageName).columnWidgets) {
                            for (auto& widget : column) {
                                if (!MobileWidget(widget) || widget.hideInSearch || widget.type == WIDGET_SEPARATOR_TEXT
                                    || widget.type == WIDGET_SEPARATOR || widget.type == WIDGET_SEARCH)
                                    continue;
                                const char* tooltip = widget.options->tooltip;
                                const std::string searchable = widget.name + " " + (tooltip == nullptr ? "" : tooltip);
                                if (!menuSearch.PassFilter(searchable.c_str()))
                                    continue;
                                ImGui::PushID(&widget);
                                const std::string label =
                                    group + " > " + PageLabel(group, pageName) + ": " + widget.name;
                                if (ImGui::Selectable(label.c_str(), false, 0, ImVec2(0, 42 * u))) {
                                    CVarSetString(activeCvar, group.c_str());
                                    CVarSetString(groupEntry.sidebarCvar, pageName.c_str());
                                    mExpandedSettingsGroup = group;
                                    mMobileDetailWindow.clear();
                                    Ship::Context::GetRawInstance()
                                        ->GetWindow()
                                        ->GetGui()
                                        ->SaveConsoleVariablesNextFrame();
                                }
                                ImGui::PopID();
                            }
                        }
                    }
                }
            }
        } else if (!mMobileDetailWindow.empty()) {
            auto window = Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetGuiWindow(mMobileDetailWindow);
            if (window != nullptr) {
                window->Hide();
                window->Init();
                window->DrawElement();
            }
        } else {
            auto& updates = MenuInit::GetUpdateFuncs();
            if (updates.contains(active) && updates.at(active).contains(page)) {
                for (auto& update : updates.at(active).at(page))
                    update();
            }
            auto& section = entry.sidebars.at(page);
            const int columns = ImGui::GetContentRegionAvail().x > 640 * u
                ? std::min<int>(section.columnCount, section.columnWidgets.size())
                : 1;
            if (ImGui::BeginTable("##settings-columns", std::max(1, columns), ImGuiTableFlags_SizingStretchSame)) {
                for (int column = 0; column < std::max(1, columns); ++column) {
                    ImGui::TableNextColumn();
                    for (size_t index = column; index < section.columnWidgets.size(); index += std::max(1, columns)) {
                        for (auto& widget : section.columnWidgets.at(index)) {
                            if (!MobileWidget(widget))
                                continue;
                            ImGui::PushID(&widget);
                            if (widget.type == WIDGET_WINDOW_BUTTON) {
                                auto options = std::static_pointer_cast<UIWidgets::WindowButtonOptions>(widget.options);
                                if (!options->embedWindow) {
                                    // Input viewer is an overlay, not a settings popout.
                                    MenuDrawItem(widget, 60, defaultThemeIndex);
                                } else if (std::strcmp(widget.windowName, "Configure Controller") == 0) {
                                    if (ImGui::Button(
                                            "Controller Bindings  " ICON_FA_CHEVRON_RIGHT, ImVec2(-1, 44 * u)
                                        ))
                                        mMobileDetailWindow = widget.windowName;
                                } else {
                                    auto window = Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetGuiWindow(
                                        widget.windowName
                                    );
                                    if (window != nullptr) {
                                        window->Hide();
                                        window->Init();
                                        ImGui::BeginChild(
                                            widget.windowName,
                                            ImVec2(0, std::max(260 * u, ImGui::GetContentRegionAvail().y)),
                                            ImGuiChildFlags_Borders
                                        );
                                        window->DrawElement();
                                        TouchDragScroll();
                                        ImGui::EndChild();
                                    }
                                }
                            } else {
                                MenuDrawItem(widget, 60, defaultThemeIndex);
                            }
                            ImGui::PopID();
                        }
                    }
                }
                ImGui::EndTable();
            }
            if (active == "Settings" && page == "General") {
                SettingsUI::BeginCard("##about");
                ImGui::TextUnformatted("PaperReality");
                ImGui::TextWrapped("Experimental iOS settings");
                ImGui::TextDisabled("%s", gBuildVersion);
                if (ImGui::Button("Restart Game")) {
                    auto console = std::dynamic_pointer_cast<Ship::ConsoleWindow>(
                        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetGuiWindow("Console")
                    );
                    if (console)
                        console->Dispatch("reset");
                }
                SettingsUI::EndCard();
            }
        }
        TouchDragScroll();
        ImGui::EndChild();
    }
    ImGui::PopFont();
    ImGui::End();
}
} // namespace PaperboatGui
