#include "SettingsUI.h"
#include "UIWidgets.hpp"
#include "TouchDragScroll.h"
#include <algorithm>
#include <cmath>
#include <imgui_internal.h>
#include <fast/Fast3dGui.h>
#include <fast/resource/type/Texture.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

namespace PaperboatGui::SettingsUI {
namespace {
    int depth = 0;
    float unit = 1.0f;
    bool preview = false;
    const ImVec4 blue(0.02f, 0.42f, 1.0f, 1.0f);
    const ImVec4 card(0.095f, 0.115f, 0.145f, 0.96f);
}

bool Active() {
    return depth > 0;
}
bool Enabled() {
#if defined(__IOS__)
    return CVarGetInteger("gSettings.Menu.ExperimentalIOS", 1) != 0;
#else
    // Explicit developer preview only; desktop behavior is unchanged by default.
    return preview;
#endif
}
void SetPreviewEnabled(bool enabled) {
    preview = enabled;
}
float Unit() {
    return unit;
}

ScopedStyle::ScopedStyle(float scale)
    : previous(ImGui::GetStyle())
    , previousUnit(unit)
    , previousFontScale(ImGui::GetIO().FontGlobalScale) {
    ++depth;
    unit = scale;
    ImGui::GetIO().FontGlobalScale = unit;
    auto& s = ImGui::GetStyle();
    s.WindowPadding = ImVec2(14, 12) * unit;
    s.FramePadding = ImVec2(12, 9) * unit;
    s.ItemSpacing = ImVec2(10, 9) * unit;
    s.ItemInnerSpacing = ImVec2(8, 6) * unit;
    s.CellPadding = ImVec2(8, 8) * unit;
    s.WindowRounding = s.ChildRounding = s.PopupRounding = 14 * unit;
    s.FrameRounding = s.GrabRounding = 10 * unit;
    s.TabRounding = 10 * unit;
    s.WindowBorderSize = s.ChildBorderSize = 1;
    s.FrameBorderSize = 0;
    s.ScrollbarSize = 8 * unit;
    s.GrabMinSize = 22 * unit;
    s.Colors[ImGuiCol_Text] = ImVec4(0.96f, 0.97f, 0.99f, 1);
    s.Colors[ImGuiCol_TextDisabled] = ImVec4(0.6f, 0.65f, 0.73f, 1);
    s.Colors[ImGuiCol_WindowBg] = ImVec4(0.025f, 0.035f, 0.06f, 0.97f);
    s.Colors[ImGuiCol_ChildBg] = card;
    s.Colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.12f, 0.16f, 1);
    s.Colors[ImGuiCol_Border] = ImVec4(0.28f, 0.32f, 0.39f, 0.5f);
    s.Colors[ImGuiCol_Separator] = s.Colors[ImGuiCol_Border];
    s.Colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.19f, 0.24f, 1);
    s.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.26f, 0.33f, 1);
    s.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.28f, 0.38f, 1);
    s.Colors[ImGuiCol_Button] = ImVec4(0.15f, 0.18f, 0.23f, 1);
    s.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.21f, 0.27f, 0.36f, 1);
    s.Colors[ImGuiCol_ButtonActive] = blue;
    s.Colors[ImGuiCol_Header] = blue;
    s.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.12f, 0.32f, 0.58f, 1);
    s.Colors[ImGuiCol_HeaderActive] = blue;
    s.Colors[ImGuiCol_SliderGrab] = ImVec4(0.96f, 0.97f, 1, 1);
    s.Colors[ImGuiCol_SliderGrabActive] = blue;
    s.Colors[ImGuiCol_CheckMark] = blue;
    s.Colors[ImGuiCol_Tab] = s.Colors[ImGuiCol_Button];
    s.Colors[ImGuiCol_TabActive] = blue;
    s.Colors[ImGuiCol_TabHovered] = s.Colors[ImGuiCol_ButtonHovered];
}

ScopedStyle::~ScopedStyle() {
    ImGui::GetStyle() = previous;
    unit = previousUnit;
    ImGui::GetIO().FontGlobalScale = previousFontScale;
    // Refresh ImGui's cached size before the other game overlays draw.
    ImGui::PushFont(ImGui::GetFont());
    ImGui::PopFont();
    --depth;
}

void Help(const char* text, bool disabled) {
    if (text != nullptr && *text != '\0' && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::BeginTooltip();
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PushTextWrapPos(std::min(420 * Unit(), ImGui::GetMainViewport()->WorkSize.x * 0.8f));
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

bool Toggle(const char* label, bool* value, const UIWidgets::CheckboxOptions& options) {
    ImGui::BeginDisabled(options.disabled);
    auto* window = ImGui::GetCurrentWindow();
    const ImGuiID id = window->GetID(label);
    const bool hidden = options.labelPosition == UIWidgets::LabelPositions::None || label[0] == '#';
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float switchW = 48 * unit, switchH = 28 * unit;
    const float textWidth = std::max(1.0f, width - switchW - 12 * unit);
    const char* end = ImGui::FindRenderedTextEnd(label);
    ImVec2 text = hidden ? ImVec2(0, 0) : ImGui::CalcTextSize(label, end, false, textWidth);
    const float height = std::max(44 * unit, text.y + 12 * unit);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImRect bb(p, p + ImVec2(width, height));
    ImGui::ItemSize(bb);
    bool hovered = false, held = false, pressed = false;
    if (ImGui::ItemAdd(bb, id)) {
        pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
        if (pressed) {
            *value = !*value;
            ImGui::MarkItemEdited(id);
        }
        ImGui::RenderNavHighlight(bb, id);
        auto* dl = window->DrawList;
        if (!hidden) {
            dl->AddText(
                nullptr, 0, p + ImVec2(0, (height - text.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), label, end,
                textWidth
            );
        }
        const ImVec2 s = p + ImVec2(width - switchW, (height - switchH) * 0.5f);
        dl->AddRectFilled(
            s, s + ImVec2(switchW, switchH),
            ImGui::GetColorU32(*value ? ImVec4(0.18f, 0.78f, 0.36f, 1) : ImVec4(0.24f, 0.27f, 0.32f, 1)), switchH / 2
        );
        dl->AddCircleFilled(
            s + ImVec2(*value ? switchW - switchH / 2 : switchH / 2, switchH / 2), switchH / 2 - 3 * unit,
            ImGui::GetColorU32(ImVec4(1, 1, 1, 1))
        );
    }
    ImGui::EndDisabled();
    Help(options.disabled ? options.disabledTooltip : options.tooltip, options.disabled);
    return pressed;
}

bool BeginChoice(const char* label, const char* preview, bool disabled) {
    ImGui::PushID(label);
    ImGui::BeginGroup();
    ImGui::BeginDisabled(disabled);
    if (label[0] != '#')
        ImGui::TextWrapped("%s", label);
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    if (ImGui::Button((std::string(preview) + "  " ICON_FA_CHEVRON_RIGHT).c_str(), ImVec2(width, 42 * unit))) {
        ImGui::OpenPopup("##choices");
    }
    ImGui::EndDisabled();
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(std::min(width, 260 * unit), 0), ImVec2(std::max(width, 260 * unit), 300 * unit)
    );
    return ImGui::BeginPopup("##choices");
}

void EndChoice() {
    ImGui::EndGroup();
    ImGui::PopID();
}

bool SliderInt(const char* label, int* value, const UIWidgets::IntSliderOptions& options) {
    ImGui::PushID(label);
    ImGui::BeginGroup();
    ImGui::BeginDisabled(options.disabled);
    if (label[0] != '#' && options.labelPosition != UIWidgets::LabelPositions::None)
        ImGui::TextWrapped(label, *value);
    const float button = 40 * unit;
    float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    bool changed = false;
    if (options.showButtons) {
        if (ImGui::Button("-", ImVec2(button, 40 * unit)) && *value > options.min) {
            *value = std::max(options.min, *value - options.step);
            changed = true;
        }
        ImGui::SameLine();
        width -= 2 * (button + ImGui::GetStyle().ItemSpacing.x);
    }
    ImGui::SetNextItemWidth(std::max(1.0f, width));
    changed |= ImGui::SliderInt(
        "##value", value, options.min, options.max, options.format, options.flags | ImGuiSliderFlags_AlwaysClamp
    );
    if (ImGui::IsItemActive())
        TouchDragScrollBlock();
    if (options.showButtons) {
        ImGui::SameLine();
        if (ImGui::Button("+", ImVec2(button, 40 * unit)) && *value < options.max) {
            *value = std::min(options.max, *value + options.step);
            changed = true;
        }
    }
    ImGui::EndDisabled();
    ImGui::EndGroup();
    Help(options.disabled ? options.disabledTooltip : options.tooltip);
    ImGui::PopID();
    return changed;
}

bool SliderFloat(const char* label, float* value, const UIWidgets::FloatSliderOptions& options) {
    ImGui::PushID(label);
    ImGui::BeginGroup();
    ImGui::BeginDisabled(options.disabled);
    if (label[0] != '#' && options.labelPosition != UIWidgets::LabelPositions::None)
        ImGui::TextWrapped(label, *value);
    float displayed = options.isPercentage ? *value * 100 : *value;
    const float factor = options.isPercentage ? 100 : 1;
    const float button = 40 * unit;
    float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    bool changed = false;
    if (options.showButtons) {
        if (ImGui::Button("-", ImVec2(button, 40 * unit)) && *value > options.min) {
            *value = std::max(options.min, *value - options.step);
            changed = true;
            displayed = *value * factor;
        }
        ImGui::SameLine();
        width -= 2 * (button + ImGui::GetStyle().ItemSpacing.x);
    }
    ImGui::SetNextItemWidth(std::max(1.0f, width));
    if (ImGui::SliderFloat(
            "##value", &displayed, options.min * factor, options.max * factor, options.format,
            options.flags | ImGuiSliderFlags_AlwaysClamp
        ))
    {
        *value = displayed / factor;
        changed = true;
    }
    if (ImGui::IsItemActive())
        TouchDragScrollBlock();
    if (options.showButtons) {
        ImGui::SameLine();
        if (ImGui::Button("+", ImVec2(button, 40 * unit)) && *value < options.max) {
            *value = std::min(options.max, *value + options.step);
            changed = true;
        }
    }
    ImGui::EndDisabled();
    ImGui::EndGroup();
    Help(options.disabled ? options.disabledTooltip : options.tooltip);
    ImGui::PopID();
    return changed;
}

void BeginCard(const char* id) {
    ImGui::PushID(id);
    ImGui::BeginChild(
        "##card", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar
    );
}
void EndCard() {
    ImGui::EndChild();
    ImGui::PopID();
}

bool NavigationRow(const char* id, const char* icon, const char* label, bool selected, bool group, bool expanded) {
    ImGui::PushID(id);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float h = 42 * unit;
    const float w = ImGui::GetContentRegionAvail().x;
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
    const bool pressed = ImGui::Selectable("##nav", selected, ImGuiSelectableFlags_None, ImVec2(w, h));
    ImGui::PopStyleColor(3);
    auto* dl = ImGui::GetWindowDrawList();
    const float inset = (group ? 8 : 18) * unit;
    const float y = p.y + (h - ImGui::GetFontSize()) / 2;
    const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
    if (selected)
        dl->AddRectFilled(p, p + ImVec2(w, h), ImGui::GetColorU32(blue), 9 * unit);
    else if (ImGui::IsItemHovered())
        dl->AddRectFilled(p, p + ImVec2(w, h), ImGui::GetColorU32(ImGuiCol_FrameBgHovered), 9 * unit);
    if (ImGui::IsItemFocused())
        dl->AddRect(p, p + ImVec2(w, h), ImGui::GetColorU32(ImGuiCol_Text), 9 * unit);
    dl->AddText(ImVec2(p.x + inset, y), color, icon);
    const float rightPadding = (group ? 28 : 6) * unit;
    dl->PushClipRect(p, p + ImVec2(std::max(1.0f, w - rightPadding), h), true);
    dl->AddText(ImVec2(p.x + inset + 30 * unit, y), color, label);
    dl->PopClipRect();
    if (group)
        dl->AddText(ImVec2(p.x + w - 20 * unit, y), color, expanded ? ICON_FA_CHEVRON_DOWN : ICON_FA_CHEVRON_RIGHT);
    ImGui::PopID();
    return pressed;
}

void Placeholder(ImVec2 size) {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::Dummy(size);
    ImGui::GetWindowDrawList()->AddRect(p, p + size, ImGui::GetColorU32(ImGuiCol_Border), 8 * Unit());
}

ImTextureID GameTexture(const char* raster, const char* palette, ImVec2* size) {
    if (raster == nullptr || *raster == '\0')
        return nullptr;
    auto manager = Ship::Context::GetRawInstance()->GetResourceManager();
    auto gui = std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
    if (!manager || !gui)
        return nullptr;
    std::string path(raster);
    if (path.starts_with("__OTR__"))
        path.erase(0, 7);
    const bool alternate = manager->IsAltAssetsEnabled();
    auto archive = manager->GetArchiveManager();
    if (!archive || (!archive->HasFile(path) && !(alternate && archive->HasFile("alt/" + path))))
        return nullptr;
    const std::string key = "SettingsPortrait/" + std::string(alternate ? "alt/" : "original/") + path;
    if (!gui->HasTextureByName(key)) {
        auto texture = std::dynamic_pointer_cast<Fast::Texture>(manager->LoadResource(raster));
        if (!texture || !texture->ImageData)
            texture = std::dynamic_pointer_cast<Fast::Texture>(manager->LoadResource(raster, true));
        if (!texture || !texture->ImageData)
            return nullptr;
        gui->LoadGuiTexture(key, *texture, palette == nullptr ? "" : palette);
    }
    if (size)
        *size = gui->GetTextureSize(key);
    return gui->GetTextureByName(key);
}

void GameImage(const char* raster, const char* palette, ImVec2 box) {
    ImVec2 size;
    ImTextureID texture = GameTexture(raster, palette, &size);
    if (!texture || size.x <= 0 || size.y <= 0) {
        Placeholder(box);
        return;
    }
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float scale = std::min(box.x / size.x, box.y / size.y);
    const ImVec2 drawSize = size * scale;
    const ImVec2 min = p + (box - drawSize) * 0.5f;
    ImGui::Dummy(box);
    ImGui::GetWindowDrawList()->AddImage(texture, min, min + drawSize);
}
} // namespace PaperboatGui::SettingsUI
