#include "common.hlsli"
#include "earth_common.hlsli"
#include "planet_sky.hlsli"
Texture2D<float4> galaxy:register(t0);
SamplerState skySampler:register(s1);
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET{
 float2 p=(uv-.5)*resolution/min(resolution.x,resolution.y);
 float3 color=planetSky(galaxy.Sample(skySampler,planetSkyUV(p)),.0025);
 return float4(tone(earthGrade(color)),1);
}
