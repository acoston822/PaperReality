// Runs with port resources only. Synthetic save values never leave the temporary test directory.
#include "port/Engine.h"
#include "port/ui/PaperboatGui.hpp"
#include "port/ui/NativeSettings.h"
#include "port/enhancements/gameplay/ActorBlueprints.h"
#include <fast/resource/ResourceType.h>
#include <fast/resource/factory/TextureFactory.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include "common_structs.h"
#include <cmath>
#include <iostream>
#include <queue>
#include <set>
#include <stdexcept>
#include <thread>
#undef End
extern std::shared_ptr<Fast::Fast3dWindow> gsFast3dWindow;
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
static void CheckTextureSelection() {
    auto manager = Ship::Context::GetRawInstance()->GetResourceManager();
    manager->GetResourceLoader()->RegisterResourceFactory(
        std::make_shared<Fast::ResourceFactoryBinaryTextureV1>(), RESOURCE_FORMAT_BINARY, "Texture",
        static_cast<uint32_t>(Fast::ResourceType::Texture), 1
    );
    Require(manager->GetArchiveManager()->AddArchive("synthetic-textures.zip") != nullptr, "Fixture not mounted");
    const auto expectBase = [&](const char* name) {
        const std::string path = "__OTR__test-native/" + std::string(name);
        const auto image = N::LoadImage(path.c_str(), "__OTR__test-native/colors");
        Require(
            image.width == 2 && image.height == 1
                && image.rgba == std::vector<uint8_t> { 255, 0, 0, 255, 0, 255, 0, 255 },
            "Original indexed sprite or palette fallback incorrect"
        );
    };
    const auto expectHD = [&](const char* name, int width, int height, std::vector<uint8_t> color) {
        const std::string path = "__OTR__test-native/" + std::string(name);
        const auto image = N::LoadImage(path.c_str(), "__OTR__test-native/colors");
        Require(image.width == width && image.height == height, "HD sprite dimensions lost");
        Require(image.rgba.size() == size_t(width * height * 4), "HD pixel count incorrect");
        for (size_t i = 0; i < image.rgba.size(); ++i)
            Require(image.rgba[i] == color[i % 4], "HD sprite pixels changed");
    };
    manager->SetAltAssetsEnabled(false);
    expectBase("cached"); // Warm the base cache before enabling HD.
    manager->SetAltAssetsEnabled(true);
    expectHD("cached", 4, 2, { 0, 0, 255, 255 });
    expectHD("variant", 8, 4, { 255, 192, 0, 255 });
    expectHD("alias-only", 12, 6, { 255, 0, 255, 255 });
    expectHD("variant-alias", 12, 6, { 255, 0, 255, 255 });
    expectHD("raw-ci", 4, 2, { 0, 255, 255, 128 });
    for (const auto* name :
         { "partial", "unsupported", "short", "claimed", "zero", "oversized", "missing-target", "bad-meta" })
        expectBase(name);
    manager->SetAltAssetsEnabled(false);
    expectBase("cached"); // A cached replacement must not survive the toggle.
    expectBase("variant");
    Require(N::LoadImage("__OTR__test-native/alias-only").rgba.empty(), "Disabled HD alias still displayed");
    expectHD("base-alias", 12, 6, { 255, 0, 255, 255 });
    const auto ci8 = N::LoadImage("__OTR__test-native/ci8", "__OTR__test-native/ci8-colors");
    Require(ci8.rgba == std::vector<uint8_t> { 255, 255, 255, 255 }, "CI8 palette decoding incorrect");
    manager->SetAltAssetsEnabled(true);
    expectHD("cached", 4, 2, { 0, 0, 255, 255 });
    const auto report = N::SpriteTextureReport();
    Require(manager->IsAltAssetsEnabled(), "Exporting coverage changed the pack selection");
    Require(
        report.find("Selected replacement: alt/icons/badge/PowerJump") != std::string::npos,
        "Coverage report did not identify the selected HD sprite"
    );
    Require(
        report.find("Selected original: icons/badge/AutoJump") != std::string::npos,
        "Coverage report did not identify the original fallback"
    );
    Require(
        report.find("Rejected: alt/icons/badge/Berserker") != std::string::npos,
        "Coverage report did not explain an unusable replacement"
    );
    Require(
        report.find("alt/icons/badge/SuperJump -> test-native/aliases/target") != std::string::npos
            && report.find("[synthetic-textures.zip]; 12x6") != std::string::npos,
        "Coverage report lost the alias, pack name or replacement dimensions"
    );
    Require(
        report.find("/tmp/") == std::string::npos && report.size() < 15000,
        "Coverage report includes device directories or unexpected payload data"
    );
    manager->SetAltAssetsEnabled(false);
    const auto disabled = N::SpriteTextureReport();
    Require(
        !manager->IsAltAssetsEnabled() && disabled.find("Alternate assets: disabled") != std::string::npos
            && disabled.find("Selected replacement:") == std::string::npos,
        "Coverage report did not respect disabled alternate assets"
    );
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
        auto visibility = Find(N::InputViewerPage("viewer/root"), "Show Input Viewer");
        auto viewer = context->GetWindow()->GetGui()->GetGuiWindow("Input Viewer");
        visibility.change(1);
        Require(viewer->IsVisible(), "Input viewer window was not shown");
        visibility.change(0);
        Require(!viewer->IsVisible(), "Input viewer window was not hidden");
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
        const auto characters = N::BlueprintEditorPage("blueprint/category/-1");
        for (const auto* name : { "Fake Bowser", "Toy Tank", "Light Bulb" }) {
            const auto model = Find(characters, name);
            Require(
                !model.portrait && !model.symbol.empty() && model.detail.ends_with(" · Model"),
                "Model actors must have labeled icons, never an unrelated NPC sprite or blank portrait"
            );
            const auto detail = N::BuildPage(model.target);
            const auto info = Find(detail, name);
            Require(info.symbol == model.symbol && !info.portrait, "Model icon differs between list and detail");
        }
        CheckTextureSelection();
        std::cout << "PASS: " << visited.size() << " native pages, " << rows
                  << " rows; queued callbacks, race lock, navigation, save guards, resolution persistence, blank "
                     "portraits, HD texture selection\n";
        N::Stop();
        PaperboatGui::Destroy();
        gsFast3dWindow.reset();
        Ship::Context::DestroyInstance();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
