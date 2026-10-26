#include "common.hlsli"
Texture2D<float4> videoFrame:register(t0);
SamplerState linearClamp:register(s0);
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET {
 float screenAspect=resolution.x/resolution.y;
 float videoAspect=spare0.x/spare0.y;
 float2 ratio=float2(screenAspect/videoAspect,videoAspect/screenAspect);
 float2 scale=spare0.z>.5?max(1,ratio):min(1,ratio);
 float2 source=(uv-.5)*scale+.5;
 if(spare0.z<.5)source+=(float2(spare0.w,spare1.x)-.5)*(1-scale);
 if(any(source<0)||any(source>1))return float4(0,0,0,1);
 // Preserve SDR video colors without procedural HDR tone mapping or bloom.
 return float4(videoFrame.SampleLevel(linearClamp,source,0).rgb,1);
}
