#include "common.hlsli"
#include "smoke_lighting.hlsli"
Texture3D<float4> velocity:register(t0);
StructuredBuffer<float4> tracers:register(t3);
Texture3D<float4> smoke:register(t1);
SamplerState linearClamp:register(s0);
float2 projectFilament(float3 p){return 4.0*(p.xy-float2(.5,.5))/(p.z+1.5);}
float3 trailPosition(uint instance,float knot){
 int index=(int)floor(knot);float t=frac(knot);uint base=8192+instance*32;
 float3 a=tracers[base+clamp(index-1,0,31)].xyz,b=tracers[base+clamp(index,0,31)].xyz;
 float3 c=tracers[base+clamp(index+1,0,31)].xyz,d=tracers[base+clamp(index+2,0,31)].xyz;
 return clamp(.5*((2*b)+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t*t*t),min(b,c),max(b,c));
}
struct FilamentVertex {float4 position:SV_POSITION;float2 uv:TEXCOORD0;float energy:TEXCOORD1;};
FilamentVertex VS(uint vertex:SV_VertexID,uint instance:SV_InstanceID){
 uint corner=vertex%6,segment=vertex/6;
 bool trail=pass>1.5;
 float3 key=float3(instance,79,13);
 float4 state=tracers[instance*2],previous=tracers[instance*2+1];
 float3 a=trail?trailPosition(instance,segment*.5):previous.xyz;
 float3 b=trail?trailPosition(instance,(segment+1)*.5):state.xyz;
 float2 pa=projectFilament(a),pb=projectFilament(b);
 float2 delta=(pb-pa)*resolution;
 float2 along=length(delta)>.001?normalize(delta):float2(0,1);
 float2 perpendicular=float2(-along.y,along.x);
 float width=trail?2.0:.65+.6*hash(key+23);
 bool end=corner==2||corner==3||corner==5;
 float side=corner==0||corner==2||corner==3?-1:1;
 float3 trailPoint=end?b:a;
 float2 position=(end?pb:pa)+(perpendicular*side*width+along*(end?1:-1)*(trail?0:width))/resolution;
 float3 eye=float3(.5,.5,-1.5);
 float3 start=lerp(eye,trailPoint,1.5/(trailPoint.z+1.5));
 uint samples=(uint)clamp(ceil(length(trailPoint-start)*spare0.z*2),1,256);
 float3 stride=(trailPoint-start)/samples;
 float optical=0;
 [loop]for(uint j=0;j<samples&&optical<8;j++){
  float3 q=start+stride*(j+.5);
  optical+=max(0,smoke.SampleLevel(linearClamp,q,0).w-SmokeDensityCutoff)*smokeEnvelope(q)*length(stride)*density*SmokeExtinction;
 }
 // A strand belongs to the plume: fade it out in empty space and dense cores.
 float4 local=smoke.SampleLevel(linearClamp,trailPoint,0);
 float support=smoothstep(SmokeDensityCutoff,.045,local.w)*saturate(local.x/max(local.w,.0001));
 float phase=state.w/max(previous.w,1);
 float envelope=smoothstep(0,.10,phase)*(1-smoothstep(.65,1,phase));
 if(trail){float progress=(segment+(end?1:0))/62.0;envelope*=sin(progress*3.14159265)*.35;}
 float valid=state.w>=0&&all(trailPoint>0)&&all(trailPoint<1)?1:0;
 FilamentVertex output;output.position=float4(position,0,1);output.uv=float2(side,phase);
 output.energy=valid*envelope*support*exp(-optical)*(.35+.65*hash(key+59));return output;
}
float4 PS(FilamentVertex input):SV_TARGET{
 float edge=exp(-input.uv.x*input.uv.x*3);
 if(pass>1.5){
  // Integrate the subpixel strip over the pixel footprint. Wider support
  // geometry prevents raster coverage holes while preserving a thin core.
  float halfPixel=max(fwidth(input.uv.x)*.5,.0001);
  edge=saturate((min(input.uv.x+halfPixel,.275)-max(input.uv.x-halfPixel,-.275))/(2*halfPixel));
 }
 return float4(tone(float3(5,2,.3)*input.energy*edge),0);
}
