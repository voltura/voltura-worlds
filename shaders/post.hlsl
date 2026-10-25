#include "common.hlsli"
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
 c+=bloom.SampleLevel(linearClamp,uv,0).rgb*.35;
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
  c+=float3(1,.57,.23)*(halo+core+streak);
 }
 float3 mapped=1-exp(-max(c,0));
 if(pass>.5){
  float3 exposed=max(c,0)*.85;
  mapped=(exposed*(2.51*exposed+.03))/(exposed*(2.43*exposed+.59)+.14);
 }
 return float4(pow(saturate(mapped),1/2.2),1);
}

