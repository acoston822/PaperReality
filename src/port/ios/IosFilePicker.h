#pragma once

#include <functional>
#include <string>
#include <vector>

#if defined(__IOS__)
namespace Paperboat {
// Shows the system Files picker. onPicked gets the paths of private copies of the chosen files (inside the app's
// temporary directory, original file names kept), or an empty list if the user cancelled.
void IosPickFiles(bool allowMultiple, std::function<void(std::vector<std::string>)> onPicked);

// Single-file convenience: the copy's path, or an empty string on cancel.
void IosPickFile(std::function<void(std::string)> onPicked);
} // namespace Paperboat
#endif
