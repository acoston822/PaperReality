#include "PaperboatModals.h"
#include "PaperboatGui.hpp"
#include "UIWidgets.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

struct PaperboatModal {
    std::string title_;
    std::string message_;
    std::string button1_;
    std::string button2_;
    std::function<void()> button1callback_;
    std::function<void()> button2callback_;
    // Text, button and padding size multiplier for this popup (1 = normal).
    float scale_ = 1.0f;
};
std::vector<PaperboatModal> modals;

bool closePopup = false;

void PaperboatModalWindow::Draw() {
    if (!IsVisible()) {
        return;
    }
    DrawElement();
    // Sync up the IsVisible flag if it was changed by ImGui
    SyncVisibilityConsoleVariable();
}

void PaperboatModalWindow::DrawElement() {
    if (modals.size() > 0) {
        PaperboatModal curModal = modals.at(0);
        if (!ImGui::IsPopupOpen(curModal.title_.c_str())) {
            ImGui::OpenPopup(curModal.title_.c_str());
        }
        if (closePopup) {
            ImGui::CloseCurrentPopup();
            modals.erase(modals.begin());
            closePopup = false;
        }
        const float scale = curModal.scale_;
        if (scale != 1.0f) {
            const ImGuiStyle& style = ImGui::GetStyle();
            ImGui::PushStyleVar(
                ImGuiStyleVar_WindowPadding, ImVec2(style.WindowPadding.x * scale, style.WindowPadding.y * scale)
            );
            ImGui::PushStyleVar(
                ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x * scale, style.FramePadding.y * scale)
            );
            ImGui::PushStyleVar(
                ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x * scale, style.ItemSpacing.y * scale)
            );
        }
        if (ImGui::BeginPopupModal(
                curModal.title_.c_str(), NULL,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                    | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings
            ))
        {
            if (scale != 1.0f) {
                ImGui::SetWindowFontScale(scale);
            }
            ImGui::Text("%s", curModal.message_.c_str());
            UIWidgets::PushStyleButton(THEME_COLOR);
            if (ImGui::Button(curModal.button1_.c_str())) {
                SPDLOG_INFO("[popup] \"{}\": {}", curModal.title_, curModal.button1_);
                if (curModal.button1callback_ != nullptr) {
                    curModal.button1callback_();
                }
                ImGui::CloseCurrentPopup();
                modals.erase(modals.begin());
            }
            UIWidgets::PopStyleButton();
            if (curModal.button2_ != "") {
                ImGui::SameLine();
                UIWidgets::PushStyleButton(THEME_COLOR);
                if (ImGui::Button(curModal.button2_.c_str())) {
                    SPDLOG_INFO("[popup] \"{}\": {}", curModal.title_, curModal.button2_);
                    if (curModal.button2callback_ != nullptr) {
                        curModal.button2callback_();
                    }
                    ImGui::CloseCurrentPopup();
                    modals.erase(modals.begin());
                }
                UIWidgets::PopStyleButton();
            }
        }
        ImGui::EndPopup();
        if (scale != 1.0f) {
            ImGui::PopStyleVar(3);
        }
    }
}

void PaperboatModalWindow::RegisterPopup(
    std::string title,
    std::string message,
    std::string button1,
    std::string button2,
    std::function<void()> button1callback,
    std::function<void()> button2callback,
    float scale
) {
    modals.push_back({ title, message, button1, button2, button1callback, button2callback, scale });
}

size_t PaperboatModalWindow::PopupsQueued() {
    return modals.size();
}

bool PaperboatModalWindow::IsPopupOpen(std::string title) {
    return !modals.empty() && modals.at(0).title_ == title;
}

void PaperboatModalWindow::DismissPopup() {
    closePopup = true;
}
