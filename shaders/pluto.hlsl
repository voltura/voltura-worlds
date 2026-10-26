#include "common.hlsli"
#include "pluto_common.hlsli"
Texture2D<float4> surfaceMap:register(t0);
SamplerState planetSampler:register(s1);
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET{
 float2 q=plutoScreen(uv);float r=length(q),edge=max(fwidth(r),.00001);
 float disc=saturate((1-r)/edge+.5);
 // A thin artistic haze, not a volumetric atmospheric simulation.
 float width=.0035;float outer=1+width*5;
 float coverage=saturate((outer-r)/edge+.5);clip(coverage-.00001);
 float3 n=normalize(float3(q,sqrt(max(1-dot(q,q),.000001))));
 float2 coord=plutoUV(plutoBody(n));
 float3 albedo=surfaceMap.SampleGrad(planetSampler,coord,plutoGradient(ddx(coord)),plutoGradient(ddy(coord))).rgb;
 // Photographic detail/contrast only: brightness is not terrain height.
 float luminance=max(dot(albedo,float3(.2126,.7152,.0722)),.001);
 albedo*=pow(luminance/.3,(detail-1)*.25);
 float mu=dot(n,plutoLight()),mu0=max(mu,0),view=max(n.z,.01);
 float diffuse=mu0*(.55+.45/max(mu0+view,.08));
 float day=smoothstep(-.04,.22,mu);
 float shell=exp(-max(r-1,0)/width);
 float limb=pow(1-saturate(n.z),5);
 float3 haze=float3(.11,.28,.52)*density*day;
 // Exposure is compensated for wallpaper readability at Pluto's distance.
 float3 surface=albedo*diffuse*1.9+haze*limb*.14;
 float audio=movement>0?1+reaction*saturate(energy)*.04:1;
 float alpha=lerp(shell*day*saturate(density*.35),1,disc)*coverage;
 float3 premult=surface*disc+haze*shell*.25*(1-disc);
 // Diagnostic modes: unlit geography, exact globe mask, haze only.
 if(spare1.x==1)return float4(tone(albedo),disc);
 if(spare1.x==2)return float4(tone(float3(.2,.2,.2)),disc);
 if(spare1.x==3){alpha=coverage;premult=haze*(limb*disc*.14+shell*.25*(1-disc));}
 return float4(tone(plutoGrade(premult*audio/max(alpha,.00001))),alpha);
}
