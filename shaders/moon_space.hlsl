#include "common.hlsli"
#include "moon_common.hlsli"
#include "planet_sky.hlsli"
Texture2D<float4> galaxy:register(t0);
SamplerState skySampler:register(s1);
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET{
 float3 origin,ray;moonCamera(uv,origin,ray);
 float2 coord=moonUV(ray);
 // NASA's galactic sky longitude increases to the left. Unlike a surface
 // map viewed from outside, the sky is observed from inside its sphere.
 coord.x=1-coord.x;
 float3 color=planetSky(galaxy.SampleGrad(skySampler,coord,moonGradient(ddx(coord)),moonGradient(ddy(coord))),.008);
 return float4(tone(moonGrade(color)),1);
}
