#include "NativeSettings.h"
#include "InputViewer.h"
#include <fast/Fast3dWindow.h>
#include <fast/interpreter.h>
#include <algorithm>
#include <memory>

namespace Paperboat::NativeSettings {
namespace {
    Row Color(std::string title, std::string cvar, Color_RGBA8 fallback) {
        Row row;
        row.title = std::move(title);
        row.id = cvar;
        row.kind = Kind::Color;
        const auto c = CVarGetColor(cvar.c_str(), fallback);
        row.value = (uint32_t(c.r) << 24) | (uint32_t(c.g) << 16) | (uint32_t(c.b) << 8) | c.a;
        row.change = [cvar](double value) {
            const uint32_t c = uint32_t(value);
            CVarSetColor(cvar.c_str(), { uint8_t(c >> 24), uint8_t(c >> 16), uint8_t(c >> 8), uint8_t(c) });
            SaveSettings(cvar);
        };
        return row;
    }
}
Page InputViewerPage(const std::string& key) {
    Page page { key, "Input Viewer", "", { } };
    if (key == "viewer/root") {
        page.sections.push_back(
            { "",
              "The input viewer is an overlay. Its size is separate from the settings interface.",
              { Toggle("Show Input Viewer", "gOpenWindows.InputViewer", false),
                Link("Appearance", "viewer/appearance", "paintpalette"),
                Link("Button Layers", "viewer/buttons", "gamecontroller"),
                Link("Analog Stick", "viewer/analog", "circle"), Link("Right Stick", "viewer/right", "circle"),
                Link("Analog Angles", "viewer/angles", "angle") } }
        );
    } else if (key == "viewer/appearance") {
        page.title = "Appearance";
        page.sections.push_back(
            { "Input Viewer",
              "",
              { Number("Overlay Size", "gInputViewer.Scale", false, .1, 5, .1, 1),
                Toggle("Allow Dragging", "gInputViewer.EnableDragging", true),
                Toggle("Show Background", "gInputViewer.ShowBackground", true) } }
        );
    } else if (key == "viewer/buttons") {
        page.title = "Button Layers";
        page.sections.push_back(
            { "Outlines",
              "",
              { Select(
                    "Visibility", "gInputViewer.ButtonOutlineMode", BUTTON_OUTLINE_NOT_PRESSED,
                    { { 0, "Always" }, { 1, "When Not Pressed" }, { 2, "When Pressed" }, { 3, "Never" } }
                ),
                Toggle("Use for All Buttons", "gInputViewer.UseGlobalButtonOutlineMode", true) } }
        );
        Section buttons { "Visible Buttons", "", { } };
        for (const auto& [name, id] : std::vector<std::pair<std::string, std::string>> { { "A", "ABtn" },
                                                                                         { "B", "BBtn" },
                                                                                         { "C Up", "CUp" },
                                                                                         { "C Right", "CRight" },
                                                                                         { "C Down", "CDown" },
                                                                                         { "C Left", "CLeft" },
                                                                                         { "L", "LBtn" },
                                                                                         { "R", "RBtn" },
                                                                                         { "Z", "ZBtn" },
                                                                                         { "Start", "StartBtn" },
                                                                                         { "D-Pad", "Dpad" },
                                                                                         { "Modifier 1", "Mod1" },
                                                                                         { "Modifier 2", "Mod2" } })
        {
            buttons.rows.push_back(Toggle(name, "gInputViewer." + id, id != "Dpad" && id != "Mod1" && id != "Mod2"));
            if (!CVarGetInteger("gInputViewer.UseGlobalButtonOutlineMode", 1))
                buttons.rows.push_back(Select(
                    name + " Outline", "gInputViewer." + id + "OutlineMode", 1,
                    { { 0, "Always" }, { 1, "When Not Pressed" }, { 2, "When Pressed" }, { 3, "Never" } }
                ));
        }
        page.sections.push_back(std::move(buttons));
    } else if (key == "viewer/analog" || key == "viewer/right") {
        const bool right = key == "viewer/right";
        page.title = right ? "Right Stick" : "Analog Stick";
        const std::string prefix = right ? "gInputViewer.RightStick." : "gInputViewer.AnalogStick.";
        page.sections.push_back(
            { "Visibility",
              "",
              { Select(
                    "Stick", prefix + "VisibilityMode", right ? 2 : 0,
                    { { 0, "Always" }, { 1, "While in Use" }, { 2, "Never" } }
                ),
                Select(
                    "Outline", prefix + "OutlineMode", right ? 2 : 0,
                    { { 0, "Always" }, { 1, "While in Use" }, { 2, "Never" } }
                ),
                Number("Movement (pixels)", prefix + "Movement", true, 0, 200, 1, right ? 7 : 12) } }
        );
    } else if (key == "viewer/angles") {
        page.title = "Analog Angles";
        page.sections.push_back(
            { "Text",
              "",
              { Toggle("Show Angle Values", "gInputViewer.AnalogAngles.Enabled", false),
                Color("Text Color", "gInputViewer.AnalogAngles.TextColor.Value", { 255, 255, 255, 255 }),
                Number("Text Size", "gInputViewer.AnalogAngles.Scale", false, .1, 5, .1, 1),
                Number("Text Offset (pixels)", "gInputViewer.AnalogAngles.Offset", true, 0, 400, 1, 0) } }
        );
        page.sections.push_back(
            { "Highlights",
              "",
              { Toggle("ESS Position", "gInputViewer.AnalogAngles.Range1.Enabled", false),
                Color("ESS Color", "gInputViewer.AnalogAngles.Range1.Color.Value", { 255, 178, 0, 255 }),
                Toggle("Walking Angles", "gInputViewer.AnalogAngles.Range2.Enabled", false),
                Color("Walking Color", "gInputViewer.AnalogAngles.Range2.Color.Value", { 0, 255, 0, 255 }) } }
        );
    }
    return page;
}

Page ShaderPage() {
    Page page { "shaders", "Shader Packs", "", { } };
    auto window = std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetRawInstance()->GetWindow());
    auto interpreter = window ? window->GetInterpreterWeak().lock() : nullptr;
    if (!interpreter)
        return page;
    Section passes { "Post-Processing", "", { } };
    for (const auto& pass : interpreter->GetPostPasses()) {
        Row row;
        row.title = pass.pack.empty() ? pass.path : pass.pack;
        row.detail = pass.path;
        row.kind = Kind::Toggle;
        row.value = pass.enabled;
        row.change = [interpreter, id = pass.id](double value) { interpreter->SetPostPassEnabled(id, value != 0); };
        passes.rows.push_back(std::move(row));
    }
    if (!passes.rows.empty())
        page.sections.push_back(std::move(passes));
    for (const auto& [shader, entry] : interpreter->GetShaderSettingsRegistry()) {
        Section section { entry.pack.empty() ? entry.path : entry.pack, "", { } };
        for (const auto& decl : entry.decls) {
            const auto current = entry.values.at(decl.var);
            Row row;
            row.id = std::to_string(shader) + "/" + decl.var;
            row.title = decl.name.empty() ? decl.var : decl.name;
            row.value = current[0];
            row.minimum = decl.min;
            row.maximum = decl.max;
            if (decl.type == "toggle")
                row.kind = Kind::Toggle;
            else if (decl.type == "enum") {
                row.kind = Kind::Choice;
                for (size_t i = 0; i < decl.optionLabels.size() && i < decl.optionValues.size(); ++i) {
                    row.choices.push_back({ int(i), decl.optionLabels[i] });
                    if (decl.optionValues[i] == current[0])
                        row.value = i;
                }
            } else if (decl.type == "color") {
                row.kind = Kind::Color;
                row.alpha = false;
                row.value = (uint32_t(std::clamp(current[0], 0.f, 1.f) * 255) << 24)
                    | (uint32_t(std::clamp(current[1], 0.f, 1.f) * 255) << 16)
                    | (uint32_t(std::clamp(current[2], 0.f, 1.f) * 255) << 8) | 255;
            } else {
                row.kind = decl.type == "int" ? Kind::Integer : Kind::Decimal;
                row.step = decl.type == "int" ? 1 : .01;
            }
            row.change = [interpreter, shader, decl, current](double value) {
                float values[4];
                std::copy_n(current.data(), 4, values);
                if (decl.type == "enum") {
                    const int index = int(value);
                    if (index < 0 || size_t(index) >= decl.optionValues.size())
                        return;
                    values[0] = decl.optionValues[index];
                } else if (decl.type == "color") {
                    const uint32_t c = uint32_t(value);
                    values[0] = ((c >> 24) & 255) / 255.f;
                    values[1] = ((c >> 16) & 255) / 255.f;
                    values[2] = ((c >> 8) & 255) / 255.f;
                } else
                    values[0] = decl.type == "toggle" ? value != 0 : std::clamp(float(value), decl.min, decl.max);
                interpreter->SetShaderSettingValue(shader, decl.var, values);
            };
            section.rows.push_back(std::move(row));
        }
        if (!section.rows.empty())
            page.sections.push_back(std::move(section));
    }
    if (page.sections.empty()) {
        Row info;
        info.title = "No Shader Controls Available";
        info.symbol = "slider.horizontal.3";
        info.detail = "Controls appear when a loaded shader pack supplies configurable settings.";
        page.sections.push_back({ "", "", { info } });
    }
    return page;
}
} // namespace Paperboat::NativeSettings
