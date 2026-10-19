#pragma once

#include <imgui.h>

namespace UIWidgets {
struct CheckboxOptions;
struct IntSliderOptions;
struct FloatSliderOptions;
}

namespace PaperboatGui::SettingsUI {
// Scoped to the experimental settings window; game overlays keep their own style.
bool Active();
bool Enabled();
void SetPreviewEnabled(bool enabled);
float Unit();
class ScopedStyle {
public:
    explicit ScopedStyle(float unit);
    ~ScopedStyle();
    ScopedStyle(const ScopedStyle&) = delete;
    ScopedStyle& operator=(const ScopedStyle&) = delete;

private:
    ImGuiStyle previous;
    float previousUnit;
    float previousFontScale;
};

bool Toggle(const char* label, bool* value, const UIWidgets::CheckboxOptions& options);
bool SliderInt(const char* label, int* value, const UIWidgets::IntSliderOptions& options);
bool SliderFloat(const char* label, float* value, const UIWidgets::FloatSliderOptions& options);
// BeginChoice always pairs with EndChoice; EndPopup is only needed when it returns true.
bool BeginChoice(const char* label, const char* preview, bool disabled);
void EndChoice();
void Help(const char* text, bool disabled = false);
void BeginCard(const char* id);
void EndCard();
bool NavigationRow(
    const char* id, const char* icon, const char* label, bool selected, bool group = false, bool expanded = false
);
void Placeholder(ImVec2 size);
ImTextureID GameTexture(const char* raster, const char* palette = "", ImVec2* size = nullptr);
void GameImage(const char* raster, const char* palette, ImVec2 box);
} // namespace PaperboatGui::SettingsUI
