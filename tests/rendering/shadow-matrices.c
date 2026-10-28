// Synthetic models only. Compile the production entity draw path without a GPU.
#include "common.h"
#include "entity.h"
#include "port/Engine.h"
#include <stdlib.h>
#include <string.h>

Gfx* gMainGfxPos;
DisplayContext* gDisplayContext;
u16 gMatrixListPos;
s16 gCurrentCamID;
Camera gCameras[4];
static GameStatus status;
GameStatus* gGameStatusPtr = &status;
static DisplayContext contexts[2];
static EntityModel models[4];
static EntityModelList modelList;
extern EntityModelList* gCurrentEntityModelList;
extern void appendGfx_entity_model(EntityModel* model);
static RenderTask queued;
static int matrixCounts[2];

// The test never executes model artwork or image effects. Keep accidental use fatal.
void gSPDisplayListOTR(Gfx* pkt, const void* dl) { __gSPDisplayList(pkt, dl); }
s32 imgfx_appendGfx_component(s32 a, ImgFXTexture* b, u32 c, Matrix4f d) { abort(); }
void transform_point(Matrix4f m, f32 a, f32 b, f32 c, f32 d, f32* x, f32* y, f32* z, f32* w) {
    *x = a; *y = b; *z = c; *w = d;
}
RenderTask* queue_render_task(RenderTask* task) { memcpy(&queued, task, sizeof(queued)); return &queued; }
#define EMPTY_DL(name) Gfx name[] = { gsSPEndDisplayList() }
EMPTY_DL(Gfx_RM1_ALPHATEST); EMPTY_DL(Gfx_RM1_ALPHATEST_NO_ZB); EMPTY_DL(Gfx_RM1_ALPHATEST_ONESIDED);
EMPTY_DL(Gfx_RM1_CLOUD_NO_ZCMP); EMPTY_DL(Gfx_RM1_DECAL_OPA); EMPTY_DL(Gfx_RM1_DECAL_XLU);
EMPTY_DL(Gfx_RM1_DECAL_XLU_NO_AA); EMPTY_DL(Gfx_RM1_INTERSECTING_OPA); EMPTY_DL(Gfx_RM1_INTERSECTING_XLU);
EMPTY_DL(Gfx_RM1_PASS_THROUGH); EMPTY_DL(Gfx_RM1_SURFACE_OPA); EMPTY_DL(Gfx_RM1_SURFACE_OPA_NO_ZB);
EMPTY_DL(Gfx_RM1_SURFACE_XLU); EMPTY_DL(Gfx_RM1_SURFACE_XLU_NO_AA); EMPTY_DL(Gfx_RM1_SURFACE_XLU_NO_ZB);
EMPTY_DL(Gfx_RM3_ALPHATEST); EMPTY_DL(Gfx_RM3_DECAL_OPA); EMPTY_DL(Gfx_RM3_DECAL_XLU);
EMPTY_DL(Gfx_RM3_INTERSECTING_OPA); EMPTY_DL(Gfx_RM3_INTERSECTING_XLU);
EMPTY_DL(Gfx_RM3_SURFACE_OPA); EMPTY_DL(Gfx_RM3_SURFACE_XLU);
static Gfx emptyModel[] = { gsSPEndDisplayList() };

void shadow_test_begin(int slot) {
    gDisplayContext = &contexts[slot];
    gMainGfxPos = gDisplayContext->mainGfx;
    gMatrixListPos = 0;
    gCurrentCamID = CAM_DEFAULT;
    status.context = CONTEXT_WORLD;
    gCurrentEntityModelList = &modelList;
}
void shadow_test_draw(int id, int path, int reflect, int shadow, float x, float z) {
    Matrix4f mf;
    EntityModel* model = &models[id];
    memset(model, 0, sizeof(*model));
    modelList[id] = model;
    model->flags = ENTITY_MODEL_FLAG_CAM0 | ENTITY_MODEL_FLAG_CAM3 | ENTITY_MODEL_FLAG_ENABLED |
                   ENTITY_MODEL_FLAG_10000 | (reflect ? ENTITY_MODEL_FLAG_REFLECT : 0);
    model->renderMode = shadow ? RENDER_MODE_SHADOW : RENDER_MODE_SURFACE_OPA;
    model->gfx.displayList = emptyModel;
    guTranslateF(mf, x, 0, z);
    guMtxF2L_NoInterp(mf, &model->transform);
    if (path == 0) {
        draw_entity_model_A(id, &model->transform);
    } else if (path == 1) {
        draw_entity_model_B(id, &model->transform, 0, NULL);
    }
    // Match execute_render_tasks' identity even when drawing directly via E.
    FrameInterpolation_RecordOpenChild((const void*)appendGfx_entity_model, (uintptr_t)model);
    if (path == 2) {
        draw_entity_model_E(id, &model->transform);
    } else {
        queued.appendGfx(queued.appendGfxArg);
    }
    FrameInterpolation_RecordCloseChild();
}
void shadow_test_end(int slot) {
    matrixCounts[slot] = gMatrixListPos;
    gSPEndDisplayList(gMainGfxPos++);
}
int shadow_test_count(int slot) { return matrixCounts[slot]; }
Mtx* shadow_test_matrix(int slot, int index) {
    for (Gfx* g = contexts[slot].mainGfx; g->words.w0 >> 24 != G_ENDDL; ++g) {
        if (g->words.w0 >> 24 == G_MTX && index-- == 0) return (Mtx*)g->words.w1;
    }
    abort();
}
