#include "smoke_atlas.hlsli"
#include "smoke_half.hlsli"
#include "smoke_sources.hlsli"
cbuffer AtlasStep:register(b0){
 uint fineGrid;uint interior;uint atlasAxis;uint slotCount;
 float delta;uint initialize;uint halfStorage;uint staggeredVelocityGrid;
 float decayRate;uint correctedTransport;float seconds;float flowStrength;
 float audio;uint injectSources;uint forwardMode;float fineStrength;
}
Texture3D<float4> coarse:register(t0);
Texture3D<float4> previousAtlas:register(t1);
StructuredBuffer<SmokeFinePage> previousPages:register(t2);
Texture3D<float4> velocity:register(t3);
// Destination slot -> virtual page. UINT_MAX marks unused destination storage.
StructuredBuffer<uint> destinationPages:register(t4);
#ifdef SMOKE_CACHED_FORWARD
Texture3D<float4> forwardAtlas:register(t5);
#endif
SamplerState linearClamp:register(s0);
RWTexture3D<float4> nextAtlas:register(u0);
#ifdef SMOKE_FINE_CURL
float3 fineCurl(float3 p){
 float3 lower=atlasAxis>8?float3(.25,.125,.25):float3(.375,.375,.375);
 float3 upper=atlasAxis>8?float3(.75,.875,.5625):float3(.625,.625,.625);
 float fade=atlasAxis>8?.03125:.0625;
 float3 t=saturate(min(p-lower,upper-p)/fade);
 float3 f=t*t*(3-2*t),df=-sign(p-(lower+upper)*.5)*6*t*(1-t)/fade;
 float w=f.x*f.y*f.z;float3 grad=float3(df.x*f.y*f.z,f.x*df.y*f.z,f.x*f.y*df.z);
 float3 potential=0,curl=0;
 [unroll]for(int i=0;i<12;i++){
  float3 k=normalize(float3(sin(i*7.13+1.7),cos(i*4.71+.3),sin(i*3.17+2.4)));
  float3 a=normalize(float3(cos(i*5.37+.8),sin(i*8.23+1.1),cos(i*2.71+.6)));
  float frequency=i<6?113.0973:226.1947;
  float phase=dot(k,p)*frequency+i*3.77+seconds*(.08+i*.007);
  float amplitude=(i<6?1:.63)/3.4641;
  potential+=a*sin(phase)*(amplitude/frequency);
  curl+=cross(k,a)*cos(phase)*amplitude;
 }
 return fineStrength*(w*curl+cross(grad,potential));
}
#endif
float3 atlasFlow(float3 position){
#ifdef SMOKE_FINE_CURL
 float3 added=fineStrength>0?fineCurl(position):0;
 if(staggeredVelocityGrid==0)return velocity.SampleLevel(linearClamp,position,0).xyz+added;
#else
 if(staggeredVelocityGrid==0)return velocity.SampleLevel(linearClamp,position,0).xyz;
#endif
 // Smoke stores each velocity component on its negative cell face. Match the
 // coarse solver's staggered reconstruction, not a collocated vector sample.
 float h=.5/staggeredVelocityGrid;
 return float3(velocity.SampleLevel(linearClamp,position+float3(h,0,0),0).x,
               velocity.SampleLevel(linearClamp,position+float3(0,h,0),0).y,
               velocity.SampleLevel(linearClamp,position+float3(0,0,h),0).z)
#ifdef SMOKE_FINE_CURL
 +added
#endif
 ;
}
float3 traceAtlas(float3 p,float step){return p-atlasFlow(p-atlasFlow(p)*(step*.5))*step;}
float4 originalAt(float3 p){return smokeSampleAtlas(p,fineGrid,interior,atlasAxis,coarse,previousAtlas,previousPages,linearClamp);}
float4 predictAt(float3 p){float3 q=traceAtlas(p,delta);return any(q<0)||any(q>1)?0:originalAt(q);}
float4 correctedAt(float3 p,float3 departure,float4 forward){
 float3 reverse=traceAtlas(p,-delta);
 if(any(reverse<0)||any(reverse>1)||any(departure<0)||any(departure>1))return forward;
 float3 cell=reverse*fineGrid-.5;int3 base=(int3)floor(cell);float3 fraction=frac(cell);
 float4 backward=0;
 [loop]for(int z=0;z<2;z++)[loop]for(int y=0;y<2;y++)[loop]for(int x=0;x<2;x++){
  int3 c=base+int3(x,y,z);
  if(any(c<0)||any(c>=(int)fineGrid))return forward;
  uint3 tile=(uint3)c/interior;uint axis=fineGrid/interior;
  SmokeFinePage page=previousPages[tile.x+axis*(tile.y+axis*tile.z)];
  if(page.slot>=atlasAxis*atlasAxis*atlasAxis||page.blend<1)return forward;
  float weight=(x?fraction.x:1-fraction.x)*(y?fraction.y:1-fraction.y)*(z?fraction.z:1-fraction.z);
#ifdef SMOKE_CACHED_FORWARD
  uint3 physical=uint3(page.slot%atlasAxis,(page.slot/atlasAxis)%atlasAxis,page.slot/(atlasAxis*atlasAxis));
  backward+=weight*forwardAtlas.Load(int4(physical*(interior+2)+(uint3)c%interior+1,0));
#else
  backward+=weight*predictAt((float3(c)+.5)/fineGrid);
#endif
 }
 float4 center=smokeFineValue((int3)(p*fineGrid),fineGrid,interior,atlasAxis,
                           coarse,previousAtlas,previousPages,linearClamp);
 float4 corrected=forward+.5*(center-backward),low=1e10,high=-1e10;
 int3 departureBase=(int3)floor(departure*fineGrid-.5);
 [loop]for(int z=0;z<2;z++)[loop]for(int y=0;y<2;y++)[loop]for(int x=0;x<2;x++){
  int3 c=clamp(departureBase+int3(x,y,z),0,(int)fineGrid-1);
  float4 value=smokeFineValue(c,fineGrid,interior,atlasAxis,coarse,previousAtlas,previousPages,linearClamp);
  low=min(low,value);high=max(high,value);
 }
 return select(or(corrected<low,corrected>high),forward,corrected);
}
[numthreads(64,1,1)]
void CS(uint3 id:SV_DispatchThreadID){
 uint cells=interior*interior*interior,slot=id.x/cells;
 if(slot>=slotCount)return;
 uint page=destinationPages[slot];if(page==0xffffffff)return;
 uint virtualAxis=fineGrid/interior;
 if(page>=virtualAxis*virtualAxis*virtualAxis)return;
 uint localId=id.x%cells;
 uint3 local=uint3(localId%interior,(localId/interior)%interior,localId/(interior*interior));
 uint3 virtualTile=uint3(page%virtualAxis,(page/virtualAxis)%virtualAxis,page/(virtualAxis*virtualAxis));
 float3 position=(float3(virtualTile*interior+local)+.5)/fineGrid;
 uint3 physicalTile=uint3(slot%atlasAxis,(slot/atlasAxis)%atlasAxis,slot/(atlasAxis*atlasAxis));
 uint3 destination=physicalTile*(interior+2)+local+1;
#ifdef SMOKE_CACHED_FORWARD
 if(forwardMode==1){nextAtlas[destination]=predictAt(position);return;}
#endif
 float4 value;
 if(initialize!=0)value=coarse.SampleLevel(linearClamp,position,0);
 else {
  float3 departure=traceAtlas(position,delta);
#ifdef SMOKE_CACHED_FORWARD
  value=forwardAtlas.Load(int4(destination,0));
#else
  value=any(departure<0)||any(departure>1)?0:
   smokeSampleAtlas(departure,fineGrid,interior,atlasAxis,coarse,previousAtlas,previousPages,linearClamp);
#endif
  if(correctedTransport!=0)value=correctedAt(position,departure,value);
  value*=exp(-delta*decayRate);
  if(injectSources!=0){
   [unroll]for(int e=0;e<3;e++)value[e]+=smokeEmitter(position,e,seconds,delta,flowStrength,audio);
  }
 }
 // Pigment concentrations are the independent transported quantities.
 // Reconstruct total density after clipping residual undershoot.
 value.xyz=max(value.xyz,0);value.w=dot(value.xyz,1);
 if(injectSources!=0)value*=min(1,8/max(value.w,1e-20));
 nextAtlas[destination]=halfStorage!=0?storeHalf(value):value;
}
