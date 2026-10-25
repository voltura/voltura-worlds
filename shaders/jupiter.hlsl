#include "common.hlsli"
#include "jupiter_common.hlsli"
Texture2D<float4> cloudMap:register(t0);
Texture2D<float2> atmosphereMap:register(t1);
SamplerState globeSampler:register(s1);
// Shared elliptical mask anchors the observed storm while belts pass it.
float3 jupiterStorm(float2 uv){
 float2 offset=float2(uv.x-38.0/360,uv.y-(.5+22.0/180));
 offset.x-=round(offset.x);
 float2 p=offset/float2(11.0/360,6.0/180);
 return float3(p,1-smoothstep(.65,1.5,length(p)));
}
float2 jupiterCloudUV(float2 uv,float wind,float age){
 uv.x-=wind*age*300*turbulence*(1-jupiterStorm(uv).z)/(2*jupiterPi);
 return float2(frac(uv.x),uv.y);
}
float4 jupiterCloudSample(float2 coord){
 float2 dx=jupiterGradient(ddx(coord)),dy=jupiterGradient(ddy(coord));
 float2 material=atmosphereMap.SampleGrad(globeSampler,coord,dx,dy);
 float3 albedo=cloudMap.SampleGrad(globeSampler,coord,dx,dy).rgb;
 float level=max(.5*log2(max(dot(dx*4096,dx*4096),dot(dy*4096,dy*4096))),4);
 float3 smoothColor=cloudMap.SampleLevel(globeSampler,coord,level).rgb;
 return float4(max(lerp(smoothColor,albedo,spare1.x==1?1:detail),0),material.r);
}
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
 float2 coord=jupiterUV(bodyNormal);
 float4 clouds;
 if(turbulence==0){clouds=jupiterCloudSample(coord);}
 else{
  float2 dx=jupiterGradient(ddx(coord)),dy=jupiterGradient(ddy(coord));
  float wind=(atmosphereMap.SampleGrad(globeSampler,coord,dx,dy).g*255-128)*(.00001/127);
  // Two staggered backtraces retire at zero weight, bounding distortion
  // without a reversing wind or a visible reset. Age is centered to keep
  // both maps close to the source and retain fine cloud structure.
  float phase=frac(jupiterClock()/80),other=frac(phase+.5);
  float weight=.5-.5*cos(2*jupiterPi*phase);
  float4 first=jupiterCloudSample(jupiterCloudUV(coord,wind,(phase-.5)*80));
  float4 second=jupiterCloudSample(jupiterCloudUV(coord,wind,(other-.5)*80));
  clouds=lerp(second,first,weight);
  // Continuous circulation with a bounded radial perturbation: rings turn
  // at gently varying rates without accumulating differential shear.
  float3 storm=jupiterStorm(coord);
  float clock=jupiterClock()*turbulence,r=length(storm.xy);
  float angle=-clock*.025+.16*(sin(clock*.06-r*2)+sin(r*2));
  float sn,cs;sincos(angle,sn,cs);
  float2 turned=float2(cs*storm.x-sn*storm.y,sn*storm.x+cs*storm.y);
  float2 vortexUV=float2(38.0/360,.5+22.0/180)+turned*float2(11.0/360,6.0/180);
  // Blend samples, rather than folding coordinates at the vortex edge.
  float4 vortexClouds=jupiterCloudSample(float2(frac(vortexUV.x),saturate(vortexUV.y)));
  clouds=lerp(clouds,vortexClouds,storm.z);

 }
 float3 n=jupiterToWorld(bodyNormal),light=jupiterLight();
 float mu=dot(n,light),muView=max(n.z,0),sun=jupiterSun(mu);
 float tau=.012*density*(.5+.5*clouds.a);
 float3 color=0;
 if(coverage>0){
  float3 albedo=clouds.rgb;
  float incoming=exp(-tau*jupiterColumn(mu)),outgoing=exp(-tau*jupiterColumn(muView));
  color=albedo*(max(mu,0)*sun/jupiterPi)*incoming*outgoing*jupiterSolar;
  // Cheap curvature-bounded aerosol column above the opaque cloud deck.
  // Methane reflectance only modulates a proxy; tau is an artistic optical depth.
  float phase=.65;
  float3 haze=float3(.95,.91,.85)*.08*phase*sun*incoming*jupiterSolar;
  color+=haze*(1-outgoing);
  if(spare1.x==1)color=clouds.rgb/90;
  if(spare1.x==2)color=clouds.a/90;
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
