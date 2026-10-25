#include "common.hlsli"
Texture2D<float4> galaxy : register(t0);
SamplerState skySampler : register(s1);
float3 stars(float2 p,float scale,float threshold,float seedValue) {
 float2 cell=floor(p*scale),local=frac(p*scale)-.5;
 float h=hash(float3(cell,seedValue));
 float2 offset=float2(hash(float3(cell,seedValue+1)),hash(float3(cell,seedValue+2)))-.5;
 float r=length(local-offset*.7),width=lerp(.018,.065,pow(h,17));
 float visibility=smoothstep(threshold,1,h);
 float footprint=scale/resolution.y;
 float filteredWidth=sqrt(width*width+footprint*footprint*.35);
 float starCore=exp(-r*r/(filteredWidth*filteredWidth))*(width*width)/(filteredWidth*filteredWidth);
 float3 color=lerp(float3(.35,.65,1),float3(1,.71,.40),hash(float3(cell,seedValue+3)));
 return color*starCore*visibility*lerp(.1,2.7,pow(h,12));
}
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET {
 float2 p=(uv-.5)*float2(resolution.x/resolution.y,1);
 // Fixed galactic coordinates: the distant sky does not follow scene motion.
 float2 skyUV=float2(.5+.22*(p.y+.5*p.x),.5+.34*(p.x-.5*p.y+.10));
 float3 sky=galaxy.Sample(skySampler,skyUV).rgb;
 float3 tint=lerp(float3(.24,.70,1.05),float3(1.1,.66,.3),smoothstep(.62,.96,uv.x));
 float3 c=sky*tint*.42;
 c+=stars(p,260,.88,13)*.65+stars(p,65,.985,25)*.65+stars(p,24,.998,37);
 c*=lerp(.16,1,smoothstep(.02,.32,uv.x));
 // The sun sits on the planet's projected upper-right limb, not in open space.
 float2 sun=(uv-orbitSunUV())*float2(resolution.x/resolution.y,1);
 float radius=length(sun);
 float halo=.00013/(radius*radius+.0003);
 float core=exp(-radius*radius*65000)*22;
 c+=float3(1,.55,.19)*(halo+core);
 return float4(tone(c),0);
}





