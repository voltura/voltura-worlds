#include "smoke_atlas.hlsli"
#include "smoke_lighting.hlsli"
cbuffer Lighting : register(b0){
 uint fineGrid;uint interior;uint atlasAxis;uint slots;
 float delta;uint initialize;uint halfStorage;uint velocityGrid;
 float densityScale;uint correction;float seconds;float flow;
 float audio;uint inject;uint mode;float strength;
}
Texture3D<float4> coarse:register(t0);
Texture3D<float4> atlas:register(t1);
StructuredBuffer<SmokeFinePage> pages:register(t2);
StructuredBuffer<uint> destinations:register(t4);
RWTexture3D<float4> lighting:register(u0);
SamplerState linearClamp:register(s0);
float4 visibleDensity(float3 p){
 float3 lower=atlasAxis>8?float3(.25,.125,.25):float3(.375,.375,.375);
 float3 upper=atlasAxis>8?float3(.75,.875,.5625):float3(.625,.625,.625);
 float3 fade=smoothstep(0,atlasAxis>8?.03125:.0625,min(p-lower,upper-p));
 float4 base=coarse.SampleLevel(linearClamp,p,0);
 float blend=fade.x*fade.y*fade.z;
 if(blend<=0)return base;
 float4 value=lerp(base,smokeSampleAtlas(p,fineGrid,interior,atlasAxis,coarse,atlas,pages,linearClamp),blend);
 value.xyz=max(value.xyz,0);value.w=dot(value.xyz,1);return value;
}
[numthreads(64,1,1)]
void CS(uint3 id:SV_DispatchThreadID){
 uint cells=interior*interior*interior,slot=id.x/cells;
 if(slot>=slots)return;
 uint page=destinations[slot],axis=fineGrid/interior;
 if(page>=axis*axis*axis)return;
 uint localId=id.x%cells;
 uint3 local=uint3(localId%interior,(localId/interior)%interior,localId/(interior*interior));
 uint3 tile=uint3(page%axis,(page/axis)%axis,page/(axis*axis));
 float3 p=(float3(tile*interior+local)+.5)/fineGrid;
 float h=1.0/fineGrid;
 float3 gradient=float3(visibleDensity(p+float3(h,0,0)).w-visibleDensity(p-float3(h,0,0)).w,
                        visibleDensity(p+float3(0,h,0)).w-visibleDensity(p-float3(0,h,0)).w,
                        visibleDensity(p+float3(0,0,h)).w-visibleDensity(p-float3(0,0,h)).w);
 float3 direction=normalize(float3(.6,.6,-.7));
 float3 exits=select(direction>0,1-p,-p)/direction;
 float stop=min(exits.x,min(exits.y,exits.z)),distance=0,stride=.0037,shadow=0;
 [loop]for(int i=0;i<20&&distance<stop;i++){
  float interval=min(stride,stop-distance);float3 q=p+direction*(distance+interval*.5);
  shadow+=max(0,visibleDensity(q).w-SmokeDensityCutoff)*smokeEnvelope(q)*interval;
  distance+=interval;stride*=1.28;
 }
 uint3 physical=uint3(slot%atlasAxis,(slot/atlasAxis)%atlasAxis,slot/(atlasAxis*atlasAxis));
 lighting[physical*(interior+2)+local+1]=float4(smokeLighting(shadow,gradient,densityScale),0);
}
