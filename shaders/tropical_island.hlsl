#include "common.hlsli"
#include "tropical_island_masks.hlsli"
#include "tropical_island_ocean.hlsli"
Texture2D<float4> artwork:register(t0);
Texture2D<float4> cleanSand:register(t1);
SamplerState linearClamp:register(s0);
float islandGust(float t,float phase) {
 return .64*sin(t*.57+phase)+.26*sin(t*.93+phase*.7)+.10*sin(t*1.41+phase*1.3);
}
float3 islandShadow(float2 p,float2 texel) {
 float3 transmission=0.;
 [unroll]for(int y=-1;y<=1;y+=2)[unroll]for(int x=-1;x<=1;x+=2) {
  float2 q=p+float2(x,y)*texel;
  transmission+=clamp(artwork.SampleLevel(linearClamp,q,0).rgb/
                     max(cleanSand.SampleLevel(linearClamp,q,0).rgb,.035),.15,1.);
 }
 return transmission*.25;
}
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET {
 uint width,height;artwork.GetDimensions(width,height);
 float2 fit=float2((resolution.x/resolution.y)/(float(width)/float(height)),1.);
 fit/=max(1.,fit.x);
 float2 p=(uv-.5)*fit+.5;
 float t=time*movement;
 float phase=p.x<.5?0.:1.7;
 float gust=islandGust(t,phase);
 float2 leaf=float2(gust+.10*sin(t*1.71+p.x*43.+p.y*28.),.28*gust);
 float3 photo=artwork.SampleLevel(linearClamp,p+leaf*.0048*density*islandPalms(p),0).rgb;
 // Keep the existing photographic display exposure for the distant scenery.
 float3 c=-log(max(1.-min(photo,.98),.02));
 float coastMask=smoothstep(.5045,.507,p.y);
 // Preserve the rocky headland; all sea and the foreground beach are rebuilt.
 float bank=1.-smoothstep(.265,.29,p.x);
 coastMask*=1.-bank*(1.-smoothstep(.607-.28*p.x,.618-.28*p.x,p.y));
 [branch]if(coastMask>.001) {
  float audio=saturate(reaction*(energy*.6+bass*.4))*step(.0001,movement)*step(.0001,flow);
  float strength=turbulence*(1.+audio*.18);
  float3 coast=coastRender(p,t*flow,strength);
  // Shadows remain coupled to the palm gust on dry sand.
  float dry=islandDrySand(p);
  float projection=lerp(.6,1.3,saturate((p.y-.62)/.38));
  float shadowGust=islandGust(t,0.);
  float2 offset=float2(shadowGust*.0075,shadowGust*.0026)*density*projection;
  coast*=lerp(1.,islandShadow(p+offset,1./float2(width,height)),dry);
  c=lerp(c,coast,coastMask);
 }
 if(palette>.5&&palette<1.5)c*=float3(1.065,1.015,.94);
 else if(palette>1.5&&palette<2.5)c*=float3(.94,1.025,1.055);
 else if(palette>2.5)c=lerp(c,dot(c,float3(.2126,.7152,.0722)).xxx,.16)*.91;
 return float4(tone(c),1);
}
