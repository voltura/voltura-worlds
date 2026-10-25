#include "smoke_atlas.hlsli"
// Readback probe for the upcoming native sampler contract. Not a scene shader.
cbuffer AtlasProbe:register(b0){uint fineGrid;uint interior;uint atlasAxis;uint count;}
Texture3D<float4> coarse:register(t0);
Texture3D<float4> atlas:register(t1);
StructuredBuffer<SmokeFinePage> pages:register(t2);
StructuredBuffer<float4> positions:register(t3);
SamplerState linearClamp:register(s0);
RWStructuredBuffer<float4> output:register(u0);
[numthreads(64,1,1)]
void CS(uint3 id:SV_DispatchThreadID){
 if(id.x<count){
  output[id.x]=smokeSampleAtlas(positions[id.x].xyz,fineGrid,interior,
                            atlasAxis,coarse,atlas,pages,linearClamp);
  output[id.x+count]=coarse.SampleLevel(linearClamp,saturate(positions[id.x].xyz),0);
  output[id.x+count*2]=smokeFineValue((int3)floor(positions[id.x].xyz*fineGrid),
                                  fineGrid,interior,atlasAxis,coarse,atlas,pages,linearClamp);
 }
}
