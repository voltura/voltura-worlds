#include "common.hlsli"
#include "mars_common.hlsli"
Texture2D<float4> galaxy:register(t0);
SamplerState skySampler:register(s1);
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET{
 float2 p=(uv-.5)*resolution/min(resolution.x,resolution.y);
 float2 skyUV=float2(.48+.14*p.x,.52+.14*p.y);
 float3 color=galaxy.Sample(skySampler,skyUV).rgb*.0035;
 // Sparse, stationary stars with pixel-footprint filtering; no twinkle noise.
 float2 cell=floor(p*170),local=frac(p*170)-.5;
 float h=hash(float3(cell,41));
 float2 offset=float2(hash(float3(cell,42)),hash(float3(cell,43)))-.5;
 float width=.025,filtered=sqrt(width*width+pow(170/min(resolution.x,resolution.y),2)*.35);
 float star=exp(-dot(local-offset*.7,local-offset*.7)/(filtered*filtered))*width*width/(filtered*filtered);
 color+=star*smoothstep(.994,1,h)*lerp(float3(.47,.60,.80),float3(.85,.72,.58),hash(float3(cell,44)))*.28;
 return float4(tone(marsGrade(color)),0);
}
