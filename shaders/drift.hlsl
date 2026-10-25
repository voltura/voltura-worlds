#include "common.hlsli"

// Infinite, continuously warped layers. Neighbour-cell accumulation keeps a
// particle's complete footprint when its centre crosses a cell or screen edge.
float3 driftTint(float value) {
 if(palette<.5)return lerp(float3(.025,.62,.78),float3(.42,.72,.16),value);
 return lerp(colorA(),colorB(),value);
}
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET {
 float2 p=(pixel.xy-resolution*.5)/resolution.y;
 float t=time*movement*(.3+.7*flow);
 float audio=saturate(reaction*(.55*energy+.3*bass+.15*treble))*step(.0001,movement);
 float3 c=float3(.0025,.008,.015)+driftTint(.1)*.006;
 int layers=spare0.x<.5?2:spare0.x<1.5?3:4;
 float concentration=lerp(.12,.56,saturate(density*.5));
 [loop]for(int layer=0;layer<layers;layer++) {
  float z=(float)layer;
  float2 q=p*(13.+z*8.);
  q+=float2(t*(.34-z*.055),t*(.048-z*.008));
  // Global warp is smooth across every cell; no independently wrapped sprites.
  q.y+=(.35+.6*turbulence)*sin(q.x*.13+t*.12+z*1.7);
  q.x+=(.12+.25*turbulence)*sin(q.y*.17-t*.10+z);
  q.y+=audio*.18*sin(q.x*.18+z);
  float2 cell=floor(q);
  [unroll]for(int y=-1;y<=1;y++)[unroll]for(int x=-1;x<=1;x++) {
   float2 id=cell+float2(x,y);
   float h=hash(float3(id,z*19.+seed));
   float2 center=id+.18+.64*float2(hash(float3(id,41.+z)),hash(float3(id,79.+z)));
   float2 delta=q-center;
   delta.x*=.78;
   // Near organisms are larger and softer; the distant population stays fine.
   float radius=lerp(.15,.055,saturate((detail+z*.45)*.4));
   float footprint=(13.+z*8.)/resolution.y;
   float width2=max(radius*radius,footprint*footprint*.3);
   float profile=exp(-dot(delta,delta)/width2);
   float presence=1-smoothstep(concentration-.035,concentration+.035,h);
   float pulse=.78+.22*sin(t*(.29+h*.32)+h*19.);
   float intensity=(.7+.7*h)*(1-z*.16)*(1+audio*(.5+z*.1));
   c+=driftTint(hash(float3(id,131.+z)))*profile*presence*pulse*intensity;
  }
 }
 float haze=noise(float3(p*2.+float2(t*.02,0),3));
 c+=driftTint(.15)*haze*.012;
 return float4(tone(c),1);
}
