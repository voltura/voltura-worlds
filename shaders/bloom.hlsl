#include "common.hlsli"
Texture2D<float4> source:register(t0);SamplerState linearClamp:register(s0);
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET {
 float3 result=0;
 if(pass<.5){
  [unroll]for(int y=0;y<4;y++)[unroll]for(int x=0;x<4;x++){
   float3 c=source.SampleLevel(linearClamp,uv+(float2(x,y)-1.5)/resolution,0).rgb;
   float peak=max(c.r,max(c.g,c.b));
   // A soft shoulder admits highlights continuously as they cross the knee.
   // Keep extraction in RGB so saturated emitters do not acquire white halos.
   float knee=clamp(peak-.25,0,.5);
   float highlight=max(peak-.5,knee*knee);
   result+=c*(highlight/max(peak,.0001))/16;
  }
 }else{
  float2 stepUV=pass<1.5?float2(4/resolution.x,0):float2(0,4/resolution.y);
  float weight=0;
  [unroll]for(int i=-8;i<=8;i++){float w=exp(-float(i*i)/18);result+=source.SampleLevel(linearClamp,uv+stepUV*i,0).rgb*w;weight+=w;}
  result/=weight;
 }
 return float4(result,1);
}
