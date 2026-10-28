#include "common.hlsli"
#include "tropical_island_ocean.hlsli"
Texture2D<float4> artwork:register(t0);
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET {
 uint width,height;artwork.GetDimensions(width,height);
 // Image placement follows the display; wave filtering follows this target.
 float2 fit=float2((spare0.y/spare0.z)/(float(width)/float(height)),1.);
 fit/=max(1.,fit.x);
 float2 p=(uv-.5)*fit+.5;
 // Include a margin above the coast blend for bilinear reconstruction.
 [branch]if(p.y<.49)return 0.;
 float audio=saturate(reaction*(energy*.6+bass*.4))*step(.0001,movement)*step(.0001,flow);
 float strength=turbulence*(1.+audio*.18);
 return float4(coastRender(p,time*movement*flow,strength),1);
}
