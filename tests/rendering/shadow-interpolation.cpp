#include "port/Engine.h"
#include "port/interpolation/FrameInterpolation.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static uint32_t targetFPS = 60;
uint32_t GameEngine::GetInterpolationFPS() { return targetFPS; }
extern "C" {
void shadow_test_begin(int slot);
void shadow_test_draw(int id, int path, int reflect, int shadow, float x, float z);
void shadow_test_end(int slot);
int shadow_test_count(int slot);
Mtx* shadow_test_matrix(int slot, int index);
void guMtxL2F(float mf[4][4], Mtx* matrix);
void shadow_test_draw_height(int id, int path, int reflect, int shadow, float x, float y, float z);
Mtx* shadow_test_support(int slot, float y);
void guMtxF2L(float mf[4][4], Mtx* matrix);
void guMtxF2L_NoInterp(float mf[4][4], Mtx* matrix);
}
static void require(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
int main(int argc, char** argv) {
    for (auto fps : { 60u, 120u }) {
        targetFPS = fps;
        for (int path = 0; path < 3; ++path) {
            for (int reflectionMode = 0; reflectionMode < 4; ++reflectionMode) {
                int prev, curr;
                bool should;
                Mtx saved[8];
                for (int frame = 0; frame < 2; ++frame) {
                    FrameInterpolation_StartRecord();
                    shadow_test_begin(frame);
                    // Reverse sorting order between frames. Model identity, not ordinal,
                    // must determine the previous position for each emitted matrix.
                    int reflect = reflectionMode == 1 || (reflectionMode == 2 && frame == 0) ||
                                  (reflectionMode == 3 && frame == 1);
                    for (int j = 0; j < 4; ++j) {
                        int id = frame ? 3 - j : j;
                        shadow_test_draw(id, path, reflect, id != 3, id * 100.0f + frame * 10.0f, 20.0f + frame * 4.0f);
                    }
                    shadow_test_end(frame);
                    FrameInterpolation_GetRecordingPair(&prev, &curr, &should);
                    FrameInterpolation_StopRecord();
                    if (!frame) {
                        for (int j = 0; j < shadow_test_count(0); ++j) saved[j] = *shadow_test_matrix(0, j);
                    }
                }
                for (int j = 0; j < shadow_test_count(0); ++j) {
                    require(!std::memcmp(&saved[j], shadow_test_matrix(0, j), sizeof(Mtx)), "Queued frame changed");
                }
                FrameInterpolation_BeginRenderPass(prev, curr, should);
                int reflect = reflectionMode == 1 || reflectionMode == 3;
                for (float step : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f }) {
                    auto replacements = FrameInterpolation_Interpolate(step);
                    for (int j = 0; j < 4; ++j) {
                        int id = 3 - j;
                        for (int mirror = 0; mirror <= reflect; ++mirror) {
                            Mtx* draw = shadow_test_matrix(1, j * (1 + reflect) + mirror);
                            auto entry = replacements.find(draw);
                            if (id == 3) {
                                require(entry == replacements.end(), "Non-shadow interpolation changed");
                                continue;
                            }
                            require(entry != replacements.end(), "Shadow's emitted matrix is missing interpolation");
                            // A newly appearing reflection has no history and must use its
                            // own current position; the existing primary must still blend.
                            float phase = mirror && reflectionMode == 3 ? 1.0f : step;
                            require(std::fabs(entry->second.mf[3][0] - (id * 100 + phase * 10)) < 0.001f,
                                    "Shadow position paired with the wrong object or frame");
                            float z = (20 + phase * 4) * (mirror ? -1 : 1);
                            require(std::fabs(entry->second.mf[3][2] - z) < 0.001f, "Reflected shadow position is wrong");
                        }
                    }
                }
            }
        }
    }
    std::puts("PASS: emitted shadow matrices, sorting, reflections, queued-frame stability and unchanged non-shadow behavior");
    if (argc > 1 && !std::strcmp(argv[1], "--draw-matrices")) return 0;
    // A switch and its shadow occupy the same world plane at each update.
    // Above 30 FPS both must follow the same interpolated plane between updates.
    // Exercise small movements too: a tiny mismatch is enough for depth rejection.
    for (float movement : { 0.0f, -14.0f, -0.0005f, 0.0005f, 14.0f }) {
        for (int path = 0; path < 3; ++path) {
            int prev, curr;
            bool should;
            Mtx* support = nullptr;
            for (int frame = 0; frame < 2; ++frame) {
                FrameInterpolation_StartRecord();
                shadow_test_begin(frame);
                float y = 12.0005f + frame * movement;
                support = shadow_test_support(frame, y);
                shadow_test_draw_height(0, path, false, true, 0, y, 0);
                shadow_test_end(frame);
                FrameInterpolation_GetRecordingPair(&prev, &curr, &should);
                FrameInterpolation_StopRecord();
            }
            Mtx* draw = shadow_test_matrix(1, 0);
            float nativeShadow[4][4], nativeSupport[4][4];
            guMtxL2F(nativeShadow, draw);
            guMtxL2F(nativeSupport, support);
            require(nativeShadow[3][1] == nativeSupport[3][1], "30 FPS support and shadow differ");
            FrameInterpolation_BeginRenderPass(prev, curr, should);
            for (auto fps : { 60u, 120u }) {
                targetFPS = fps;
                unsigned subdivisions = fps / 30;
                for (unsigned tick = 1; tick < subdivisions; ++tick) {
                    float step = float(tick) / subdivisions;
                    auto replacements = FrameInterpolation_Interpolate(step);
                    auto shadow = replacements.find(draw);
                    auto floor = replacements.find(support);
                    require(shadow != replacements.end(), "Intermediate frame has no shadow matrix");
                    require(floor != replacements.end(), "Intermediate frame has no support matrix");
                    if (std::fabs(shadow->second.mf[3][1] - floor->second.mf[3][1]) >= 0.000001f) {
                        std::fprintf(stderr, "plane mismatch: movement=%g path=%d fps=%u step=%g shadow=%.9g floor=%.9g delta=%.9g\n",
                                     movement, path, fps, step, shadow->second.mf[3][1], floor->second.mf[3][1],
                                     shadow->second.mf[3][1] - floor->second.mf[3][1]);
                    }
                    require(std::fabs(shadow->second.mf[3][1] - floor->second.mf[3][1]) < 0.000001f,
                            "Interpolated shadow leaves its support plane");
                }
            }
        }
    }
#ifdef SHADOW_PRECISION_FIX
    // Preserve all transform components, including fractional slopes/negative
    // translations, and retain snapshots after the next frame reuses addresses.
    Mtx source, copied;
    MtxF samples[2];
    int prev, curr;
    bool should;
    for (int frame = 0; frame < 2; ++frame) {
        FrameInterpolation_StartRecord();
        FrameInterpolation_RecordOpenChild("precision-copy-test", 0);
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                samples[frame].mf[i][j] = (i == j ? 1.0f : 0.0f) + (i * 4 + j - 8) * 0.0005f * (frame + 1);
        guMtxF2L(samples[frame].mf, &source);
        FrameInterpolation_CopyMatrix(&source, &copied);
        require(!std::memcmp(&source, &copied, sizeof(Mtx)), "Native packed matrix changed");
        FrameInterpolation_RecordCloseChild();
        FrameInterpolation_GetRecordingPair(&prev, &curr, &should);
        FrameInterpolation_StopRecord();
    }
    FrameInterpolation_BeginRenderPass(prev, curr, should);
    std::memset(&source, 0, sizeof(source));
    std::memset(&copied, 0, sizeof(copied));
    auto replacements = FrameInterpolation_Interpolate(0.5f);
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            require(std::fabs(replacements.at(&copied).mf[i][j] -
                         (samples[0].mf[i][j] + samples[1].mf[i][j]) * 0.5f) < 0.000001f,
                    "Full precision or queued sample ownership lost");
    FrameInterpolation_StartRecord();
    guMtxF2L_NoInterp(samples[0].mf, &source);
    MtxF fallback;
    FrameInterpolation_ReadMatrix(&source, &fallback);
    float packed[4][4];
    guMtxL2F(packed, &source);
    require(!std::memcmp(packed, fallback.mf, sizeof(packed)), "Stale float sample survived frame reset");
    FrameInterpolation_StopRecord();
    FrameInterpolation_CopyMatrix(&source, &copied);
    require(!std::memcmp(&source, &copied, sizeof(Mtx)), "Copy changed bytes outside recording");
#endif
    std::puts("PASS: shadow draw matrices interpolate at 60/120 FPS; sorting, reflections, queued frames and non-shadow behavior preserved; shadow/support planes agree; fractional/sloped precision and cache reset verified");
}

