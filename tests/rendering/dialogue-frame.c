#include "common.h"
#include "assets/ui.h"
#include "port/Engine.h"
#include <stdio.h>
#include <stdlib.h>

Gfx* gMainGfxPos;
static Gfx commands[2048];
static MessageDrawState drawState;
static MessageDrawState* msg_drawState = &drawState;
static u8 images[2][16][32];
IMG_BIN* gMessageBoxFrameParts[2][16];
s32 gMsgBGScrollAmtX, gMsgBGScrollAmtY;
s32 my_ceil(f32 f) { return (s32)(f + 0.999f); }
uint8_t GameEngine_OTRSigCheck(const char* path) { return 0; }
void* GameEngine_GetDataExact(const char* path) { static u16 palette[256]; return palette; }
void gDPSetTextureImageOTR(Gfx* pkt, int fmt, int siz, int width, uintptr_t img) {
    pkt->words.w0 = (G_SETTIMG << 24) | (fmt << 21) | (siz << 19) | (width - 1);
    pkt->words.w1 = img;
}
#include "dialogue-frame-production.inc"
static void require(int ok, const char* why) {
    if (!ok) { fprintf(stderr, "FAIL: %s\n", why); exit(1); }
}
// A contrasting synthetic edge makes wrapped filtering visible without artwork.
// Sample one quarter texel outside the first row/column of a border piece.
static float sample_join(int mode) {
    const float first = 0.0f, opposite = 1.0f;
    return (mode & G_TX_CLAMP) ? first : 0.75f * first + 0.25f * opposite;
}
int main(void) {
    for (int f = 0; f < 2; ++f)
        for (int i = 0; i < 16; ++i) gMessageBoxFrameParts[f][i] = images[f][i];
    gMainGfxPos = commands;
    msg_draw_frame(40, 36, 240, 40, MSG_STYLE_INSPECT, 0, 0, 255, 255);
    int piece = 0;
    for (Gfx* p = commands; p < gMainGfxPos; ++p) {
        if ((p->words.w0 >> 24) == G_SETTILE && ((p->words.w1 >> 24) & 7) == G_TX_RENDERTILE) {
            // Ignore the background tile; border tiles are CI4, the fill is I4.
            if (((p->words.w0 >> 21) & 7) != G_IM_FMT_CI) continue;
            int cms = (p->words.w1 >> 8) & 3;
            int cmt = (p->words.w1 >> 18) & 3;
            require(cms == ((piece == 2 || piece == 13) ? G_TX_WRAP : G_TX_CLAMP), "Frame wraps across a vertical join");
            require(cmt == ((piece == 7 || piece == 8) ? G_TX_WRAP : G_TX_CLAMP), "Frame wraps across a horizontal join");
            if (piece != 2 && piece != 13)
                require(sample_join(cms) == 0, "Filtering pulls the opposite column into a vertical join");
            if (piece != 7 && piece != 8)
                require(sample_join(cmt) == 0, "Filtering pulls the opposite row into a horizontal join");
            ++piece;
        }
    }
    require(piece == 16, "All 16 frame pieces must retain their draw commands");
    puts("PASS: actual frame tile commands clamp across joins and retain repetition along long edges");
}
