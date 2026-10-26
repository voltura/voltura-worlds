#include "common.hlsli"
#include "uranus_common.hlsli"
#include "planet_sky.hlsli"
Texture2D<float4> galaxy:register(t0);
SamplerState skySampler:register(s1);
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET{
 if(spare1.x>1.5)return float4(0,0,0,1);
 float2 p=(uv-.5)*resolution/min(resolution.x,resolution.y);
 return float4(tone(planetSky(galaxy.Sample(skySampler,planetSkyUV(p)),.0035)),1);
}
