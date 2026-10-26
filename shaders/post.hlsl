#include "common.hlsli"
cbuffer OutputData:register(b1){float outputHdr,whiteScale,peakRatio,outputReserved;};
Texture2D<float4> scene:register(t0);Texture2D<float4> bloom:register(t1);
SamplerState linearClamp:register(s0);
// Orbit opaque HDR alpha stores normalized near-field blur radius.
float nearBlurRadius(float2 uv){return scene.SampleLevel(linearClamp,uv,0).a*(10*resolution.y/1080);}
float3 orbitFocus(float2 uv,float3 center){
 float centerRadius=nearBlurRadius(uv),radius=centerRadius;
 float maximum=10*resolution.y/1080;
 [unroll]for(int axis=0;axis<4;axis++){
  float2 offset=axis==0?float2(1,0):axis==1?float2(-1,0):axis==2?float2(0,1):float2(0,-1);
  radius=max(radius,nearBlurRadius(uv+offset*maximum/resolution));
 }
 [branch]if(radius<.5)return center;
 float3 result=center;
 [unroll]for(int i=0;i<24;i++){
  float angle=i*2.39996323;float r=sqrt((i+.5)/24.0)*radius;
  float2 sampleUV=uv+float2(cos(angle),sin(angle))*r/resolution;
  float4 sample=scene.SampleLevel(linearClamp,sampleUV,0);
  float sampleRadius=sample.a*(10*resolution.y/1080);
  float coverage=saturate(max(centerRadius,sampleRadius)-r+.5);
  result+=lerp(center,sample.rgb,coverage);
 }
 return result/25;
}
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET {
 float3 c=scene.SampleLevel(linearClamp,uv,0).rgb;
 [branch]if(pass>.5)c=orbitFocus(uv,c);
 c+=bloom.SampleLevel(linearClamp,uv,0).rgb*.28;
 if(pass>.5){
  float2 offset=(uv-orbitSunUV())*float2(resolution.x/resolution.y,1);
  float radius=length(offset);
  float halo=.00085/(radius*radius+.0008);
  float core=exp(-radius*radius*11000)*16;
  // A restrained six-ray aperture pattern, fixed to the light source.
  // Filter the narrow ray width against the pixel footprint for stable motion.
  float rayWidth=max(.00065, .55/resolution.y);
  float streak=0;
  [unroll]for(int axis=0;axis<3;axis++){
   float angle=axis*1.0471975512;
   float along=dot(offset,float2(cos(angle),sin(angle)));
   float across=dot(offset,float2(-sin(angle),cos(angle)));
   streak+=exp(-across*across/(rayWidth*rayWidth))*exp(-abs(along)*24)*.28;
  }
  c+=float3(1,.57,.23)*(halo+core+streak)*brightness;
 }
 c=max(c,0);
 if(outputHdr>.5)c=float3(isfinite(c.r)?min(c.r,65504):0,isfinite(c.g)?min(c.g,65504):0,isfinite(c.b)?min(c.b,65504):0);
 // Blend a shared RGB gain into the shoulder to retain saturated light while
 // allowing the hottest intersections to approach white. Keep the old toe.
 float peak=max(c.r,max(c.g,c.b));
 float3 mapped=lerp(1-exp(-c),c*((1-exp(-peak))/max(peak,.00001)),.38);
 if(pass>.5){
  float3 exposed=max(c,0)*.85;
  mapped=(exposed*(2.51*exposed+.03))/(exposed*(2.43*exposed+.59)+.14);
 }
 if(outputHdr>.5){
  float H=max(peakRatio-1,0),e=max(peak-1,0);
  float h=H>0?H*(1-exp(-(e*e)/(H*(1+e)))):0;
  float3 result=(saturate(mapped)+h*c/max(peak,1e-5))*whiteScale;
  return float4(min(max(result,0),whiteScale*peakRatio),1);
 }
 float3 encoded=pow(saturate(mapped),1/2.2);
 // Static sub-code-value dither prevents banding without temporal grain.
 float dither=(frac(52.9829189*frac(dot(pixel.xy,float2(.06711056,.00583715))))-.5)/255;
 return float4(saturate(encoded+dither),1);
}

