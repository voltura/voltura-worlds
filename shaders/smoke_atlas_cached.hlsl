// Store predictions once in FP32; keep correction/source math in one shader.
#define SMOKE_CACHED_FORWARD 1
#include "smoke_atlas_advect.hlsl"
