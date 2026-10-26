#include "common.hlsli"
#include "uranus_common.hlsli"

float3 uranusClouds(float3 n){
 float t=uranusClock();
 // Cartesian noise avoids longitude seams and polar singularities. Slow
 // latitude-dependent advection is independent of the 900-second body spin.
 float wind=t*turbulence*.00016*(1-2*n.y*n.y);
 float3 p=uranusRY(n,frac(t/900)*(2*uranusPi)+wind);
 float latitude=asin(clamp(n.y,-1,1));
 float bands=sin(latitude*19+.16*sin(latitude*37))*.65+sin(latitude*42)*.35;
 float broad=fbm(p*5+float3(7,3,11))-.5;
 float fine=fbm(p*27+float3(13,1,5))-.5;
 float polar=smoothstep(.55,.95,n.y)*.035;
 float variation=bands*.018*flow+broad*.055+fine*.035*detail;
 // Subdued methane-filtered visible-light clouds, rather than infrared bands.
 return float3(.47,.73,.75)*(1+variation)+polar*float3(.8,.9,.85);
}
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET{
 float2 q=uranusScreen(uv);
 float3 o=uranusToBody(float3(q,3)),d=uranusToBody(float3(0,0,-1));
 float3 metric=float3(1,1/(uranusPolar*uranusPolar),1);
 float a=dot(d*d,metric),b=dot(o*d,metric),c=dot(o*o,metric)-1;
 float disc=b*b-a*c;
 float r=sqrt(max(1-disc/a,.000001));
 float edge=max(fwidth(r),.000001),coverage=saturate((1-r)/edge+.5);
 float3 body=o+d*((-b-sqrt(max(disc,0)))/a);
 float3 n=uranusToWorld(normalize(body*metric));
 float mu=dot(n,uranusLight()),view=max(n.z,.001);
 float day=smoothstep(-.015,.015,mu);
 float incidence=max(mu,0)*day;
 // A diffuse cloud deck with gentle limb darkening and a fixed terminator.
 float diffuse=incidence*(.72+.28/max(incidence+view,.08));
 float3 albedo=uranusClouds(normalize(body));
 float3 color=albedo*diffuse*1.5;
 float hazeWidth=.0025+.0025*density;
 float shell=exp(-max(r-1,0)/hazeWidth);
 float outer=saturate((1+hazeWidth*4-r)/edge+.5);
 clip(outer-.00001);
 float limb=pow(1-saturate(view),3);
 float3 haze=float3(.43,.70,.76)*(.045+.035*density)*day;
 color+=haze*limb;
 // Audio changes radiance only: never geometry, light, stars or camera.
 float audio=movement>0?reaction*saturate(energy)*.035:0;
 color*=1+audio;
 float haloAlpha=shell*day*.28;
 float alpha=lerp(haloAlpha,1,coverage)*outer;
 float3 premult=lerp(haze*shell*.28*(1+audio),color,coverage)*outer;
 // Native capture diagnostics: albedo, opaque silhouette and illumination.
 if(spare1.x>.5&&spare1.x<1.5)return float4(tone(albedo)*coverage,coverage);
 if(spare1.x>1.5&&spare1.x<2.5)return float4(coverage.xxx,1);
 if(spare1.x>2.5)return float4((diffuse*coverage).xxx,1);
 return float4(tone(uranusGrade(premult))/max(alpha,.00001),alpha);
}
