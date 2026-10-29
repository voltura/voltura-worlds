#include "common.hlsli"
#include "moon_common.hlsli"
Texture2D<float4> surfaceMap:register(t0);
Texture2D<float4> normalMap:register(t1);
Texture2D<float4> earthMap:register(t2);
SamplerState globeSampler:register(s1);
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET{
 float3 origin,ray;moonCamera(uv,origin,ray);
 float moonDistance,coverage,earthDistance,earthCoverage;float3 n,en;
 moonSphere(origin,ray,0,1,moonDistance,coverage,n);
 moonSphere(origin,ray,float3(0,0,moonEarthDistance),moonEarthRadius,earthDistance,earthCoverage,en);
 #ifdef JOURNEY
 earthCoverage=0; // Earth is a separate body in the shared scene.
#endif
 float2 coord=moonUV(n),dx=moonGradient(ddx(coord)),dy=moonGradient(ddy(coord));
 // Earth's fixed orientation and cloud composite are sufficient at its
 // natural ~2-degree diameter; no full-size Earth textures or air integration.
 float3 earthBody=float3(en.x,.91748*en.y+.39778*en.z,-.39778*en.y+.91748*en.z);
 float2 earthCoord=moonUV(earthBody),ex=moonGradient(ddx(earthCoord)),ey=moonGradient(ddy(earthCoord));
 float3 light=moonLight(),ground=0,earth=0;
 if(coverage>0){
  float3 albedo=surfaceMap.SampleGrad(globeSampler,coord,dx,dy).rgb;
  float3 tn=normalMap.SampleGrad(globeSampler,coord,dx,dy).xyz*2-1;
  tn=normalize(float3(tn.xy*detail*2.4,max(tn.z,.1)));
  float lon=(coord.x-.5)*2*moonPi,lat=(.5-coord.y)*moonPi;
  float3 east=float3(cos(lon),0,-sin(lon));
  float3 north=float3(-sin(lon)*sin(lat),cos(lat),-cos(lon)*sin(lat));
  float3 bump=normalize(east*tn.x+north*tn.y+n*tn.z);
  float mu=dot(n,light),mu0=max(mu,0),muView=max(dot(n,-ray),.001);
  float slope=clamp(dot(bump,light)-mu,-.15,.15);
  float incidence=max(mu0+slope*smoothstep(0,.08,mu0),0)*moonSun(mu);
  float diffuse=incidence*(.2+.8/max(mu0+muView,.08));
  ground=albedo*diffuse;
  // Capture-only probes isolate geography, direct light and exact sphere masks.
  if(spare1.x==1)ground=albedo;
  if(spare1.x==2)ground=mu<-.02?1:0;
  if(spare1.x==3)ground=1;
 }
 if(earthCoverage>0){
  float4 map=earthMap.SampleGrad(globeSampler,earthCoord,ex,ey);
  float mu=dot(en,light),day=max(mu,0)*moonSun(mu);
  earth=map.rgb*day/moonPi;
  earth+=float3(1,.63,.28)*pow(map.a,1.35)*(.42/moonPi)*(1-smoothstep(-.12,.02,mu));
  float rim=pow(1-saturate(dot(en,-ray)),4)*day;
  earth+=float3(.03,.10,.24)*rim;
  earth*=density;
  if(spare1.x==3)earth=float3(0,1,0);
 }
 if(spare1.x==1||spare1.x==2){earth=0;earthCoverage=0;}
 // Correct front-to-back occlusion, including partially covered limb pixels.
 float mc=coverage,ec=earthCoverage;
 float3 color=earth*ec*(1-mc)+ground*mc;
 if(earthDistance<moonDistance)color=ground*mc*(1-ec)+earth*ec;
 float alpha=mc+ec*(1-mc);
 if(spare1.x==3)return float4(color/max(alpha,.00001)*.15,alpha);
 float audio=movement>0?1+reaction*saturate(energy)*.06:1;
 return float4(tone(moonGrade(color*3.2*audio))/max(alpha,.00001),alpha);
}
