// Runs with port resources only. Synthetic save values never leave the temporary test directory.
#include "port/Engine.h"
#include "port/ui/PaperboatGui.hpp"
#include "port/ui/NativeSettings.h"
#include "port/enhancements/gameplay/ActorBlueprints.h"
#include "common_structs.h"
#include <cmath>
#include <iostream>
#include <queue>
#include <set>
#include <stdexcept>
#include <thread>
#undef End
extern "C" {
#include "dx/versioning.h"
extern SaveData gCurrentSaveFile;
extern PlayerData gPlayerData;
}
namespace N = Paperboat::NativeSettings;
static void Require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
static N::Row Find(const N::Page& page, const std::string& title) {
    for (const auto& section : page.sections)
        for (const auto& row : section.rows)
            if (row.title == title)
                return row;
    throw std::runtime_error("Missing row: " + title);
}
int main() {
    try {
        GameEngine::Instance = new GameEngine();
        auto context = Ship::Context::GetRawInstance();
        context->InitAudio({ .SampleRate = 32000, .SampleLength = 1024, .DesiredBuffered = 1680 });
        PaperboatGui::SetupGuiElements();
        auto& menu = *PaperboatGui::mPaperboatMenu;
        N::Frame(menu);
        int queued = 0;
        std::thread producer([&] {
            N::Enqueue([&] {
                ++queued;
                N::Enqueue([&] { ++queued; });
            });
        });
        producer.join();
        Require(queued == 0, "UIKit queue executed on producer thread");
        N::Frame(menu);
        Require(queued == 1, "Queue must defer nested actions to next frame");
        N::Frame(menu);
        Require(queued == 2, "Nested action lost");
        const auto navigation = N::BuildNavigation();
        Require(navigation.size() == 4, "Expected four navigation groups");
        const auto empty = N::SaveEditorPage("save/root");
        Require(!Find(empty, "Player").enabled, "Save editing enabled without a save");
        Require(N::LoadImage("__OTR__sprites/not-present").rgba.empty(), "Missing sprite must remain blank");
        N::RememberPage("menu/Enhancements/Gameplay");
        Require(N::InitialPage() == "menu/Enhancements/Gameplay", "Navigation selection not restored");
        N::RememberPage("save/items");
        Require(N::InitialPage() == "menu/Enhancements/Gameplay", "Nested page overwrote navigation preference");
        gCurrentSaveFile.magicString[0] = 'T';
        gPlayerData.hardMaxHP = gPlayerData.curMaxHP = gPlayerData.curHP = 10;
        gPlayerData.hardMaxFP = gPlayerData.curMaxFP = gPlayerData.curFP = 5;
        gPlayerData.starPower = 257;
        gPlayerData.maxStarPower = 2;
        auto player = N::SaveEditorPage("save/player");
        Require(gPlayerData.starPower == 257, "Reading editor rounded fractional energy");
        auto coins = Find(player, "Coins");
        coins.change(9999);
        Require(gPlayerData.coins == 999, "Coins not clamped");
        gCurrentSaveFile.magicString[0] = 0;
        coins.change(0);
        Require(gPlayerData.coins == 999, "Stale editor wrote after save unloaded");
        gCurrentSaveFile.magicString[0] = 'T';
        auto gameplay = menu.NativePage("menu/Enhancements/Gameplay");
        auto sprint = Find(gameplay, "Sprint Button");
        sprint.change(1);
        Require(CVarGetInteger("gEnhancements.SprintButton", 0) == 1, "Native control did not write its setting");
        CVarSetInteger("gSettings.DisableChanges", 1);
        sprint.change(0);
        Require(CVarGetInteger("gEnhancements.SprintButton", 0) == 1, "Stale callback bypassed race lock");
        CVarSetInteger("gSettings.DisableChanges", 0);
        sprint.change(0);
        auto graphics = menu.NativePage("menu/Settings/Graphics");
        CVarSetInteger("gSettings.AdvancedResolution.Enabled", 1);
        graphics = menu.NativePage("menu/Settings/Graphics");
        auto width = Find(graphics, "Width");
        width.change(16);
        Require(CVarGetFloat("gSettings.AdvancedResolution.AspectRatioX", 0) == 16, "Custom aspect ratio not saved");
        std::queue<std::string> pending;
        std::set<std::string> visited;
        for (const auto& group : navigation)
            for (const auto& page : group.pages)
                pending.push(page.key);
        size_t rows = 0, portraits = 0;
        while (!pending.empty()) {
            auto key = pending.front();
            pending.pop();
            if (!visited.insert(key).second)
                continue;
            const auto page = N::BuildPage(key);
            Require(!page.title.empty(), "Missing page title");
            for (const auto& section : page.sections)
                for (const auto& row : section.rows) {
                    ++rows;
                    Require(!row.title.empty(), "Empty row title");
                    if (row.kind == N::Kind::Link) {
                        Require(!row.target.empty(), "Missing destination");
                        pending.push(row.target);
                    }
                    if (row.kind == N::Kind::Integer || row.kind == N::Kind::Decimal) {
                        Require(
                            std::isfinite(row.minimum) && std::isfinite(row.maximum) && row.minimum <= row.maximum,
                            "Invalid slider range"
                        );
                        Require(row.step > 0, "Invalid slider step");
                    }
                    if (row.portrait && portraits < 4) {
                        Require(row.portrait().rgba.empty(), "Unexpected game assets in fixture");
                        ++portraits;
                    }
                }
        }
        const auto catalog = ActorBlueprints::GetCatalog();
        Require(!catalog.empty(), "Blueprint catalog empty");
        const auto bp =
            N::BlueprintEditorPage("blueprint/actor/" + std::to_string(catalog.front().actorType) + "/combat");
        Require(!bp.sections.empty(), "Missing actor combat fields");
        std::cout
            << "PASS: " << visited.size() << " native pages, " << rows
            << " rows; queued callbacks, race lock, navigation, save guards, resolution persistence, blank portraits\n";
        N::Stop();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
