#include "common.hlsli"
#include "smoke_lighting.hlsli"
#include "smoke_atlas.hlsli"
Texture3D<float4> fineAtlas:register(t4);
Texture3D<float4> fineLight:register(t3);
StructuredBuffer<SmokeFinePage> finePages:register(t5);
Texture3D<float4> lightVolume:register(t1);
Texture3D<float4> volume:register(t0);SamplerState linearClamp:register(s0);
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET {
 // Capture-only views share the exact frozen density used by the beauty render.
 if(spare0.w>1.5&&spare0.w<2.5){
  float4 slice=volume.SampleLevel(linearClamp,float3(uv.x,1-uv.y,.5),0);
  return float4(tone(slice.www*3),1);
 }
 // Frame the volume across the display, including wide and portrait screens.
 // Screen-relative axes let the smoke fill the wallpaper instead of leaving
 // aspect-dependent side margins around a small, right-shifted sculpture.
 float3 origin=float3(.5,.5,-1.5),ray=normalize(float3((uv.x-.5)*.9,(.5-uv.y)*.9,1.8));
 float3 inv=1/ray;float3 a=(0-origin)*inv,b=(1-origin)*inv;
 float start=max(max(min(a.x,b.x),min(a.y,b.y)),min(a.z,b.z));float stop=min(min(max(a.x,b.x),max(a.y,b.y)),max(a.z,b.z));
 if(stop<=max(start,0))return float4(tone(float3(.00002,.00004,.00009)),1);
 int steps=(int)clamp(ceil((stop-max(start,0))*spare0.z*2),64,768);float stride=(stop-max(start,0))/steps;float transmission=1;float3 col=0;
 float jitter=.5;
 [loop]for(int i=0;i<steps;i++){
  float3 pos=origin+ray*(max(start,0)+(i+jitter)*stride);
  // Capture prototypes use a central box or a larger front region. Include a
  // full coarse brick margin so empty-space jumps cannot cross a fine edge.
  float3 fineMin=spare2.y>12?float3(.25,.125,.25):float3(.375,.375,.375);
  float3 fineMax=spare2.y>12?float3(.75,.875,.5625):float3(.625,.625,.625);
  bool fineRegion=spare2.x>1.5&&all(pos>=fineMin-4.0/spare0.z)&&all(pos<=fineMax+4.0/spare0.z);
  if(spare2.z<.5&&!fineRegion){
   int3 brick=(int3(clamp(pos*spare1.yzw,0,spare1.yzw-1))/4)*4;
   if(lightVolume.Load(int4(brick,0)).w<.5){
    float3 boundary=(float3(brick)+select(ray>0,4.0,0.0))/spare1.yzw;
    float3 exits=(boundary-pos)*inv;
    float distance=max(0,min(exits.x,min(exits.y,exits.z)));
    // Keep the original quadrature positions: skip only complete empty steps.
    i+=max(1,(int)floor(distance/stride))-1;
    continue;
   }
  }
  float3 samplePos=pos;float4 d=volume.SampleLevel(linearClamp,samplePos,0);
  if(fineRegion){
   float4 refined=smokeSampleAtlas(samplePos,(uint)spare2.x,8,(uint)spare2.y,volume,fineAtlas,finePages,linearClamp);
   float3 blend=smoothstep(0,spare2.y>12?.03125:.0625,min(samplePos-fineMin,fineMax-samplePos));
   d=lerp(d,refined,blend.x*blend.y*blend.z);
   d.xyz=max(d.xyz,0);d.w=dot(d.xyz,1);
  }
  float den=max(0,d.w-SmokeDensityCutoff)*density*SmokeExtinction*smokeEnvelope(pos);
  if(den>.001){
   float3 weights=max(d.xyz,0)/max(dot(max(d.xyz,0),1),1e-20);
   float3 mixture=exp(weights.x*log(float3(1,.42,.06))+weights.y*log(max(colorA(),.002))+weights.z*log(max(colorB(),.002)));
   float3 pigment=pow(mixture,1.65);
   float3 lighting=lightVolume.SampleLevel(linearClamp,pos,0).rgb;
   if(fineRegion&&spare2.w>.5){
    float3 blend=smoothstep(0,spare2.y>12?.03125:.0625,min(pos-fineMin,fineMax-pos));
    float3 refinedLight=smokeSampleAtlas(pos,(uint)spare2.x,8,(uint)spare2.y,lightVolume,fineLight,finePages,linearClamp).rgb;
    lighting=lerp(lighting,refinedLight,blend.x*blend.y*blend.z);
   }
   // Warm pigment carries a restrained inner glow. Thin fringes shine more
   // than dense cores, preserving the shadowed folds instead of flattening them.
   float gold=weights.x;
   float fringe=exp(-max(0,d.w-SmokeDensityCutoff)*11);
   // Diluted gold should not wash cooler pigments in orange self-emission.
   // The continuous ramp preserves the original gain for pure gold.
   float hotGold=pow(saturate((gold-.25)/.75),2);
   float3 emission=float3(1,.24,.015)*hotGold*(.03+fringe*.8);
   if(spare0.w>.5&&spare0.w<3.5){lighting=1;emission=0;}
   if(spare0.w>2.5){pigment=1;emission=0;}
   float alpha=1-exp(-den*stride);col+=transmission*alpha*(pigment*lighting+emission);transmission*=1-alpha;
   if(transmission<.008)break;
  }
 }
 return float4(tone(col+float3(.00002,.00004,.00009)),1);
}



