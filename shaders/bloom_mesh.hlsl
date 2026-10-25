#include "bloom_surface.hlsli"
BloomVertex VS(uint id:SV_VertexID){
 const uint columns=512,rows=192;
 uint cell=id/6,corner=id%6;
 float2 offsets[6]={float2(0,0),float2(1,0),float2(0,1),float2(0,1),float2(1,0),float2(1,1)};
 float2 uv=(float2(cell%columns,cell/columns)+offsets[corner])/float2(columns,rows);
 float3 p=bloomSurface(uv);
 float z=3.7-p.z;
 BloomVertex result;
 result.position=float4((p.x+.36)*2.35/(resolution.x/resolution.y),p.y*2.35,z*1.001-.1001,z);
 result.normal=normalize(cross(bloomSurface(uv+float2(.0001,0))-p,bloomSurface(uv+float2(0,.0001))-p));
 result.world=p;result.uv=uv;return result;
}
