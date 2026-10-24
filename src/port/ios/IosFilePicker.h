#pragma once

#include <functional>
#include <string>
#include <vector>

#if defined(__IOS__)
namespace Paperboat {
// Uses App Files with a controller connected, or the system Files picker. onPicked gets the paths of private copies of the chosen files (inside the app's
// temporary directory, original file names kept), or an empty list if the user cancelled.
void IosPickFiles(bool allowMultiple, std::function<void(std::vector<std::string>)> onPicked);

// Export a generated file through App Files or the system Files picker.
void IosExportFile(std::string path, std::function<void(bool)> onDone = { });

// Opens App Files with a controller, otherwise the Documents folder in Files.
void IosOpenAppFiles(std::string path);

// Single-file convenience: the copy's path, or an empty string on cancel.
void IosPickFile(std::function<void(std::string)> onPicked);
} // namespace Paperboat
#endif
