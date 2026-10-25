#include "common.hlsli"

float contourTerrain(float2 q) {
 float value=0,weight=.5;
 int octaves=spare0.x<.5?3:spare0.x<1.5?4:5;
 [loop]for(int i=0;i<octaves;i++) {
  // Detail adds fine structure without changing the large, continuous terrain.
  float fineWeight=i<2?1:lerp(.15,1,saturate(detail*.5));
  value+=noise(float3(q,seed*.01+7.))*weight*fineWeight;
  q=q*2.03+float2(4.1,1.7);weight*=.5;
 }
 return value;
}
float3 contourTint(float height) {
 if(palette<.5)return lerp(float3(.09,.43,.42),float3(.66,.5,.16),smoothstep(.35,.8,height));
 return lerp(colorA(),colorB(),saturate(height));
}
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET {
 float2 p=(pixel.xy-resolution*.5)/resolution.y;
 float t=time*movement*(.3+.7*flow);
 float audio=saturate(reaction*(.6*energy+.25*bass+.15*mid))*step(.0001,movement);
 float2 q=p*3.+float2(.12*sin(t*.12),t*.027);
 float2 warp=float2(sin(q.y*1.5+t*.08),cos(q.x*1.7-t*.06));
 q+=warp*(.08+.2*turbulence+audio*.08);
 float height=contourTerrain(q)+.10*sin(q.x*1.7+t*.08);
 float levels=lerp(9.,30.,saturate(density*.5));
 float bands=height*levels;
 float distanceToLine=abs(frac(bands+.5)-.5);
 // Pixel-footprint filtering prevents stair steps and sparkling at high density.
 float aa=max(fwidth(bands),.0001);
 float thickness=lerp(.026,.055,saturate(detail*.5));
 float ink=1-smoothstep(thickness,thickness+aa,distanceToLine);
 float3 tint=contourTint(height);
 // Screen derivatives reuse the terrain evaluation, adding relief without
 // additional fractal samples. The slope is normalized to scene coordinates.
 float2 slope=float2(ddx(height),ddy(height))*resolution.y/3;
 float3 normal=normalize(float3(-slope*.65,1));
 float relief=saturate(dot(normal,normalize(float3(-.55,-.65,1))));
 float3 c=lerp(float3(.003,.008,.014),tint*(.018+.065*relief),saturate(height));
 c+=tint*ink*(.30+.20*relief+audio*.28);
 // Wider major contours give a quiet hierarchy instead of a uniform wire field.
 float majorDistance=abs(frac(bands*.2+.5)-.5)*5;
 float major=1-smoothstep(thickness*1.7,thickness*1.7+aa,majorDistance);
 c+=lerp(tint,float3(.72,.68,.48),.15)*major*.19;
 return float4(tone(c),1);
}
