// Experimental manual trilinear sampler for selective fine-density storage.
// Each corner resolves its owning page independently, so interpolation across
// a tile edge never reads an unrelated physical atlas neighbor. The coarse
// simulation remains defined everywhere, including absent or fading pages.
struct SmokeFinePage { uint slot; float blend; };

// A known fine-cell center needs its absolute value, not a correction sampled
// at eight corners. Fully resident cells can avoid coarse texture reads.
float4 smokeFineValue(int3 cell,uint fineGrid,uint interior,uint atlasAxis,
 Texture3D<float4> coarse,Texture3D<float4> atlas,
 StructuredBuffer<SmokeFinePage> pages,SamplerState linearClamp) {
 cell=clamp(cell,0,(int)fineGrid-1);
 uint3 tile=(uint3)cell/interior;uint axis=fineGrid/interior;
 SmokeFinePage page=pages[tile.x+axis*(tile.y+axis*tile.z)];
 float3 position=(float3(cell)+.5)/fineGrid;
 if(page.slot>=atlasAxis*atlasAxis*atlasAxis || !isfinite(page.blend) || page.blend<=0)
  return coarse.SampleLevel(linearClamp,position,0);
 uint3 physical=uint3(page.slot%atlasAxis,(page.slot/atlasAxis)%atlasAxis,
                      page.slot/(atlasAxis*atlasAxis));
 float4 value=atlas.Load(int4(physical*(interior+2)+(uint3)cell%interior+1,0));
 if(page.blend>=1)return value;
 return lerp(coarse.SampleLevel(linearClamp,position,0),value,page.blend);
}

float4 smokeFineCorner(int3 cell,uint fineGrid,uint interior,uint atlasAxis,
 Texture3D<float4> coarse,Texture3D<float4> atlas,
 StructuredBuffer<SmokeFinePage> pages,SamplerState linearClamp) {
 cell=clamp(cell,0,(int)fineGrid-1);
 float3 position=(float3(cell)+.5)/fineGrid;
 float4 fallback=coarse.SampleLevel(linearClamp,position,0);
 uint3 virtualTile=(uint3)cell/interior;
 uint virtualAxis=fineGrid/interior;
 uint pageIndex=virtualTile.x+virtualAxis*(virtualTile.y+virtualAxis*virtualTile.z);
 SmokeFinePage page=pages[pageIndex];
 if(page.slot>=atlasAxis*atlasAxis*atlasAxis || !isfinite(page.blend) || page.blend<=0)
  return 0;
 uint3 physicalTile=uint3(page.slot%atlasAxis,(page.slot/atlasAxis)%atlasAxis,
                         page.slot/(atlasAxis*atlasAxis));
 uint3 local=(uint3)cell%interior+1;
 float4 refined=atlas.Load(int4(physicalTile*(interior+2)+local,0));
 return (refined-fallback)*saturate(page.blend);
}

float4 smokeSampleAtlas(float3 position,uint fineGrid,uint interior,uint atlasAxis,
 Texture3D<float4> coarse,Texture3D<float4> atlas,
 StructuredBuffer<SmokeFinePage> pages,SamplerState linearClamp) {
 // Caller validates nonzero dimensions, divisibility, table length and finite
 // positions before dispatch. Atlas borders are reserved for a later fast path.
 float3 cell=saturate(position)*fineGrid-.5;
 int3 base=(int3)floor(cell);float3 f=frac(cell);
 // Interpolate a correction, not eight coarse fallbacks. Otherwise even an
 // entirely absent atlas would resample and blur the original coarse field.
 float4 result=coarse.SampleLevel(linearClamp,saturate(position),0);
 [unroll]for(int z=0;z<2;z++)[unroll]for(int y=0;y<2;y++)[unroll]for(int x=0;x<2;x++){
  float weight=(x?f.x:1-f.x)*(y?f.y:1-f.y)*(z?f.z:1-f.z);
  result+=weight*smokeFineCorner(base+int3(x,y,z),fineGrid,interior,atlasAxis,
                             coarse,atlas,pages,linearClamp);
 }
 return result;
}
