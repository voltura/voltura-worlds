#include "common.hlsli"
#include "saturn_common.hlsli"
#include "orbit_rings.hlsli"
#include "planet_sky.hlsli"
Texture2D<float4> cloudMap:register(t0);
Texture2D<float4> galaxy:register(t1);
SamplerState globeSampler:register(s1);
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET{
 float2 skyP=(uv-.5)*resolution/min(resolution.x,resolution.y);
 float3 sky=planetSky(galaxy.Sample(globeSampler,planetSkyUV(skyP)),.0035);
 float2 q=saturnScreen(uv);
 float3 origin=saturnToBody(float3(q,4)),ray=saturnToBody(float3(0,0,-1));
 float disc;float globeT=saturnHit(origin,ray,disc);
 float3 metric=float3(1,1/(saturnPolar*saturnPolar),1);
 float a=dot(ray*ray,metric);
 float radial=sqrt(max(1-disc/a,0));
 float edge=max(fwidth(radial),.00001);
 float coverage=saturate((1-radial)/edge+.5);
 float3 body=origin+ray*globeT;
 float3 normal=normalize(body*metric);
 float latitude=asin(clamp(normal.y,-1,1));
 float longitude=atan2(normal.x,normal.z)-saturnAngle();
 longitude+=sin(saturnAngle())*.018*sin(latitude*19)*turbulence;
 float2 coord=float2(frac(longitude/(2*saturnPi)),.5-latitude/saturnPi);
 float2 dx=ddx(coord),dy=ddy(coord);dx.x-=round(dx.x);dy.x-=round(dy.x);
 float3 bodyLight=normalize(float3(-.45,.35,1)),light=saturnToWorld(bodyLight);
 float3 n=saturnToWorld(normal);
 float mu=dot(n,light),muView=max(n.z,0);
 float ringT=abs(ray.y)>.00001?-origin.y/ray.y:100000;
 float3 ringP=origin+ray*ringT;
 float ringR=length(ringP.xz),footprint=max(fwidth(ringR)*.5,.00002);
 float tau=saturnRingDepth(ringR,footprint);
 float muRingView=max(abs(ray.y),.0001),muRingLight=max(abs(bodyLight.y),.0001);
 float ringAlpha=1-exp(-tau/muRingView);
 if(ringT<0)ringAlpha=0;
 // Ring shadow on the globe uses the same radial optical-depth profile.
 float shadowT=abs(bodyLight.y)>.00001?-body.y/bodyLight.y:-1;
 float shadowR=length((body+bodyLight*max(shadowT,0)).xz);
 float shadowDepth=saturnRingDepth(shadowR,max(fwidth(shadowR)*.5,.00002));
 float globeVisibility=shadowT>0?exp(-shadowDepth/muRingLight):1;
 // Globe shadow on the ring is an ellipsoid intersection along sunlight.
 float shadowDisc;float shadowHit=saturnHit(ringP+bodyLight*.0001,bodyLight,shadowDisc);
 float ringVisibility=shadowHit>0?smoothstep(0,.008, -shadowDisc):1;
 float3 globe=0;
 if(coverage>0){
  float3 albedo=cloudMap.SampleGrad(globeSampler,coord,dx,dy).rgb;
  float level=max(.5*log2(max(dot(dx*4096,dx*4096),dot(dy*4096,dy*4096))),5);
  float3 smoothColor=cloudMap.SampleLevel(globeSampler,coord,level).rgb;
  albedo=max(lerp(smoothColor,albedo,detail),0);
  // Retain the real northern hexagonal jet but soften the authored map's
  // sharp polar boundary so it reads as clouds rather than a cut-out polygon.
  albedo=lerp(albedo,smoothColor,.65*smoothstep(radians(68),radians(78),latitude));
  float haze=1-exp(-.012*density/ sqrt(muView*muView+.0015));
  globe=albedo*(max(mu,0)*globeVisibility*.7+.009);
  globe=globe*(1-haze)+float3(.92,.84,.66)*haze*.16*max(mu,0)*globeVisibility;
 }
 float scattering=ringScattering(tau,muRingView,muRingLight,ray.y*bodyLight.y<0);
 float3 ringColor=lerp(float3(.55,.49,.39),float3(.9,.85,.73),saturate((ringR-1.239)/.7));
 ringColor*=.88+.10*sin(ringR*85+2*sin(ringR*23))*exp(-.5*pow(85*footprint,2))+.05*sin(ringR*220)*exp(-.5*pow(220*footprint,2));
 float3 ring=ringColor*scattering*(.65*ringVisibility+.005);
 float audio=movement>0?1+reaction*saturate(energy)*.06:1;
 globe=saturnGrade(globe)*audio;ring=saturnGrade(ring)*audio;
 // Ring emission is premultiplied. Composite against the far layer first.
 float3 color;
 if(ringT<globeT)color=ring+(lerp(sky,globe,coverage))*(1-ringAlpha);
 else color=lerp(ring+sky*(1-ringAlpha),globe,coverage);
 // Narrow pixel-integrated atmosphere outside the solid silhouette.
 float h=max(radial-1,0),scaleHeight=.0009;
 float limb=density*.035*scaleHeight/edge*(exp(-max(h-edge*.5,0)/scaleHeight)-exp(-(h+edge*.5)/scaleHeight));
 color+=saturnGrade(float3(.9,.8,.6))*limb*(1-coverage)*max(mu,0)*audio*(ringT<globeT?1-ringAlpha:1);
 if(spare1.x==1)color=coverage*.2;
 if(spare1.x==2)color=ringAlpha*(ringT<globeT?1:1-coverage)*.2;
 if(spare1.x==3)color=coverage*(1-globeVisibility)*.2;
 if(spare1.x==4)color=ringAlpha*(1-ringVisibility)*(1-coverage)*.2;
 if(spare1.x==5)color=sky*(1-ringAlpha)*(1-coverage);
 if(spare1.x==6)color=sky;
 return float4(tone(color),1);
}
