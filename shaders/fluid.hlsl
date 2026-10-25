#include "common.hlsli"
#include "smoke_transport.hlsli"
#include "smoke_lighting.hlsli"
#include "smoke_sources.hlsli"
RWTexture3D<float4> velocityIn:register(u0);
RWTexture3D<float4> velocityOut:register(u1);
RWTexture3D<float4> densityIn:register(u2);
RWTexture3D<float4> densityOut:register(u3);
RWTexture3D<float> pressureIn:register(u4);
RWTexture3D<float> pressureOut:register(u5);
RWTexture3D<float> divergence:register(u6);
RWTexture3D<float4> curl:register(u7);
Texture3D<float4> sampledVelocity:register(t0);
Texture3D<float4> sampledDensity:register(t1);
Texture3D<float4> sampledForward:register(t2);
Texture3D<float4> detailVelocity:register(t3);
SamplerState volumeSampler:register(s0);
#include "smoke_half.hlsli"
int3 bound(int3 p){return clamp(p,0,(int)grid-1);}
float3 densityGrid(){return spare1.yzw;}
int3 boundDensity(int3 p){return clamp(p,0,(int3)densityGrid()-1);}
#ifdef SMOKE_CLOSED_FACES
// Capture multigrid uses N cells and implicit zero normal velocity at face N.
// Face N-1 is an interior face and must remain free to receive pressure correction.
bool closedFaces(){return true;}
float3 vel(int3 p){
 float3 v=sampledVelocity.Load(int4(bound(p),0)).xyz;
 if(closedFaces()){if(p.x<=0||p.x>=(int)grid)v.x=0;if(p.y<=0||p.y>=(int)grid)v.y=0;if(p.z<=0||p.z>=(int)grid)v.z=0;}
 return v;
}
float3 wallFaces(float3 v,int3 p){if(p.x==0)v.x=0;if(p.y==0)v.y=0;if(p.z==0)v.z=0;return v;}
float4 sampleV(float3 p){
 if(!closedFaces())return sampledVelocity.SampleLevel(volumeSampler,(p+.5)/grid,0);
 int3 b=(int3)floor(p);float3 f=frac(p),value=0;
 [unroll]for(int z=0;z<2;z++)[unroll]for(int y=0;y<2;y++)[unroll]for(int x=0;x<2;x++){
  float3 w=lerp(1-f,f,float3(x,y,z));value+=vel(b+int3(x,y,z))*w.x*w.y*w.z;
 }
 return float4(value,0);
}
#else
float3 vel(int3 p){return sampledVelocity.Load(int4(bound(p),0)).xyz;}
float4 sampleV(float3 p){return sampledVelocity.SampleLevel(volumeSampler,(p+.5)/grid,0);}
#endif
// Pressure's forward divergence/backward gradient place each component on
// the negative cell face. Reconstruct the vector at a common spatial point
// before tracing; sampling all three raw channels together offsets the flow.
float3 sampleFlow(float3 p){
 if(spare1.x>.5)return smokeDiagnosticFlow((p+.5)/grid);
 return float3(sampleV(p+float3(.5,0,0)).x,
               sampleV(p+float3(0,.5,0)).y,
               sampleV(p+float3(0,0,.5)).z);
}
float3 centerVelocity(int3 p){
 return .5*(vel(p)+float3(vel(p+int3(1,0,0)).x,vel(p+int3(0,1,0)).y,vel(p+int3(0,0,1)).z));
}
float3 traceFlow(float3 position,float stepSeconds){
 float stepCells=stepSeconds*grid;
 float3 midpoint=position-sampleFlow(position)*(stepCells*.5);
 return position-sampleFlow(midpoint)*stepCells;
}
float3 advectFaces(int3 p){
 float3 result;
 [unroll]for(int axis=0;axis<3;axis++){
  float3 faceOffset=0;faceOffset[axis]=.5;
  float3 departure=traceFlow(float3(p)-faceOffset,dt);
  result[axis]=sampleV(departure+faceOffset)[axis];
 }
 return result;
}
float3 densityFlow(float3 p){
 float3 v=sampleFlow(p);
 if(spare2.x<.5||spare1.x>.5)return v;
 return v+detailVelocity.SampleLevel(volumeSampler,(p+.5)/grid,0).xyz;
}
float3 traceDensity(int3 position,float stepSeconds){
 float3 velocityPosition=(float3(position)+.5)*(grid/densityGrid())-.5;
 float3 midpoint=velocityPosition-densityFlow(velocityPosition)*(stepSeconds*grid*.5);
 return float3(position)-densityFlow(midpoint)*(stepSeconds*densityGrid());
}
float4 sampleD(float3 p){return sampledDensity.SampleLevel(volumeSampler,(p+.5)/densityGrid(),0);}
// Cubic semi-Lagrangian density transport. Clamp interpolation
// to the enclosing eight samples; no artificial extrema are introduced.
float4 densityCubicWeights(float t){
 float t2=t*t,t3=t2*t;
 return float4(-.5*t+t2-.5*t3,1-2.5*t2+1.5*t3,.5*t+2*t2-1.5*t3,-.5*t2+.5*t3);
}
float4 sampleCubicDensity(float3 p){
 int3 base=(int3)floor(p);float3 f=frac(p);
 float4 wx=densityCubicWeights(f.x),wy=densityCubicWeights(f.y),wz=densityCubicWeights(f.z);
 float4 value=0,lo=1e10,hi=-1e10;
 [loop]for(int z=0;z<4;z++){
  float4 plane=0;
  [loop]for(int y=0;y<4;y++){
   float4 row=0;
   [unroll]for(int x=0;x<4;x++){
    float4 v=sampledDensity.Load(int4(boundDensity(base+int3(x,y,z)-1),0));
    row+=v*wx[x];
    if(x>=1&&x<=2&&y>=1&&y<=2&&z>=1&&z<=2){lo=min(lo,v);hi=max(hi,v);}
   }
   plane+=row*wy[y];
  }
  value+=plane*wz[z];
 }
 return clamp(value,lo,hi);
}
float4 sampleForward(float3 p){return sampledForward.SampleLevel(volumeSampler,(p+.5)/densityGrid(),0);}
float emitter(float3 uv,int e){
 return smokeEmitter(uv,e,time,dt,flow,energy*reaction);
}
float3 bodyForce(float3 uv){
 float3 force=0;
 float smoke=sampleD(uv*densityGrid()-.5).w;
 [unroll]for(int e=0;e<3;e++){
  float phase=e*2.1+time*.13,emit=emitter(uv,e);
  smoke+=emit;
  force+=float3((1-float(e))*.24,.22,-cos(phase)*.08)*emit*3;
  float3 offset=uv-float3(.31+e*.22,.32+e*.035,.5+.08*cos(phase));
  float radial=length(offset.xz);
  float ring=exp(-(pow(radial-.075,2)+offset.y*offset.y)/.0025);
  float2 direction=offset.xz/max(radial,.001);
  force+=float3(direction.x*offset.y,-(radial-.075),direction.y*offset.y)*ring*dt*8*turbulence;
 }
 // Broad counter-rotating flow brings neighboring pigments into shared folds.
 [unroll]for(int roll=0;roll<2;roll++){
  float3 center=float3(.55+(roll==0?-.045:.045),.40+roll*.23,.5);
  float3 offset=uv-center;
  // Unequal roll radii avoid two matching lobes and carry existing density
  // into broad folds. The solver, transport stencil and iteration budget stay fixed.
  float strength=exp(-dot(offset,offset)/(roll==0?.032:.050));
  float3 axis=normalize(float3(.2,roll==0?.12:-.12,1));
  force+=cross(axis,offset)*strength*(roll==0?1:-1)*dt*.60*turbulence;
 }
 force.y+=dt*smoke*.12*flow;
 float3 swirl=float3(sin(uv.y*19+time*.31)*cos(uv.z*17),sin(uv.z*15-time*.27)*cos(uv.x*18),sin(uv.x*17+time*.23)*cos(uv.y*16));
 return force+swirl*dt*.02*turbulence*saturate(smoke*4);
}
float3 confinement(int3 p){
 p=bound(p);
 float3 grad=float3(curl[bound(p+int3(1,0,0))].w-curl[bound(p-int3(1,0,0))].w,
  curl[bound(p+int3(0,1,0))].w-curl[bound(p-int3(0,1,0))].w,
  curl[bound(p+int3(0,0,1))].w-curl[bound(p-int3(0,0,1))].w);
 return cross(grad/(length(grad)+.00001),curl[p].xyz)*turbulence*dt*3;
}
groupshared uint occupiedBrick;
[numthreads(4,4,4)]
void CS(uint3 id:SV_DispatchThreadID,uint lane:SV_GroupIndex){
 float3 extent=(pass<.5||pass>6.5)?densityGrid():grid;
 if(any(id>=(uint3)extent))return;int3 p=id;float3 uv=(id+.5)/extent;
 if(pass<.5){ // clear
  if(pass>-.5&&all(id<(uint)grid)){velocityIn[p]=0;velocityOut[p]=0;pressureIn[p]=0;pressureOut[p]=0;divergence[p]=0;}
  float4 initial=0;
  if(spare1.x>.5){
   float3 q=uv-float3(.65,.5,.5);
   float value=.1*exp(-dot(q,q)/.0025)*(1+.3*cos(q.x*80)*cos(q.y*80)*cos(q.z*80));
   initial=float4(value*(.5+.45*sin(q.x*100)),value*(.5+.45*sin(q.y*110)),value*(.5+.45*cos(q.z*90)),value);
  }
  densityIn[p]=storeHalf(initial);densityOut[p]=storeHalf(initial);curl[p]=0;return;
 }
 if(pass>8.5){
  float3 departure=traceDensity(p,dt);
  bool cubic=spare1.x<.5||spare1.x>2.5;
  densityOut[p]=storeHalf(cubic?sampleCubicDensity(departure):sampleD(departure));return;
 }
 if(pass>7.5){
  // Four-cell brick plus a one-cell halo bounds trilinear density samples.
  if(lane==0)occupiedBrick=0;
  GroupMemoryBarrierWithGroupSync();
  int3 brickBase=(p/4)*4;
  uint occupied=0;
  for(uint index=lane;index<216;index+=64){
   int3 offset=int3(index%6,(index/6)%6,index/36)-1;
   occupied|=sampledDensity.Load(int4(boundDensity(brickBase+offset),0)).w>SmokeDensityCutoff*.9999;
  }
  if(occupied)InterlockedOr(occupiedBrick,1);
  GroupMemoryBarrierWithGroupSync();
  // The one-cell halo covers every trilinear density/light footprint touching
  // this brick. No visible coarse density can sample an entirely empty brick.
  // Fine-atlas density can evolve independently, so that mode retains lighting.
  if(occupiedBrick==0&&spare2.w<.5){curl[p]=0;return;}
  // Include empty neighbors so boundary interpolation receives valid shadow depth.
  float3 light=normalize(float3(.6,.6,-.7));float shadow=0;
  // Resolve nearby folds first, then use wider intervals for distant smoke.
  // Integrate at each interval midpoint with its actual world-space length.
  float3 exitDistances=select(light>0,1-uv,-uv)/light;
  float exitDistance=min(exitDistances.x,min(exitDistances.y,exitDistances.z));
  float distance=0,stride=.0037;
  [loop]for(int s=0;s<20&&distance<exitDistance;s++){
   float interval=min(stride,exitDistance-distance);
   float3 probe=uv+light*(distance+interval*.5);
   shadow+=max(0,sampleD(probe*densityGrid()-.5).w-SmokeDensityCutoff)*smokeEnvelope(probe)*interval;
   distance+=interval;stride*=1.28;
  }
  float3 gradient=float3(
   sampledDensity.Load(int4(boundDensity(p+int3(1,0,0)),0)).w-sampledDensity.Load(int4(boundDensity(p-int3(1,0,0)),0)).w,
   sampledDensity.Load(int4(boundDensity(p+int3(0,1,0)),0)).w-sampledDensity.Load(int4(boundDensity(p-int3(0,1,0)),0)).w,
   sampledDensity.Load(int4(boundDensity(p+int3(0,0,1)),0)).w-sampledDensity.Load(int4(boundDensity(p-int3(0,0,1)),0)).w);
  gradient*=densityGrid()/densityGrid().z;
  curl[p]=float4(smokeLighting(shadow,gradient,density),float(occupiedBrick));return;
 }
 if(pass>6.5){
  float4 transported=sampledForward.Load(int4(p,0));
  // Only the legacy diagnostic needs a reverse trace and extrema correction.
  [branch]if(spare1.x>.5&&spare1.x<1.5){
   float3 departure=traceDensity(p,dt);
   float4 corrected=transported+.5*(sampledDensity.Load(int4(p,0))-sampleForward(traceDensity(p,-dt)));
   float4 low=8,high=0;int3 cell=(int3)floor(departure);
   [unroll]for(int z=0;z<2;z++)[unroll]for(int y=0;y<2;y++)[unroll]for(int x=0;x<2;x++){
    float4 value=sampledDensity.Load(int4(boundDensity(cell+int3(x,y,z)),0));low=min(low,value);high=max(high,value);
   }
   transported=select(or(corrected<low,corrected>high),transported,corrected);
  }
  // Bound pigment by total concentration after independent interpolation.
  transported.xyz=clamp(transported.xyz,0,transported.www);
  if(spare1.x>.5){curl[p]=storeHalf((spare1.x>1.5&&spare1.x<2.5)?sampledForward.Load(int4(p,0)):transported);return;}
  float4 d=transported*exp(-dt*.22);
  [unroll]for(int e=0;e<3;e++){
   float amount=emitter(uv,e);float3 pigment=e==0?float3(1,0,0):e==1?float3(0,1,0):float3(0,0,1);
   d+=float4(pigment,1)*amount;
  }
  // Transport source concentrations, not premultiplied display RGB.
  // Restore the concentration sum after independent scalar corrections.
  float pigmentMass=dot(d.xyz,1);
  if(pigmentMass>0)d.xyz*=d.w/pigmentMass;else d=0;
  float edge=min(min(min(uv.x,1-uv.x),min(uv.y,1-uv.y)),min(uv.z,1-uv.z));
  // Curl is scratch storage here, before the vorticity pass overwrites it.
  curl[p]=storeHalf(d*smoothstep(0,.04,edge)*min(1,8/max(d.w,1e-20)));return;
 }
 if(pass<1.5){ // Advection and forces both evaluated on component faces.
  float3 v=advectFaces(p)*exp(-dt*.13);
  [unroll]for(int axis=0;axis<3;axis++){
   float3 offset=0;offset[axis]=.5/grid;
   v[axis]+=bodyForce(uv-offset)[axis];
  }
#ifdef SMOKE_CLOSED_FACES
  v=wallFaces(v,p);
#endif
  velocityOut[p]=storeHalf(float4(v,0));return;
 }
 if(pass<2.5){float3 dx=(centerVelocity(p+int3(1,0,0))-centerVelocity(p-int3(1,0,0)))*.5,dy=(centerVelocity(p+int3(0,1,0))-centerVelocity(p-int3(0,1,0)))*.5,dz=(centerVelocity(p+int3(0,0,1))-centerVelocity(p-int3(0,0,1)))*.5;float3 c=float3(dy.z-dz.y,dz.x-dx.z,dx.y-dy.x);curl[p]=storeHalf(float4(c,length(c)));return;}
 if(pass<3.5){
  float3 centerForce=confinement(p);
  float3 faceForce=.5*(centerForce+float3(confinement(p-int3(1,0,0)).x,
   confinement(p-int3(0,1,0)).y,confinement(p-int3(0,0,1)).z));
  float3 v=vel(p)+faceForce;
#ifdef SMOKE_CLOSED_FACES
  v=wallFaces(v,p);
#endif
  velocityOut[p]=storeHalf(float4(v,0));return;
 }
 // Paired forward divergence and backward gradient compose the nearest-neighbor
 // Laplacian solved by Jacobi. Central differences composed a two-cell stencil.
 // Keep the previous pressure as the next solve's initial estimate. Resetting
 // it every frame wastes the bounded iteration budget on low-frequency error.
 if(pass<4.5){divergence[p]=(vel(p+int3(1,0,0)).x-vel(p).x+vel(p+int3(0,1,0)).y-vel(p).y+vel(p+int3(0,0,1)).z-vel(p).z);return;}
 if(pass<5.5){pressureOut[p]=(pressureIn[bound(p+int3(1,0,0))]+pressureIn[bound(p-int3(1,0,0))]+pressureIn[bound(p+int3(0,1,0))]+pressureIn[bound(p-int3(0,1,0))]+pressureIn[bound(p+int3(0,0,1))]+pressureIn[bound(p-int3(0,0,1))]-divergence[p])/6;return;}
 float3 v=vel(p)-float3(pressureIn[p]-pressureIn[bound(p-int3(1,0,0))],pressureIn[p]-pressureIn[bound(p-int3(0,1,0))],pressureIn[p]-pressureIn[bound(p-int3(0,0,1))]);
#ifdef SMOKE_CLOSED_FACES
 v=wallFaces(v,p);
#else
 if(p.x==0||p.x==(int)grid-1)v.x=0;if(p.y==0||p.y==(int)grid-1)v.y=0;if(p.z==0||p.z==(int)grid-1)v.z=0;
#endif
 velocityOut[p]=storeHalf(float4(v,0));
}







