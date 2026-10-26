#include "common.hlsli"
#include "mercury_common.hlsli"
Texture2D<float4> surfaceMap:register(t0);
Texture2D<float4> normalMap:register(t1);
SamplerState planetSampler:register(s1);
float2 mercuryGradient(float2 v){v.x-=round(v.x);return v;}
float solarDisc(float mu){
 // Solar angular radius at Mercury's mean distance, 0.387 AU.
 float x=clamp(mu/.012015,-1,1);
 return .5+(asin(x)+x*sqrt(max(1-x*x,0)))/mercuryPi;
}
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET{
 float2 q=mercuryScreen(uv);
 float r2=dot(q,q),radius=sqrt(r2),edge=max(fwidth(radius),.00001);
 float coverage=saturate((1-radius)/edge+.5);
 float3 n=normalize(float3(q,sqrt(max(1-r2,.000001))));
 float3 body=mercuryToBody(n);
 float longitude=atan2(body.z,body.x),latitude=asin(clamp(body.y,-1,1));
 float2 coord=float2(longitude/(2*mercuryPi)+.5,.5-latitude/mercuryPi);
 float2 dx=mercuryGradient(ddx(coord)),dy=mercuryGradient(ddy(coord));
 clip(coverage-.00001);
 float3 albedo=surfaceMap.SampleGrad(planetSampler,coord,dx,dy).rgb;
 // Contrast operates on measured geography, without adding synthetic craters.
 albedo*=pow(max(dot(albedo,float3(.2126,.7152,.0722)),.001)/.18,(density-1)*.3);
 float3 tn=normalize(normalMap.SampleGrad(planetSampler,coord,dx,dy).xyz*2-1);
 tn=normalize(float3(tn.xy*(detail*2.4),max(tn.z,.1)));
 float3 east=float3(-sin(longitude),0,cos(longitude));
 float3 north=float3(-cos(longitude)*sin(latitude),cos(latitude),-sin(longitude)*sin(latitude));
 float3 bump=mercuryToWorld(normalize(east*tn.x+north*tn.y+body*tn.z));
 float3 light=mercuryLight();
 float mu=dot(n,light),mu0=max(mu,0),muView=max(n.z,.001);
 // A normal map cannot lift a ridge above the geometric horizon. Fade its
 // lighting perturbation before the finite solar-disc cutoff.
 float slope=clamp(dot(bump,light)-mu,-.15,.15);
 float incidence=max(mu0+slope*smoothstep(0,.08,mu0),0)*solarDisc(mu);
 float diffuse=incidence*(.45+.55/max(mu0+muView,.08));
 float audio=movement>0?reaction*saturate(energy)*.035:0;
 // Cinematic exposure preserves detail under strong distance-based sunlight.
 float3 color=albedo*mercurySolar*diffuse*.55*(1+audio);
 return float4(tone(mercuryGrade(color)),coverage);
}
