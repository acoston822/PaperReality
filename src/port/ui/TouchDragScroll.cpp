#include "TouchDragScroll.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace PaperboatGui {

namespace {
constexpr float kDragThreshold = 10.0f;  // pixels before a press turns into a scroll
constexpr float kMinFlickSpeed = 250.0f; // pixels per second needed at release to start gliding
constexpr float kGlideDecay = 3.2f;      // higher stops sooner
constexpr float kStopSpeed = 20.0f;

bool sBlocked = false;
ImGuiID sPressWindow = 0;
bool sScrolling = false;
ImGuiID sGlideWindow = 0;
float sVelocity = 0.0f; // pixels per second, in scroll direction
} // namespace

void TouchDragScrollBlock() {
    sBlocked = true;
}

static void Step(ImGuiWindow* win, bool hovered) {
    ImGuiIO& io = ImGui::GetIO();
    const float dt = std::max(io.DeltaTime, 1.0f / 240.0f);

    // A new press stops any glide in progress.
    if (ImGui::IsMouseClicked(0)) {
        sGlideWindow = 0;
    }

    if (!io.MouseDown[0]) {
        if (sPressWindow == win->ID) {
            if (sScrolling && std::fabs(sVelocity) >= kMinFlickSpeed) {
                sGlideWindow = win->ID;
            }
            sPressWindow = 0;
            sScrolling = false;
        }
        sBlocked = false;
        if (sGlideWindow == win->ID) {
            const float next = std::clamp(win->Scroll.y + sVelocity * dt, 0.0f, win->ScrollMax.y);
            ImGui::SetScrollY(win, next);
            sVelocity *= std::exp(-kGlideDecay * dt);
            if (std::fabs(sVelocity) < kStopSpeed || next <= 0.0f || next >= win->ScrollMax.y) {
                sGlideWindow = 0;
                sVelocity = 0.0f;
            }
        }
        return;
    }

    if (ImGui::IsMouseClicked(0) && hovered) {
        sPressWindow = win->ID;
        sScrolling = false;
        sVelocity = 0.0f;
    }
    if (sPressWindow != win->ID || sBlocked || win->ScrollMax.y <= 0.0f) {
        return;
    }
    if (!sScrolling && ImGui::IsMouseDragging(0, kDragThreshold)) {
        sScrolling = true;
        ImGui::ClearActiveID();
    }
    if (sScrolling) {
        const float delta = -io.MouseDelta.y;
        ImGui::SetScrollY(win, std::clamp(win->Scroll.y + delta, 0.0f, win->ScrollMax.y));
        // Smoothed so a pause before lifting the finger doesn't throw the list.
        sVelocity = sVelocity * 0.6f + (delta / dt) * 0.4f;
    }
}

void TouchDragScroll() {
    Step(ImGui::GetCurrentWindow(), ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows));
}

void TouchDragScrollPopups() {
    ImGuiContext* g = ImGui::GetCurrentContext();
    static int sLastFrame = -1;
    if (g == nullptr || g->FrameCount == sLastFrame) {
        return; // once per frame, whichever caller gets here first
    }
    sLastFrame = g->FrameCount;
    for (ImGuiWindow* win : g->Windows) {
        if (win == nullptr || !win->WasActive || !(win->Flags & ImGuiWindowFlags_Popup) ||
            (win->Flags & (ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_Modal | ImGuiWindowFlags_ChildWindow))) {
            continue;
        }
        // The colour picker popup uses drags for its own controls.
        if (std::strstr(win->Name, "picker") != nullptr) {
            continue;
        }
        bool hovered = false;
        for (ImGuiWindow* h = g->HoveredWindow; h != nullptr; h = h->ParentWindow) {
            if (h == win) {
                hovered = true;
                break;
            }
        }
        Step(win, hovered);
    }
}

} // namespace PaperboatGui
