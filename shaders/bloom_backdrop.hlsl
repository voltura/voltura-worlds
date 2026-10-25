#include "common.hlsli"
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET{
 float2 p=(uv-float2(.60,.49))*float2(resolution.x/resolution.y,1);
 float glow=exp(-dot(p,p)*2.7);
 float3 tint=palette<.5?float3(.006,.022,.064):palette<1.5?float3(.005,.038,.030):palette<2.5?float3(.045,.011,.008):float3(.021,.028,.043);
 return float4(tone(float3(.0003,.0006,.0015)+tint*glow*.28),1);
}
