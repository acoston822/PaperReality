#pragma once

#include <functional>
#include <string>
#include <vector>

#if defined(__IOS__)
namespace Paperboat {
// Shows the system Files picker. onPicked gets the paths of private copies of the chosen files (inside the app's
// temporary directory, original file names kept), or an empty list if the user cancelled.
void IosPickFiles(bool allowMultiple, std::function<void(std::vector<std::string>)> onPicked);

// Export a generated file with the system Files picker.
void IosExportFile(std::string path, std::function<void(bool)> onDone = { });

// Opens the app's Documents folder in Files; displays instructions if Files is unavailable.
void IosOpenAppFiles(std::string path);

// Single-file convenience: the copy's path, or an empty string on cancel.
void IosPickFile(std::function<void(std::string)> onPicked);
} // namespace Paperboat
#endif
