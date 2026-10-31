#include "LeafTrace.h"
#if defined(PAPERREALITY_LEAF_DIAGNOSTICS) && PAPERREALITY_LEAF_DIAGNOSTICS
#include "port/Engine.h"
#include "port/FilePicker.h"
#include "port/ShipInit.hpp"
#include "port/hooks/Events.h"
#include <fstream>
#include <stdexcept>
namespace {
int32_t Command(std::shared_ptr<Ship::Console>, std::vector<std::string> args, std::string* output) {
    if (!output)
        return 1;
    if (args.size() == 3 && args[1] == "lift") {
        float amount = -1;
        if (args[2] != "off") {
            try {
                size_t used = 0;
                amount = std::stof(args[2], &used);
                if (used != args[2].size() || amount < 0)
                    throw std::invalid_argument("lift");
            } catch (const std::exception&) {
                *output = "Use rendertrace lift off, rendertrace lift 0 (identity control), or a value from 0 to 1.";
                return 1;
            }
        }
        if (!Paperboat::Diagnostics::SetLeafTestLift(amount)) {
            *output = "Lift must be finite and between 0 and 1, or off.";
            return 1;
        }
        *output = amount < 0 ? "Leaf test off: original drawing path."
            : amount == 0    ? "Leaf test: identity-matrix control; no vertical offset."
                             : "Leaf test: upward offset " + std::to_string(amount) + " world units.";
        *output += " This affects only the identified leaf in Goomba Road 3 and resets on map load or app restart.";
        return 0;
    }
    if (args.size() != 2) {
        *output = "Usage: rendertrace start|stop|status|export, or rendertrace lift off|0|0.25. Capture in Goomba Road "
                  "3 (kmr_07).";
        return 1;
    }
    if (args[1] == "start") {
        Paperboat::Diagnostics::StartLeafTrace();
        *output = "Render capture armed. Close Settings in Goomba Road 3. Stand still, move, then run against a wall. "
                  "Captures up to 600 game frames; Settings time is excluded.";
    } else if (args[1] == "stop") {
        Paperboat::Diagnostics::StopLeafTrace();
        *output = "Render capture stop requested. Use rendertrace export to save the text report.";
    } else if (args[1] == "status") {
        *output = Paperboat::Diagnostics::LeafTraceStatus();
    } else if (args[1] == "export") {
        if (!Paperboat::Diagnostics::HasLeafTraceFrames()) {
            *output = "No captured frames yet. Run rendertrace start, close Settings in Goomba Road 3, and play before "
                      "exporting.";
            return 1;
        }
        auto report = std::make_shared<const std::string>(Paperboat::Diagnostics::LeafTraceReport());
        Ship::FileBrowserRequest request;
        request.Title = "Export Render Diagnostics";
        request.Save = true;
        request.DefaultName = "PaperReality-Render-Diagnostics.txt";
        request.Filters = { { "Text report", { "*.txt" } } };
        Paperboat::PickFile(std::move(request), [report](std::optional<std::filesystem::path> path) {
            if (!path)
                return;
            std::ofstream file(*path, std::ios::binary | std::ios::trunc);
            file << *report;
            file.close();
            if (!file)
                GameEngine_LogInfo("[rendertrace] Could not write the diagnostic report");
        });
        *output = "Exporting PaperReality-Render-Diagnostics.txt through Files.";
    } else {
        *output = "Usage: rendertrace start|stop|status|export, or rendertrace lift off|0|0.25";
        return 1;
    }
    return 0;
}
void Register() {
    Ship::Context::GetRawInstance()->GetConsole()->AddCommand(
        "rendertrace",
        { Command, "Render Diagnostics: start, stop, status, export; lift off|0|0.25 for the leaf-only test." }
    );
    Ship::Context::GetRawInstance()->GetConsole()->AddCommand(
        "leaftrace", { Command, "Compatibility alias for rendertrace." }
    );
    REGISTER_LISTENER(OnMapLoad, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        Paperboat::Diagnostics::SetLeafTraceMap(reinterpret_cast<OnMapLoad*>(event)->mapName);
    });
}
RegisterShipInitFunc init(Register);
} // namespace
#endif
