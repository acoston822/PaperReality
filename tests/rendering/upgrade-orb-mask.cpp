#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "port/patches/MotionBlurFlamePatches.cpp"

static u8 syntheticMask[256];
static int mode = 1, scenario, reads, loads;
static const u8* loaded;
Gfx* gMainGfxPos;
extern "C" {
const char* D_E00A29D0[] = { "synthetic-dl0", "synthetic-dl1", "synthetic-dl2" };
MotionBlurFlamePreset D_E00A29DC[] = { {16,16,1,1,8,8}, {16,16,1,1,8,8}, {16,16,1,1,8,8} };
s32 D_E00A2A24[] = {100,60,30,10,0,0,0};
int32_t CVarGetInteger(const char*, int32_t) { return mode; }
void GameEngine_LogInfo(const char*, ...) {}
uint16_t GameEngine_GetTexWidthExact(const char*) { return scenario == 3 ? 32 : 16; }
uint16_t GameEngine_GetTexHeightExact(const char*) { return 16; }
size_t GameEngine_GetSizeExact(const char*) { return scenario == 2 ? 128 : 256; }
void* GameEngine_GetDataExact(const char*) { ++reads; return scenario == 1 ? nullptr : syntheticMask; }
void gSPDisplayListOTR(Gfx* p, const void* data) { __gSPDisplayList(p, data); }
void gDPSetTextureImageOTR(Gfx* p, int fmt, int size, int width, uintptr_t img) {
    ++loads; loaded = reinterpret_cast<const u8*>(img);
    // The block-load macro transfers 8-bit data through a 16-bit load tile.
    if (fmt != G_IM_FMT_I || (size != G_IM_SIZ_8b && size != G_IM_SIZ_16b)) std::abort();
    gSetImage(p, G_SETTIMG, fmt, size, width, img);
}
s32 is_point_visible(f32 x, f32 y, f32 z, s32 radius, f32* sx, f32* sy) { *sx=x; *sy=y; return 1; }
}
static void require(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
int main(int argc, char** argv) {
    scenario = argc > 1 ? std::atoi(argv[1]) : 0;
    for (int i=0;i<256;++i) syntheticMask[i]=i;
    static Gfx commands[512];
    EffectInstance effect{};
    MotionBlurFlameFXData data{};
    EffectSharedData shared{};
    effect.shared=&shared;
    effect.data.motionBlurFlame=&data;
    data.unk_4C=127; data.unk_5C=data.unk_60=data.unk_64=255;
    data.unk_1C[0]=160; data.unk_2C[0]=100;
    data.unk_00=scenario==5?1:0;
    if (scenario==4) mode=0;
    gMainGfxPos=commands;
    port_motion_blur_flame_appendGfx(&effect);
    const bool useNative=scenario==0;
    require(loads == (useNative?1:0), "Mode/type/resource validation must gate the native override");
    if (useNative) {
        bool renderI8 = false;
        for (Gfx* p=commands;p<gMainGfxPos;++p) {
            if ((p->words.w0 >> 24)==G_SETTILE && ((p->words.w1 >> 24)&7)==G_TX_RENDERTILE)
                renderI8 = ((p->words.w0 >> 21)&7)==G_IM_FMT_I && ((p->words.w0 >> 19)&3)==G_IM_SIZ_8b;
        }
        require(renderI8,"Native mask must retain I8 render-tile semantics");
        require(loaded != syntheticMask, "Queued commands need stable copied mask storage");
        require(!std::memcmp(loaded,syntheticMask,256), "Native intensity data changed");
        std::memset(syntheticMask,255,sizeof(syntheticMask));
        require(loaded[0]==0 && loaded[128]==128 && loaded[255]==255, "Queued mask changed with source storage");
        const int oldReads=reads;
        gMainGfxPos=commands;
        port_motion_blur_flame_appendGfx(&effect);
        require(reads==oldReads,"Ready mask should not reload every draw");
        // An I8 black edge decodes to alpha zero; opaque RGBA black is alpha one.
        require(loaded[0] == 0, "Native black mask edge must remain transparent");
    }
    puts("PASS: production orb override validates native data, preserves queued storage, and leaves disabled/other/missing cases alone");
}
