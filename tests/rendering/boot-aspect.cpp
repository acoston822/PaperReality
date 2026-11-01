#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <initializer_list>
struct Interpreter { struct { uint32_t width{},height{}; float aspect_ratio{}; } mCurDimensions; } instance;
static Interpreter* active;
Interpreter* GameEngine_GetInterpreter() { return active; }
constexpr float WS_NATIVE_WIDTH=320,WS_NATIVE_HEIGHT=240;
#include "boot-aspect-production.inc"
void require(bool ok, const char* message) { if(!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); } }
int main() {
 require(GameEngine_GetAspectRatio()==320.0f/240,"Missing interpreter fallback");
 active=&instance;
 require(std::isfinite(GameEngine_GetAspectRatio()) && GameEngine_GetAspectRatio()>0,"Zero-initialized interpreter must supply a usable aspect");
 instance.mCurDimensions={1920,886,0};
 require(std::fabs(GameEngine_GetAspectRatio()-1920.0f/886)<1e-6f,"Use configured target before cached aspect exists");
 for(float invalid : {0.0f,-1.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
  instance.mCurDimensions.aspect_ratio=invalid;
  require(std::fabs(GameEngine_GetAspectRatio()-1920.0f/886)<1e-6f,"Reject invalid cached aspects");
 }
 for(float valid : {4.0f/3,16.0f/9,1920.0f/886}) {
  instance.mCurDimensions.aspect_ratio=valid;
  require(GameEngine_GetAspectRatio()==valid,"Preserve initialized aspect");
 }
 instance.mCurDimensions={0,886,0};
 require(GameEngine_GetAspectRatio()==320.0f/240,"Unusable dimensions fall back to native aspect");
 std::puts("PASS: production aspect getter handles first-frame dimensions and invalid cache; valid aspect unchanged");
}
