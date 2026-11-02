#include "port/FilePicker.h"
#include "spdlog/spdlog.h"

#if defined(__IOS__)
#include "port/ios/IosFilePicker.h"
#include "port/ui/NativeSettings.h"
#include "port/ui/PaperboatGui.hpp"
#include <atomic>
#endif

#if PAPERBOAT_NATIVE_FILE_DIALOG
#include <cstdlib>
#include <string>
#include <vector>
#include "portable-file-dialogs.h"
#endif

namespace fs = std::filesystem;

namespace Paperboat {

#if PAPERBOAT_NATIVE_FILE_DIALOG
// portable-file-dialogs takes a flat filter list: { label, "*.a *.b", label2, "*.c", ... }.
static std::vector<std::string> ToPfdFilters(const std::vector<Ship::FileFilter>& filters) {
    std::vector<std::string> out;
    for (const auto& filter : filters) {
        out.push_back(filter.Label);
        std::string patterns;
        for (const auto& pattern : filter.Patterns) {
            if (!patterns.empty()) {
                patterns += " ";
            }
            patterns += pattern;
        }
        out.push_back(patterns);
    }
    if (out.empty()) {
        out = { "All Files", "*" };
    }
    return out;
}
#endif

#if PAPERBOAT_NATIVE_FILE_DIALOG
static bool NativeDialogUsable() {
    if (std::getenv("GAMESCOPE_WAYLAND_DISPLAY") != nullptr) {
        return false;
    }
    return pfd::settings::available();
}
#endif

void PickFile(Ship::FileBrowserRequest request, std::function<void(std::optional<fs::path>)> onResult) {
#if defined(__IOS__)
    // The ImGui browser is covered by the native settings screen. Use Files for
    // every iOS caller and return to the game queue before importing game state.
    if (!request.Save) {
        IosPickFile([onResult = std::move(onResult)](std::string picked) {
            NativeSettings::Enqueue([onResult, picked = std::move(picked)] {
                if (!onResult)
                    return;
                if (picked.empty()) {
                    onResult(std::nullopt);
                    return;
                }
                const fs::path path(picked);
                onResult(path);
                // Files imported as copies are temporary; never delete a caller's
                // original file or leave private imports in the app indefinitely.
                std::error_code error;
                fs::remove(path, error);
            });
        });
        return;
    }
    // Existing save callbacks write to a path. Generate the file privately, then
    // let the system export a copy to the destination chosen by the user.
    static std::atomic<uint64_t> serial { 0 };
    const auto folder = fs::temp_directory_path() / ("PaperReality-export-" + std::to_string(++serial));
    std::error_code error;
    fs::create_directories(folder, error);
    if (error) {
        if (onResult)
            onResult(std::nullopt);
        PaperboatGui::RegisterPopup("Export File", "Could not create a temporary export file.");
        return;
    }
    const auto path =
        folder / (request.DefaultName.empty() ? fs::path("export.json") : fs::path(request.DefaultName).filename());
    if (onResult)
        onResult(path);
    if (!fs::is_regular_file(path, error)) {
        fs::remove_all(folder, error);
        return;
    }
    IosExportFile(path.string(), [folder](bool) {
        NativeSettings::Enqueue([folder] {
            std::error_code error;
            fs::remove_all(folder, error);
        });
    });
#elif PAPERBOAT_NATIVE_FILE_DIALOG
    if (!NativeDialogUsable()) {
        SPDLOG_INFO("Native file dialog unavailable, using the in-game file browser.");
        request.OnResult = std::move(onResult);
        Ship::FileBrowserWindow::Open(std::move(request));
        return;
    }

    const std::string startDir = request.StartDir.empty() ? "." : request.StartDir.string();
    const std::vector<std::string> filters = ToPfdFilters(request.Filters);

    std::optional<fs::path> result;
    if (pfd::settings::available()) {
        if (request.Save) {
            const std::string defaultPath = (fs::path(startDir) / request.DefaultName).string();
            std::string selection = pfd::save_file(request.Title, defaultPath, filters).result();
            if (!selection.empty()) {
                result = fs::path(selection);
            }
        } else {
            std::vector<std::string> selection = pfd::open_file(request.Title, startDir, filters).result();
            if (!selection.empty()) {
                result = fs::path(selection.front());
            }
        }
    } else {
        SPDLOG_ERROR("portable-file-dialogs is not available on this system.");
    }
    if (onResult) {
        onResult(result);
    }
#else
    request.OnResult = std::move(onResult);
    Ship::FileBrowserWindow::Open(std::move(request));
#endif
}

} // namespace Paperboat
