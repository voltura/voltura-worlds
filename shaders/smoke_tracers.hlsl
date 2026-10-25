#include "common.hlsli"
Texture3D<float4> velocity:register(t0);
RWStructuredBuffer<float4> particles:register(u7);
SamplerState linearClamp:register(s0);
float3 flowAt(float3 p){
 float h=.5/grid;
 return float3(velocity.SampleLevel(linearClamp,p+float3(h,0,0),0).x,
 velocity.SampleLevel(linearClamp,p+float3(0,h,0),0).y,
 velocity.SampleLevel(linearClamp,p+float3(0,0,h),0).z);
}
[numthreads(64,1,1)]
void CS(uint3 id:SV_DispatchThreadID){
 uint i=id.x;float3 key=float3(i,113,29);
 float life=5+hash(key+7)*7;
 float4 state=0;
 if(pass>.5)state=particles[i*2];
 bool spawn=pass<.5||state.w>life||any(state.xyz<.02)||any(state.xyz>.98);
 if(spawn){
  float phase=time*.13;
  float3 center=float3(.31+.035*sin(phase),.18+.045*sin(phase*.7),.5+.08*cos(phase));
  float cycle=floor(time/life);
  float3 jitter=float3(hash(key+cycle),hash(key+cycle+17),hash(key+cycle+31))-.5;
  state=float4(center+jitter*.09,pass<.5?-hash(key+51)*life:0);
 }
 float3 previous=state.xyz;
 if(state.w>=0){float stepSeconds=min(dt,.05);float3 v=flowAt(state.xyz);state.xyz+=flowAt(state.xyz+v*stepSeconds*.5)*stepSeconds;}
 state.w+=dt;
 if(i<128){
  uint base=8192+i*32;
  if(spawn){[unroll]for(uint n=0;n<32;n++)particles[base+n]=state;}
  else if(floor(time*15)>floor((time-dt)*15)){
   [loop]for(int n=31;n>0;n--)particles[base+n]=particles[base+n-1];
  }
  particles[base]=state;
 }
 particles[i*2]=state;particles[i*2+1]=float4(previous,life);
}
