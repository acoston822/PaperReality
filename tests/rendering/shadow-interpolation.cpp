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
}
static void require(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
int main() {
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
    std::puts("PASS: actual shadow draw matrices interpolate at 60/120 FPS through sorting, reflections and queued frames; non-shadow draws unchanged");
}
