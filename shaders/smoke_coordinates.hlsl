#include "common.hlsli"
#include "smoke_transport.hlsli"
#include "smoke_detail_clock.hlsli"
Texture3D<float4> velocityField:register(t0);
Texture3D<float4> coordinates:register(t1);
Texture3D<float4> forwardCoordinates:register(t2);
RWTexture3D<float4> outputCoordinates:register(u1);
SamplerState linearClamp:register(s0);
float3 sampleMap(Texture3D<float4> field,float3 p,uint chart){
 // Repeated hardware-filtered interpolation can accumulate fractional-weight
 // quantization error even in an affine coordinate field. Use float weights.
 float3 cell=p*grid-.5;int3 base=(int3)floor(cell);float3 f=frac(cell),result=0;
 [unroll]for(int z=0;z<2;z++)[unroll]for(int y=0;y<2;y++)[unroll]for(int x=0;x<2;x++){
  int3 at=clamp(base+int3(x,y,z),0,(int)grid-1);at.x+=chart*(int)grid;
  float3 w=lerp(1-f,f,float3(x,y,z));
  result+=field.Load(int4(at,0)).xyz*w.x*w.y*w.z;
 }
 return result;
}
float3 flowAt(float3 p){
 if(spare1.x>.5)return smokeDiagnosticFlow(p);
 float h=.5/grid;
 return float3(velocityField.SampleLevel(linearClamp,p+float3(h,0,0),0).x,
  velocityField.SampleLevel(linearClamp,p+float3(0,h,0),0).y,
  velocityField.SampleLevel(linearClamp,p+float3(0,0,h),0).z);
}
// Two independently advected coordinate charts share one volume. Resetting
// half a period apart allows later detail synthesis to crossfade without pops.
[numthreads(4,4,4)]
void CS(uint3 id:SV_DispatchThreadID){
 if(any(id>=uint3(grid*2,grid,grid)))return;
 uint chart=id.x>=(uint)grid?1:0;
 float3 p=(float3(id)-float3(chart*grid,0,0)+.5)/grid;
 float period=smokeDetailPeriod(),phase=chart*(period*.5);
 // The imported-density transport diagnostic needs a persistent map.
 bool reset=pass<.5||(spare2.y<1.5&&floor((time+phase)/period)!=floor((max(0,time-dt)+phase)/period));
 if(reset){outputCoordinates[id]=float4(p,0);return;}
 float3 midpoint=p-flowAt(p)*dt*.5;
 float3 departure=clamp(p-flowAt(midpoint)*dt,.5/grid,1-.5/grid);
 if(pass>2.5){
  float3 reverseMid=p+flowAt(p)*dt*.5;
  float3 reverse=clamp(p+flowAt(reverseMid)*dt,.5/grid,1-.5/grid);
  float3 forward=forwardCoordinates.Load(int4(id,0)).xyz;
  float3 corrected=forward+.5*(coordinates.Load(int4(id,0)).xyz-sampleMap(forwardCoordinates,reverse,chart));
  int3 base=(int3)floor(departure*grid-.5);float3 lo=1e10,hi=-1e10;
  [unroll]for(int z=0;z<2;z++)[unroll]for(int y=0;y<2;y++)[unroll]for(int x=0;x<2;x++){
   int3 at=clamp(base+int3(x,y,z),0,(int)grid-1);at.x+=chart*(int)grid;
   float3 value=coordinates.Load(int4(at,0)).xyz;lo=min(lo,value);hi=max(hi,value);
  }
  outputCoordinates[id]=float4(clamp(corrected,lo,hi),0);return;
 }
 outputCoordinates[id]=float4(sampleMap(coordinates,departure,chart),0);
}
