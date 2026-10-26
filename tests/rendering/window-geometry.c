// Exercise production draw_box/gu with queued display lists, without a renderer
// or imported assets. Texture middleware only emits commands for this check.
#include "common.h"
#include "port/interpolation/FrameInterpolation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Gfx* gMainGfxPos;
DisplayContext* gDisplayContext;
u16 gMatrixListPos;
GameStatus* gGameStatusPtr;

float OTRGetDimensionFromRightEdge(float v) {
    return 320 - v;
}
float OTRGetDimensionFromLeftEdge(float v) {
    return v;
}
void FrameInterpolation_RecordMatrixMtxFToMtx(MtxF* src, Mtx* dst) {
    (void) src;
    (void) dst;
}
void gSPVertexOTR(Gfx* pkt, uintptr_t v, int n, int v0) {
    __gSPVertex(pkt, v, n, v0);
}
void gDPSetTextureImageOTR(Gfx* pkt, int fmt, int siz, int width, uintptr_t img) {
    gSetImage(pkt, G_SETTIMG, fmt, siz, width, img);
}

static DisplayContext contexts[2];
static const int widths[] = { 162, 68, 68, 130, 124, 130, 124, 130, 124, 88, 88, 88 };
static const int heights[] = { 25, 20, 20, 54, 15, 54, 15, 54, 15, 16, 16, 16 };
typedef struct Reference {
    Vtx* ptr;
    Vtx copy[4];
} Reference;
typedef struct Frame {
    Gfx* begin;
    Gfx* end;
    Reference refs[256];
    int count;
} Frame;

static void require(int value, const char* message) {
    if (!value) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

static Frame build_frame(int slot, int panels, int phase) {
    Frame frame = { 0 };
    gDisplayContext = &contexts[slot];
    // Catch reliance on the old BSS buffer's initial zero contents.
    memset(gDisplayContext->mainGfx, 0xA5, sizeof(gDisplayContext->mainGfx));
    gMatrixListPos = 0;
    gMainGfxPos = frame.begin = gDisplayContext->mainGfx;
    for (int i = 0; i < panels; ++i) {
        int style = (i + phase) % ARRAY_COUNT(widths);
        draw_box(
            DRAW_FLAG_ROTSCALE | DRAW_FLAG_CULL_BACK, (void*) 0, 10 + i, 30 + i, 0, widths[style], heights[style], 255,
            0, 1, 1, 0, 10 + 5 * phase, 0, NULL, NULL, NULL, 320, 240, NULL
        );
    }
    gSPEndDisplayList(gMainGfxPos++);
    frame.end = gMainGfxPos;
    require(frame.end <= frame.begin + ARRAY_COUNT(gDisplayContext->mainGfx), "Display list capacity exceeded");

    // Follow command flow so embedded vertex bytes are never read as commands.
    for (Gfx* g = frame.begin; g < frame.end;) {
        unsigned opcode = g->words.w0 >> 24;
        if (opcode == G_DL) {
            Gfx* next = (Gfx*) g->words.w1;
            require(((g->words.w0 >> 16) & 0xFF) == G_DL_NOPUSH, "Unexpected display-list call");
            require(next > g && next < frame.end, "Vertex data branch leaves this display list");
            g = next;
            continue;
        }
        if (opcode == G_VTX) {
            require(frame.count < ARRAY_COUNT(frame.refs), "Too many vertex loads");
            int panel = frame.count / 4;
            int corner = frame.count % 4;
            int style = (panel + phase) % ARRAY_COUNT(widths);
            int w = widths[style], h = heights[style];
            Reference* ref = &frame.refs[frame.count++];
            ref->ptr = (Vtx*) g->words.w1;
            require(
                ref->ptr == frame.refs[panel * 4].ptr + corner * 4,
                "Corner loads escape their contiguous 16-vertex allocation"
            );
            memcpy(ref->copy, ref->ptr, sizeof(ref->copy));
            for (int k = 0; k < 4; ++k) {
                int right = k % 2, bottom = k / 2;
                int x0 = corner % 2 ? 0 : -w / 2;
                int y0 = corner / 2 ? 0 : -h / 2;
                int x1 = corner % 2 ? w - w / 2 : 0;
                int y1 = corner / 2 ? h / 2 : 0;
                require(
                    ref->copy[k].v.ob[0] == (right ? x1 : x0) && ref->copy[k].v.ob[1] == (bottom ? y1 : y0)
                        && ref->copy[k].v.ob[2] == 0,
                    "Corner vertices were read from the wrong part of the allocation"
                );
                require(
                    ref->copy[k].v.tc[0] == (corner % 2 ? (right ? w : w / 2) : (right ? w / 2 : 0)) * 32
                        && ref->copy[k].v.tc[1] == (corner / 2 ? (bottom ? h : h / 2) : (bottom ? h / 2 : 0)) * 32,
                    "Corner texture coordinates changed"
                );
                require(ref->copy[k].v.flag == 0, "Uninitialized vertex flags");
                for (int c = 0; c < 4; ++c)
                    require(ref->copy[k].v.cn[c] == 0, "Uninitialized vertex color/normal");
            }
        }
        ++g;
    }
    require(frame.count == panels * 4, "Unexpected panel geometry command count");
    return frame;
}

static void check_stable(const Frame* frame) {
    for (int i = 0; i < frame->count; ++i) {
        const Reference* ref = &frame->refs[i];
        require(
            memcmp(ref->copy, ref->ptr, sizeof(ref->copy)) == 0,
            "A newer queued frame overwrote an earlier frame's vertices"
        );
        require(
            (uintptr_t) ref->ptr >= (uintptr_t) frame->begin
                && (uintptr_t) ref->ptr + sizeof(ref->copy) <= (uintptr_t) frame->end,
            "Panel geometry is not owned by its display list"
        );
        for (int j = 0; j < i; ++j)
            require(frame->refs[j].ptr != ref->ptr, "Panels reuse a live vertex allocation");
    }
}

int main(void) {
    // A file-menu-sized pair first, then enough panels to wrap the old ring
    // within a single frame. Reuse a context only after its frame is retired.
    Frame first = build_frame(0, 12, 0);
    Frame second = build_frame(1, 12, 1);
    check_stable(&first);
    check_stable(&second);
    Frame busy = build_frame(0, 64, 2);
    check_stable(&second);
    check_stable(&busy);
    build_frame(1, 64, 3);
    check_stable(&busy);
    puts("PASS: queued rotating-window geometry survives later frames, context reuse and 64-panel pressure");
    return 0;
}
