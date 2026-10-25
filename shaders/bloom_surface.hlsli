#include "common.hlsli"
struct BloomVertex {float4 position:SV_POSITION;float3 world:TEXCOORD0;float2 uv:TEXCOORD1;float3 normal:TEXCOORD2;};
// Restore the original continuous rolled sheet. Density gently changes the
// curl spacing; the default value of one preserves the original 8.7-radian roll.
float3 bloomSurface(float2 uv){
 float t=time*movement*.16*(.5+.5*flow);
 float a=uv.x*6.283185;
 float v=uv.y;
 float curl=v*(8.7+(density-1)*1.5)-.9;
 float pleat=sin(a*7+sin(t*.6)*.5);
 float ripple=sin(a*11+t*.7)*.025*detail;
 float roll=.16+.43*v;
 float radius=.82+roll*cos(curl)+pleat*.17*(.65+.35*turbulence)+ripple;
 float3 p=float3(radius*cos(a),roll*sin(curl),radius*sin(a));
 p.y+=sin(a*7+sin(t*.6)*.5)*.20;
 p.y+=sin(a*3+t)*.045;
 p*=.90*(1+sin(t*.8)*.025+energy*reaction*step(.0001,movement)*.045);
 float tilt=.82+sin(t*.47)*.065;
 p.yz=float2(p.y*cos(tilt)-p.z*sin(tilt),p.y*sin(tilt)+p.z*cos(tilt));
 float turn=.20+sin(t*.34)*.15;
 p.xz=float2(p.x*cos(turn)+p.z*sin(turn),-p.x*sin(turn)+p.z*cos(turn));
 float slant=-.32;
 p.xy=float2(p.x*cos(slant)-p.y*sin(slant),p.x*sin(slant)+p.y*cos(slant));
 return p;
}
