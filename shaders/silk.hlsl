#include "common.hlsli"

// Layered translucent bands, following Liquid Waves' low-opacity fills and
// illuminated upper contours. All compositing happens in linear scene colour.
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET {
 float aspect=resolution.x/resolution.y;
 float2 p=(uv-.5)*float2(aspect,1);
 float t=time*movement*(.25+.75*flow);
 float moving=saturate(movement*100);
 float audio=moving*reaction*saturate(energy*.65+bass*.25+mid*.1);
 float quality=saturate(spare0.x*.5);
 float layerCount=3+floor(density*2.5);
 float3 c=float3(.007,.010,.022);
 c+=lerp(colorA(),colorB(),.5)*.014*exp(-dot(p,p)*2.5);
 [loop]for(int i=0;i<9;i++){
  if(i>=layerCount)break;
  float f=float(i),id=f/max(1,layerCount-1);
  float phase=t*(i%2?-.20:.18)+f*.83;
  // Analytic, unbounded horizontal curves: there are no finite mesh ends or
  // UV wrap boundaries which could leave rectangles in the desktop corners.
  float x=p.x+.055*turbulence*sin(p.x*2.5-phase*.7+f);
  float broad=sin(x*(2.1+id*.8)-phase);
  float roll=sin(x*4.3+phase*.72+f)*.36;
  float fine=sin(x*9.5+phase*.41+f*1.7)*(.008+.012*quality)*detail*turbulence;
  float center=(id-.5)*.45+(.10+.055*turbulence+audio*.045)*(broad+roll)+fine;
  float halfWidth=(.030+.013*density+audio*.018)*(.8+.2*sin(x*2.5-phase+f));
  float v=(p.y-center)/halfWidth;
  float aa=max(fwidth(v),.008);
  float coverage=1-smoothstep(1-aa,1+aa,abs(v));
  float roundness=sqrt(saturate(1-v*v));
  float longitudinal=.27+.73*pow(.5+.5*sin(x*.8+f*.67),.7);
  float opacity=coverage*(.20+.24*roundness)*longitudinal;
  float3 base=lerp(colorA(),colorB(),id);
  float3 satin=base*(.52+1.25*roundness+.26*v);
  satin+=lerp(base,float3(.65,.78,1),.35)*pow(saturate(roundness*.8-v*.45),18)*.75;
  c=lerp(c,satin,opacity);
  // A pixel-filtered crest remains fine at all resolutions and detail levels.
  float crest=1-smoothstep(aa*.45,aa*1.6,abs(v+.92));
  c+=lerp(base,float3(.6,.72,.9),.35)*crest*(.055+.045*detail)*longitudinal;
 }
 return float4(tone(c),1);
}
