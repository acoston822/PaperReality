#pragma once

#if defined(PAPERREALITY_LEAF_DIAGNOSTICS) && PAPERREALITY_LEAF_DIAGNOSTICS
#ifdef __cplusplus
extern "C" {
#endif
void LeafTrace_BeginFrame(void);
void LeafTrace_EndFrame(void);
void LeafTrace_Queued(const void* model, int distance);
void LeafTrace_Culled(const void* model);
void LeafTrace_Draw(const void* model, int renderClass, int renderModeIndex, const void* material);
void LeafTrace_Task(const void* function, const void* argument, int priority);
int LeafTrace_BeginLeafDraw(const void* model);
void LeafTrace_EndLeafDraw(int pushed);
#ifdef __cplusplus
}
#include <string>
namespace Paperboat::Diagnostics {
void StartLeafTrace();
void StopLeafTrace();
bool HasLeafTraceFrames();
std::string LeafTraceReport();
std::string LeafTraceStatus();
void SetLeafTraceMap(const char* name);
// -1 disables the experiment; zero is an identity-matrix control.
bool SetLeafTestLift(float amount);
float LeafTestLift();
}
#endif
#else
#define LeafTrace_BeginFrame()                              ((void) 0)
#define LeafTrace_EndFrame()                                ((void) 0)
#define LeafTrace_Queued(model, distance)                   ((void) 0)
#define LeafTrace_Culled(model)                             ((void) 0)
#define LeafTrace_Draw(model, renderClass, index, material) ((void) 0)
#define LeafTrace_Task(function, argument, priority)        ((void) 0)
#define LeafTrace_BeginLeafDraw(model)                      0
#define LeafTrace_EndLeafDraw(pushed)                       ((void) 0)
#endif
