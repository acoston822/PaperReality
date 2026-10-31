#include "LeafTrace.h"
#if defined(PAPERREALITY_LEAF_DIAGNOSTICS) && PAPERREALITY_LEAF_DIAGNOSTICS
#include "port/Engine.h"
#include "port/FilePicker.h"
#include "port/ShipInit.hpp"
#include "port/hooks/Events.h"
#include <fstream>
namespace {
int32_t Command(std::shared_ptr<Ship::Console>, std::vector<std::string> args, std::string* output) {
    if (!output)
        return 1;
    if (args.size() != 2) {
        *output = "Usage: leaftrace start|stop|status|export. Capture in Goomba Road 3 (kmr_07).";
        return 1;
    }
    if (args[1] == "start") {
        Paperboat::Diagnostics::StartLeafTrace();
        *output = "Leaf capture armed. Close Settings in Goomba Road 3. Stand still, move, then run against a wall. "
                  "Captures up to 600 game frames; Settings time is excluded.";
    } else if (args[1] == "stop") {
        Paperboat::Diagnostics::StopLeafTrace();
        *output = "Leaf capture stop requested. Use leaftrace export to save the text report.";
    } else if (args[1] == "status") {
        *output = Paperboat::Diagnostics::LeafTraceStatus();
    } else if (args[1] == "export") {
        if (!Paperboat::Diagnostics::HasLeafTraceFrames()) {
            *output = "No captured frames yet. Run leaftrace start, close Settings in Goomba Road 3, and play before "
                      "exporting.";
            return 1;
        }
        auto report = std::make_shared<const std::string>(Paperboat::Diagnostics::LeafTraceReport());
        Ship::FileBrowserRequest request;
        request.Title = "Export Leaf Diagnostics";
        request.Save = true;
        request.DefaultName = "PaperReality-Leaf-Diagnostics.txt";
        request.Filters = { { "Text report", { "*.txt" } } };
        Paperboat::PickFile(std::move(request), [report](std::optional<std::filesystem::path> path) {
            if (!path)
                return;
            std::ofstream file(*path, std::ios::binary | std::ios::trunc);
            file << *report;
            file.close();
            if (!file)
                GameEngine_LogInfo("[leaftrace] Could not write the diagnostic report");
        });
        *output = "Exporting PaperReality-Leaf-Diagnostics.txt through Files.";
    } else {
        *output = "Usage: leaftrace start|stop|status|export";
        return 1;
    }
    return 0;
}
void Register() {
    Ship::Context::GetRawInstance()->GetConsole()->AddCommand(
        "leaftrace", { Command, "Capture/export leaf rendering metadata: start, stop, status, export." }
    );
    REGISTER_LISTENER(OnMapLoad, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        Paperboat::Diagnostics::SetLeafTraceMap(reinterpret_cast<OnMapLoad*>(event)->mapName);
    });
}
RegisterShipInitFunc init(Register);
} // namespace
#endif
