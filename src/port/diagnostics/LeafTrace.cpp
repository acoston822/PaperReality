#include "LeafTrace.h"

#if defined(PAPERREALITY_LEAF_DIAGNOSTICS) && PAPERREALITY_LEAF_DIAGNOSTICS
#include "LeafTraceCapture.h"
#include "port/Engine.h"
#include "port/ui/NativeSettings.h"
#include "model.h"
#include "mapfs/kmr_07_shape.h"
#include <cmath>
#include <cstring>
#include <nlohmann/json.hpp>
#include <unordered_map>

namespace {
using Json = nlohmann::json;
using Paperboat::Diagnostics::LeafTraceCapture;
LeafTraceCapture capture;
std::atomic<float> testLift { -1.0f };
// Everything below is owned by the game thread, not the console/UI queue.
std::string map;
bool sampling = false;
unsigned generation = 0;
std::unordered_map<const void*, unsigned> nodes, functions, arguments;
Json frame;

unsigned Token(std::unordered_map<const void*, unsigned>& table, const void* key) {
    const auto [entry, inserted] = table.emplace(key, table.size() + 1);
    return entry->second;
}
std::string Name(const char* text) {
    std::string name;
    if (text) {
        for (size_t i = 0; text[i] && i < 64; ++i) {
            const unsigned char c = text[i];
            name += (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.'
                ? c
                : '_';
        }
    }
    return name;
}
unsigned Node(const Model* model) {
    if (!model || !model->modelNode)
        return 0;
    const auto found = nodes.find(model->modelNode);
    if (found != nodes.end())
        return found->second;
    unsigned id = Token(nodes, model->modelNode);
    auto* texture = get_model_property(model->modelNode, MODEL_PROP_KEY_TEXTURE_NAME);
    auto* bounds =
        reinterpret_cast<ModelBoundingBox*>(get_model_property(model->modelNode, MODEL_PROP_KEY_BOUNDING_BOX));
    Json description = { { "node", id },
                         { "owner_tree_id", model->modelID },
                         { "type", model->modelNode->type },
                         { "texture", Name(texture ? static_cast<const char*>(texture->data.p) : nullptr) },
                         { "center", { model->center.x, model->center.y, model->center.z } },
                         { "flags", model->flags },
                         { "render_mode", model->renderMode } };
    if (bounds)
        description["bounds"] = { bounds->minX, bounds->minY, bounds->minZ, bounds->maxX, bounds->maxY, bounds->maxZ };
    frame["inventory"].push_back(std::move(description));
    return id;
}
uint32_t MatrixHash(const Mtx* matrix) {
    if (!matrix)
        return 0;
    uint32_t result = 2166136261u;
    const auto* bytes = reinterpret_cast<const unsigned char*>(matrix);
    for (size_t i = 0; i < sizeof(*matrix); ++i)
        result = (result ^ bytes[i]) * 16777619u;
    return result;
}
Json Header() {
    return { { "type", "header" },
             { "format", 2 },
             { "name", "Render Diagnostics" },
             { "map", "kmr_07" },
             { "branch", "experimental/leaf-render-diagnostics" },
             { "upstream", "6cecd01a7" },
             { "scope", "CPU command submission metadata; does not establish GPU pixel visibility" },
             { "leaf_lift_modes", { { "off", -1 }, { "identity_control", 0 }, { "lift_range", { 0, 1 } } } },
             { "privacy", "No images, textures, saves, raw display lists, vertices, device paths or memory addresses" },
             { "limits", { { "frames", 600 }, { "bytes", 6 * 1024 * 1024 } } },
             { "queue_fields", { "node", "distance" } },
             { "task_fields", { "function_token", "argument_token", "priority" } },
             { "draw_fields",
               { "node", "flags", "render_mode", "render_class", "material_index", "geometry_set", "geometry_clear",
                 "render_bits", "other_mode_h", "texture_id", "texture_variation", "matrix_hash", "matrix_freshness",
                 "geometry_emitted" } },
             { "player_fields", { "x", "y", "z", "speed", "action", "animation", "flags", "stick_x", "stick_y" } } };
}
std::string Report() {
    const auto snapshot = capture.Read();
    Json summary = { { "type", "summary" },
                     { "frames", snapshot.frames },
                     { "active", snapshot.active },
                     { "pending_request", snapshot.pending },
                     { "byte_limit_reached", snapshot.limited } };
    return Header().dump() + "\n" + snapshot.text + summary.dump() + "\n";
}
} // namespace

namespace Paperboat::Diagnostics {
void StartLeafTrace() {
    capture.Start();
}
void StopLeafTrace() {
    capture.Stop();
}
bool HasLeafTraceFrames() {
    return capture.Read().frames != 0;
}
std::string LeafTraceReport() {
    return Report();
}
std::string LeafTraceStatus() {
    const auto snapshot = capture.Read();
    return "Render capture: " + std::to_string(snapshot.frames) + " frames, " + std::to_string(snapshot.text.size())
        + " bytes; "
        + (snapshot.pending      ? "request pending"
               : snapshot.active ? "armed/recording"
                                 : "stopped")
        + (snapshot.limited ? "; byte limit reached" : "");
}
void SetLeafTraceMap(const char* name) {
    testLift.store(-1.0f);
    map = Name(name);
    sampling = false;
    nodes.clear();
    functions.clear();
    arguments.clear();
    if (map != "kmr_07" || capture.Read().frames)
        capture.Stop();
}
bool SetLeafTestLift(float amount) {
    if (!std::isfinite(amount) || (amount != -1.0f && (amount < 0.0f || amount > 1.0f)))
        return false;
    testLift.store(amount);
    return true;
}
float LeafTestLift() {
    return testLift.load();
}
} // namespace Paperboat::Diagnostics

extern "C" void LeafTrace_BeginFrame(void) {
    sampling = false;
    if (!capture.Begin(
            map == "kmr_07" && gGameStatusPtr && gGameStatusPtr->context == CONTEXT_WORLD
            && gCurrentCameraID == CAM_DEFAULT && !Paperboat::NativeSettings::IsPresented()
        ))
        return;
    const unsigned next = capture.Generation();
    if (generation != next) {
        generation = next;
        nodes.clear();
        functions.clear();
        arguments.clear();
    }
    sampling = true;
    const auto& player = gPlayerStatus;
    const auto& camera = gCameras[gCurrentCameraID];
    frame = { { "type", "frame" },
              { "tick", gGameStatusPtr->frameCounter },
              { "camera_id", gCurrentCameraID },
              { "player",
                { player.pos.x, player.pos.y, player.pos.z, player.curSpeed, player.actionState, player.anim,
                  player.flags, gGameStatusPtr->stickX[0], gGameStatusPtr->stickY[0] } },
              { "camera",
                { camera.lookAt_eye.x, camera.lookAt_eye.y, camera.lookAt_eye.z, camera.lookAt_obj.x,
                  camera.lookAt_obj.y, camera.lookAt_obj.z } },
              { "viewport", { camera.viewportStartX, camera.viewportStartY, camera.viewportW, camera.viewportH } },
              { "configured_fps", CVarGetInteger("gSettings.InterpolationFPS", 30) },
              { "match_refresh_rate", CVarGetInteger("gSettings.MatchRefreshRate", 0) },
              { "inventory", Json::array() },
              { "model_flags", Json::array() },
              { "queued", Json::array() },
              { "culled", Json::array() },
              { "draws", Json::array() },
              { "tasks", Json::array() },
              { "leaf_lift_mode", testLift.load() },
              { "leaf_lift_applied", Json::array() },
              { "z_fighting", CVarGetInteger("gSettings.ZFightingMode", 0) },
              { "alternate_assets", CVarGetInteger("gEnhancements.Mods.AlternateAssets", 0) } };
    if (gCurrentModels) {
        for (auto* model : *gCurrentModels) {
            if (model)
                frame["model_flags"].push_back({ Node(model), model->flags, model->matrixFreshness });
        }
    }
}
extern "C" void LeafTrace_Queued(const void* data, int distance) {
    if (sampling)
        frame["queued"].push_back({ Node(static_cast<const Model*>(data)), distance });
}
extern "C" void LeafTrace_Culled(const void* data) {
    if (sampling)
        frame["culled"].push_back(Node(static_cast<const Model*>(data)));
}
extern "C" void LeafTrace_Task(const void* function, const void* argument, int priority) {
    if (sampling)
        frame["tasks"].push_back({ Token(functions, function), Token(arguments, argument), priority });
}
extern "C" void LeafTrace_Draw(const void* data, int renderClass, int materialIndex, const void* material) {
    if (!sampling)
        return;
    const auto* model = static_cast<const Model*>(data);
    uint32_t geometrySet = 0, geometryClear = 0, renderBits = 0, otherH = 0;
    const auto* list = static_cast<const Gfx*>(material);
    // These are the port's small static material-state lists, never model data.
    for (size_t i = 0; list && i < 16; ++i) {
        const unsigned opcode = list[i].words.w0 >> 24;
        if (opcode == G_ENDDL)
            break;
        if (opcode == G_GEOMETRYMODE) {
            geometryClear |= ~uint32_t(list[i].words.w0) & 0xFFFFFF;
            geometrySet |= uint32_t(list[i].words.w1);
        } else if (opcode == G_SETOTHERMODE_L)
            renderBits = uint32_t(list[i].words.w1);
        else if (opcode == G_SETOTHERMODE_H)
            otherH = uint32_t(list[i].words.w1);
    }
    frame["draws"].push_back(
        { Node(model), model->flags, model->renderMode, renderClass, materialIndex, geometrySet, geometryClear,
          renderBits, otherH, model->textureID, model->textureVariation, MatrixHash(model->finalMtx),
          model->matrixFreshness, !(model->flags & MODEL_FLAG_HAS_LOCAL_VERTEX_COPY) }
    );
}
extern "C" void LeafTrace_EndFrame(void) {
    if (!sampling)
        return;
    sampling = false;
    capture.AppendFrame(frame.dump() + "\n");
}

extern "C" int LeafTrace_BeginLeafDraw(const void* data) {
    const float lift = testLift.load();
    const auto* model = static_cast<const Model*>(data);
    if (lift < 0 || map != "kmr_07" || !model || !model->modelNode || model->modelID != MODEL_o853
        || model->modelNode->type != SHAPE_TYPE_MODEL || model->renderMode != RENDER_MODE_ALPHATEST || !gGameStatusPtr
        || gGameStatusPtr->context != CONTEXT_WORLD || gCurrentCameraID != CAM_DEFAULT || !gDisplayContext
        || !gMainGfxPos || gMatrixListPos >= ARRAY_COUNT(gDisplayContext->matrixStack))
        return 0;
    auto* texture = get_model_property(model->modelNode, MODEL_PROP_KEY_TEXTURE_NAME);
    if (!texture || !texture->data.p || std::strcmp(static_cast<const char*>(texture->data.p), "kmr_flower_01tif"))
        return 0;

    // Temporary leaf-only draw transform. Preserve the authored model, material,
    // alpha test, depth flags and collision data. Zero deliberately follows the
    // same command path as a positive lift, controlling for a matrix/state flush.
    Matrix4f translation;
    Mtx* matrix = &gDisplayContext->matrixStack[gMatrixListPos++];
    FrameInterpolation_RecordOpenChild("leaf_ground_test", reinterpret_cast<uintptr_t>(model->modelNode));
    guTranslateF(translation, 0, lift, 0);
    guMtxF2L(translation, matrix);
    FrameInterpolation_RecordCloseChild();
    gSPMatrix(gMainGfxPos++, matrix, G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    if (sampling)
        frame["leaf_lift_applied"].push_back({ Node(model), lift });
    return 1;
}

extern "C" void LeafTrace_EndLeafDraw(int pushed) {
    if (pushed)
        gSPPopMatrix(gMainGfxPos++, G_MTX_MODELVIEW);
}
#endif
