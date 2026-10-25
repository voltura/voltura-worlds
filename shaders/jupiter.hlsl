#include "common.hlsli"
#include "jupiter_common.hlsli"
Texture2D<float4> cloudMap:register(t0);
Texture2D<float2> atmosphereMap:register(t1);
SamplerState globeSampler:register(s1);
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET{
 float2 q=jupiterScreen(uv);
 float3 origin=jupiterToBody(float3(q,3)),ray=jupiterToBody(float3(0,0,-1));
 float3 metric=float3(1,1/(jupiterPolar*jupiterPolar),1);
 float a=dot(ray*ray,metric),b=dot(origin*ray,metric),c=dot(origin*origin,metric)-1;
 float discriminant=b*b-a*c;
 float radial=sqrt(max(1-discriminant/a,.0000001));
 float edge=max(fwidth(radial),.000001),coverage=saturate((1-radial)/edge+.5);
 float distance=(-b-sqrt(max(discriminant,0)))/a;
 float3 body=origin+ray*distance,bodyNormal=normalize(body*metric);
 float2 coord=jupiterUV(bodyNormal),dx=jupiterGradient(ddx(coord)),dy=jupiterGradient(ddy(coord));
 float2 material=atmosphereMap.SampleGrad(globeSampler,coord,dx,dy);
 // Bounded, reversible advection follows the inferred OPAL zonal winds.
 // 9.925 h / 900 accelerates physical rates with the cinematic rotation.
 float wind=(material.g*255-128)*(.00001/127);
 float windTime=sin(jupiterAngle())*(900/(2*jupiterPi))*(9.925*3600/900)*turbulence;
 coord.x=frac(coord.x-wind*windTime/(2*jupiterPi));
 dx=jupiterGradient(ddx(coord));dy=jupiterGradient(ddy(coord));
 material=atmosphereMap.SampleGrad(globeSampler,coord,dx,dy);
 float3 n=jupiterToWorld(bodyNormal),light=jupiterLight();
 float mu=dot(n,light),muView=max(n.z,0),sun=jupiterSun(mu);
 float tau=.012*density*(.5+.5*material.r);
 float3 color=0;
 if(coverage>0){
  float3 albedo=cloudMap.SampleGrad(globeSampler,coord,dx,dy).rgb;
  // Cloud detail adjusts contrast around a lower-resolution observation;
  // visible color is never interpreted as solid terrain or cloud altitude.
  float level=max(.5*log2(max(dot(dx*4096,dx*4096),dot(dy*4096,dy*4096))),4);
  float3 smoothColor=cloudMap.SampleLevel(globeSampler,coord,level).rgb;
  albedo=max(lerp(smoothColor,albedo,detail),0);
  float incoming=exp(-tau*jupiterColumn(mu)),outgoing=exp(-tau*jupiterColumn(muView));
  color=albedo*(max(mu,0)*sun/jupiterPi)*incoming*outgoing*jupiterSolar;
  // Cheap curvature-bounded aerosol column above the opaque cloud deck.
  // Methane reflectance only modulates a proxy; tau is an artistic optical depth.
  float phase=.65;
  float3 haze=float3(.95,.91,.85)*.08*phase*sun*incoming*jupiterSolar;
  color+=haze*(1-outgoing);
  if(spare1.x==1)color=cloudMap.SampleGrad(globeSampler,coord,dx,dy).rgb/90;
  if(spare1.x==2)color=material.r/90;
  if(spare1.x==3)color=.15/90;
  if(spare1.x==4)color=(mu>0?.15:0)/90;
 }
 // The real scale height is subpixel at globe scale. Integrate its radial
 // exponential over a pixel footprint to keep the narrow limb stable.
 float height=max(radial-1,0),lo=max(height-edge*.5,0),hi=height+edge*.5;
 float average=jupiterScaleHeight/edge*(exp(-lo/jupiterScaleHeight)-exp(-hi/jupiterScaleHeight));
 float limbTau=.012*density*sqrt(2*jupiterPi/jupiterScaleHeight)*average;
 float limbAlpha=(1-exp(-limbTau))*saturate((1+8*jupiterScaleHeight-radial)/edge+.5);
 float horizon=sqrt(saturate(1-1/max(radial*radial,1)));
 float3 limbColor=float3(.95,.91,.85)*.08*.65*jupiterSolar*jupiterSun(mu+horizon);
 float alpha=coverage+(1-coverage)*limbAlpha;
 float3 radiance=color*coverage+limbColor*limbAlpha*(1-coverage);
 if(spare1.x>0){alpha=coverage;radiance=color*coverage;}
 float audio=movement>0?1+reaction*saturate(energy)*.06:1;
 return float4(tone(jupiterGrade(radiance*90*audio))/max(alpha,.00001),alpha);
}
