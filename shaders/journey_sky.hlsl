#include "common.hlsli"
#include "planet_sky.hlsli"
Texture2D<float4> galaxy:register(t0);
SamplerState globeSampler:register(s1);
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET{
 float2 p=(uv-.5)*resolution/min(resolution.x,resolution.y);
 float3 forward=normalize(spare0.xyz),right=normalize(cross(forward,float3(0,1,0))),up=cross(right,forward);
 float3 ray=normalize(forward+right*p.x*.942809-up*p.y*.942809);
 float2 coord=float2(.48+atan2(ray.x,ray.z)/6.283185,.52-asin(clamp(ray.y,-1,1))/3.141593);
 return float4(tone(planetSky(galaxy.Sample(globeSampler,coord),.0035)),1);
}
