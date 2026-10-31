// Exercise the real collector with generated model metadata; no game assets or GPU.
#include "port/Engine.h"
#include "port/diagnostics/LeafTrace.h"
#include "port/diagnostics/LeafTraceCapture.h"
#include "model.h"
#include <nlohmann/json.hpp>
#include <atomic>
#include <cassert>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>

extern "C" {
Camera gCameras[4];
s32 gCurrentCameraID;
GameStatus gameStatus;
GameStatus* gGameStatusPtr = &gameStatus;
PlayerStatus gPlayerStatus;
int32_t CVarGetInteger(const char*, int32_t defaultValue) {
    return defaultValue;
}
}
namespace D = Paperboat::Diagnostics;
using Json = nlohmann::json;
static std::vector<Json> Report() {
    std::vector<Json> lines;
    std::istringstream stream(D::LeafTraceReport());
    for (std::string line; std::getline(stream, line);)
        lines.push_back(Json::parse(line));
    return lines;
}
int main() {
    // Bounded lifecycle, paused frames, request replacement and thread-safe reads.
    D::LeafTraceCapture bounded(2, 20);
    bounded.Start();
    assert(!bounded.Begin(false));
    assert(bounded.Read().frames == 0);
    assert(bounded.Begin(true));
    assert(bounded.AppendFrame("{\"frame\":1}\n"));
    assert(bounded.Begin(true));
    assert(!bounded.AppendFrame(std::string(21, 'x')));
    assert(bounded.Read().limited && bounded.Read().frames == 1);
    assert(bounded.Read().text == "{\"frame\":1}\n"); // no partial JSON line
    bounded.Start();
    assert(bounded.Begin(true));
    assert(bounded.AppendFrame("{}\n"));
    assert(bounded.Begin(true));
    assert(bounded.AppendFrame("{}\n"));
    assert(!bounded.Begin(true));
    assert(bounded.Read().frames == 2);
    bounded.Start();
    bounded.Stop();
    assert(!bounded.Begin(true));
    D::LeafTraceCapture threaded(5000, 100000);
    std::atomic<bool> done = false;
    std::thread reader([&] {
        while (!done.load()) {
            auto snapshot = threaded.Read();
            assert(snapshot.text.size() <= 100000);
        }
    });
    for (int i = 0; i < 200; ++i) {
        threaded.Start();
        if (threaded.Begin(true))
            threaded.AppendFrame("{}\n");
    }
    done = true;
    reader.join();

    struct Properties {
        ModelBoundingBox bounds;
        ModelNodeProperty texture;
    } properties {};
    static_assert(sizeof(Properties) == 7 * sizeof(ModelNodeProperty));
    properties.bounds.key = MODEL_PROP_KEY_BOUNDING_BOX;
    properties.bounds.minX = 10;
    properties.bounds.maxX = 20;
    properties.bounds.minY = 0;
    properties.bounds.maxY = 1;
    properties.bounds.minZ = 30;
    properties.bounds.maxZ = 40;
    properties.texture.key = MODEL_PROP_KEY_TEXTURE_NAME;
    properties.texture.data.p = const_cast<char*>("synthetic_leaf");
    ModelNode node {};
    node.type = SHAPE_TYPE_MODEL;
    node.numProperties = 7;
    node.propertyList = reinterpret_cast<ModelNodeProperty*>(&properties);
    Model leaf {};
    leaf.flags = MODEL_FLAG_VALID;
    leaf.modelID = 12;
    leaf.modelNode = &node;
    leaf.renderMode = RENDER_MODE_DECAL_OPA;
    Mtx matrix {};
    leaf.finalMtx = &matrix;
    ModelList models {};
    models[0] = &leaf;
    gCurrentModels = &models;
    gGameStatusPtr->context = CONTEXT_WORLD;
    gCurrentCameraID = CAM_DEFAULT;
    Gfx material[] = {
        gsDPSetRenderMode(G_RM_ZB_OPA_DECAL, G_RM_ZB_OPA_DECAL2),
        gsSPClearGeometryMode(G_CULL_BOTH | G_LIGHTING),
        gsSPSetGeometryMode(G_ZBUFFER | G_SHADE),
        gsSPEndDisplayList(),
    };
    const auto original = leaf;
    Gfx originalMaterial[4];
    std::memcpy(originalMaterial, material, sizeof(material));
    D::SetLeafTraceMap("kmr_07");
    LeafTrace_BeginFrame();
    LeafTrace_EndFrame();
    assert(!D::HasLeafTraceFrames()); // opt-in
    D::SetLeafTraceMap("kmr_06");
    D::StartLeafTrace();
    LeafTrace_BeginFrame();
    LeafTrace_EndFrame();
    assert(!D::HasLeafTraceFrames()); // only the target map
    D::SetLeafTraceMap("kmr_07");
    for (int i = 0; i < 2; ++i) {
        gGameStatusPtr->frameCounter = i;
        gGameStatusPtr->stickX[0] = i ? 60 : 0;
        LeafTrace_BeginFrame();
        LeafTrace_Queued(&leaf, 5000);
        if (i)
            LeafTrace_Culled(&leaf);
        LeafTrace_Task(reinterpret_cast<void*>(1), &leaf, 10000);
        LeafTrace_Draw(&leaf, 0, 2, material);
        LeafTrace_EndFrame();
    }
    auto lines = Report();
    assert(lines.size() == 4 && lines.front()["format"] == 1 && lines.back()["frames"] == 2);
    const auto& standing = lines[1];
    const auto& running = lines[2];
    assert(standing["inventory"][0]["texture"] == "synthetic_leaf");
    assert(standing["inventory"][0]["bounds"] == Json({ 10, 0, 30, 20, 1, 40 }));
    assert(running["inventory"].empty());
    assert(standing["player"][0] == running["player"][0] && running["player"][7] == 60);
    assert(standing["draws"] == running["draws"] && standing["tasks"] == running["tasks"]);
    assert(standing["culled"].empty() && running["culled"].size() == 1);
    assert(standing["draws"][0][7] == material[0].words.w1); // correct material render bits
    assert(standing["draws"][0][13] == true);
    assert(!std::memcmp(&original, &leaf, sizeof(leaf)));
    assert(!std::memcmp(originalMaterial, material, sizeof(material)));
    D::StopLeafTrace();
    LeafTrace_BeginFrame();
    LeafTrace_EndFrame();
    assert(Report().back()["frames"] == 2);
    D::StartLeafTrace();
    for (int i = 0; i < 605; ++i) {
        LeafTrace_BeginFrame();
        LeafTrace_EndFrame();
    }
    assert(Report().back()["frames"] == 600 && Report().back()["active"] == false);
    D::StartLeafTrace();
    LeafTrace_BeginFrame();
    LeafTrace_Draw(&leaf, 0, 2, material);
    D::SetLeafTraceMap("kmr_06");
    LeafTrace_EndFrame();
    assert(Report().back()["frames"] == 0); // do not finish a frame after unload
    std::cout << "PASS: real leaf collector opt-in/map gates, metadata, standing/wall input, stable IDs, stop/limits, "
                 "concurrent reads and read-only engine data\n";
}
