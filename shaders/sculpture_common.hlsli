#include "common.hlsli"

// Bounded, asset-free sculptures. The camera is fitted to the shorter display
// dimension, so portrait and ultrawide displays retain the same clear margins.
float2 sculptureRotate(float2 p,float angle){float s=sin(angle),c=cos(angle);return float2(c*p.x-s*p.y,s*p.x+c*p.y);}
float sculptureClock(){return time*movement*(.35+.65*flow);}
float sculptureAudio(){return saturate(energy*.55+bass*.3+mid*.1+treble*.05)*reaction*min(movement,1);}
float sculptureSmoothMin(float a,float b,float k){float h=saturate(.5+.5*(b-a)/k);return lerp(b,a,h)-k*h*(1-h);}

float sculptureField(float3 p){
 float t=sculptureClock(),audio=sculptureAudio();
#if SCULPTURE_LATTICE
 p.xz=sculptureRotate(p.xz,t*.11);p.yz=sculptureRotate(p.yz,.32+.12*sin(t*.13));
 float frequency=3.1+detail*.65;
 float3 q=p*frequency;
 q+=.12*turbulence*sin(q.yzx*.7+t*.17);
 float gyroid=dot(sin(q),cos(q.zxy));
 float thickness=.16+.11*density+audio*.10;
 // The conservative step compensates for the sinusoidal warp and gradient.
 return max((abs(gyroid)-thickness)/(frequency*2.4),length(p)-1.18);
#else
 p.xz=sculptureRotate(p.xz,t*.075);
 float distortion=.018*turbulence;
 p+=distortion*sin(p.yzx*(3+detail*1.5)+t*.19);
 float3 a=float3(-.48+.12*sin(t*.32),-.12+.14*cos(t*.23),.02);
 float3 b=float3(.46,.11*sin(t*.29),.12*cos(t*.19));
 float3 c=float3(.02,.58+.12*cos(t*.27),-.1);
 float radius=.58+.055*density+audio*.055;
 float s=sculptureSmoothMin(length(p-a)-radius,length(p-b)-radius*.94,.38+.055*density);
 return sculptureSmoothMin(s,length(p-c)-(.25+.085*density),.31);
#endif
}

float3 sculptureNormal(float3 p,float epsilon){
#if SCULPTURE_LATTICE
 float2 e=float2(epsilon,0);
 return normalize(float3(sculptureField(p+e.xyy)-sculptureField(p-e.xyy),sculptureField(p+e.yxy)-sculptureField(p-e.yxy),sculptureField(p+e.yyx)-sculptureField(p-e.yyx)));
#else
 // Tetrahedral differences need four field evaluations instead of six.
 // Keep the normal budget bounded as lighting becomes more expressive.
 float2 e=float2(1,-1)*epsilon;
 return normalize(e.xyy*sculptureField(p+e.xyy)+e.yyx*sculptureField(p+e.yyx)
                 +e.yxy*sculptureField(p+e.yxy)+e.xxx*sculptureField(p+e.xxx));
#endif
}
float3 sculptureBackground(float2 p){
 float glow=exp(-dot(p,p)*5);
 return float3(.004,.006,.012)+lerp(colorA(),colorB(),.25)*(.009+.018*glow);
}
float3 sculptureEnvironment(float3 r){
 float3 a=colorA(),b=colorB();
 float3 c=lerp(float3(.005,.009,.018),float3(.12,.17,.24),smoothstep(-.55,.85,r.y));
 // Broad rectangular studio cards create readable reflections through the
 // entire merge, rather than isolated pin highlights that look like plastic.
 float2 keyAxis=float2((r.x+.42)/(.24-.035*min(detail,2)),(r.y-.38)/.72);
 float2 keySquared=keyAxis*keyAxis;
 float key=exp(-dot(keySquared,keySquared));
 float strip=exp(-pow((r.x-.58)/.075,2))*smoothstep(-.7,-.35,r.y)*(1-smoothstep(.65,.9,r.y));
 float warm=pow(saturate(dot(r,normalize(float3(.75,-.2,-.6)))),18);
 c+=float3(1.1,1.18,1.3)*key*1.8;
 c+=lerp(float3(.28,.65,.95),a,.35)*strip*1.35;
 c+=lerp(float3(.95,.46,.19),b,.45)*warm*.8;
 return c;
}

float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET{
 float shortSide=max(1,min(resolution.x,resolution.y));
 float2 p=(uv-.5)*resolution/shortSide;p.y=-p.y;
 float3 result=sculptureBackground(p);
 float3 ro=float3(0,0,4.8),rd=normalize(float3(p*2.8,-3.2));
 // Restrict marching to the sphere containing every possible parameter state.
 const float bound=1.5;
 float b=dot(ro,rd),discriminant=b*b-dot(ro,ro)+bound*bound;
 if(discriminant<=0)return float4(tone(result),1);
 float root=sqrt(discriminant),distance=max(0,-b-root),end=-b+root;
 float quality=clamp(spare0.x,0,2);
 int steps=quality<.5?64:quality<1.5?88:120;
 float tolerance=max(.00065,(1.6-quality*.45)/shortSide);
 bool hit=false;float nearest=1;
 [loop]for(int i=0;i<120;i++){
  if(i>=steps||distance>end)break;
  float s=sculptureField(ro+rd*distance);nearest=s;
  if(s<tolerance){hit=true;break;}
  distance+=max(s*.82,tolerance*.25);
 }
 if(hit){
  float3 pos=ro+rd*distance,n=sculptureNormal(pos,max(.001,tolerance));
  float3 light=normalize(float3(-.6,.8,1));
  float diffuse=saturate(dot(n,light)),fresnel=pow(1-saturate(dot(n,-rd)),SCULPTURE_LATTICE?3:5);
  float audio=sculptureAudio();
#if SCULPTURE_LATTICE
  float3 base=lerp(colorA(),colorB(),.13+.12*pos.y);
  // A stronger key and restrained fill distinguish inner walls without
  // spending extra distance-field evaluations on ambient occlusion.
  result=base*(.035+.65*diffuse)+lerp(float3(.32,.44,.50),colorA(),.55)*pow(saturate(dot(reflect(-light,n),-rd)),24+detail*12)*.8;
  result+=colorA()*(.10+.12*audio)*fresnel;
  // A second, broad fill reveals the inside walls without expensive shadows.
  result+=base*.065*saturate(dot(n,normalize(float3(.7,-.3,.2))));
#else
  result=sculptureEnvironment(reflect(rd,n))*(.82+.18*fresnel);
  result*=lerp(float3(.8,.88,.97),.4+colorA()*.65,.2);
  result+=colorB()*audio*.10*(.2+fresnel);
#endif
 }
 return float4(tone(result),1);
}
