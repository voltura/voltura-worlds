#include "common.hlsli"

float3 eclipseStars(float2 uv,float amount){
 float2 p=uv*float2(resolution.x/resolution.y,1)*150;
 float2 cell=floor(p),local=frac(p);
 float id=hash(float3(cell,31));
 float2 center=.25+.5*float2(hash(float3(cell,17)),hash(float3(cell,79)));
 float2 pixels=(local-center)*(resolution.y/150);
 float radius=max(.42,.52*resolution.y/1080);
 float profile=exp(-dot(pixels,pixels)/(radius*radius));
 float3 tint=lerp(float3(.52,.67,1),float3(1,.77,.43),hash(float3(cell,107)));
 return tint*profile*step(1-amount*.007,id)*(.06+.17*hash(float3(cell,139)));
}

float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET {
 float aspect=resolution.x/resolution.y;
 float2 p=(uv-.5)*float2(aspect,1);
 float t=time*movement*(.2+.8*flow);
 float audio=saturate(movement*100)*reaction*saturate(bass*.55+energy*.35+treble*.1);
 float quality=saturate(spare0.x*.5);
 // Fit the entire silhouette on narrow displays as well as landscape screens.
 float fit=min(1,aspect/.9);
 float radius=(.245+.022*density+audio*.008)*fit;
 float2 q=p-float2(.035*sin(t*.035),.015*cos(t*.029))*fit;
 float r=length(q),a=atan2(q.y,q.x);
 float edge=r-radius;
 float aa=max(fwidth(r),1/resolution.y);
 // Integer angular harmonics meet exactly at atan2's -pi/+pi seam.
 float rays=1+.13*turbulence*sin(a*13+t*.16)+.075*turbulence*sin(a*29-t*.23);
 rays+=(.02+.03*quality)*detail*turbulence*sin(a*53+t*.31);
 float plume=.8+.2*sin(a*7-t*.09);
 float outside=max(0,edge);
 float envelope=exp(-outside/(fit*(.034+.013*density)));
 float corona=envelope*max(.1,rays)*(.52+.16*plume+audio*.35);
 float rim=exp(-outside/max(.001,fit*.007));
 float3 warm=float3(1,.39,.075),hot=float3(1,.72,.34);
 if(palette>.5&&palette<1.5){warm=float3(.035,.58,.45);hot=float3(.44,1,.85);}
 else if(palette>=1.5&&palette<2.5){warm=float3(.75,.12,.025);hot=float3(1,.55,.13);}
 else if(palette>=2.5){warm=float3(.34,.4,.53);hot=float3(.76,.86,1);}
 float3 sky=float3(.003,.005,.011);
 sky+=eclipseStars(uv,(.5+.5*detail)*(.75+.25*quality)*(.55+.45*density));
 float3 c=sky+warm*corona*.55+hot*rim*.9;
 float disk=1-smoothstep(-aa,aa,edge);
 // The opaque body masks the corona and stars without a hard raster edge.
 float sphere=sqrt(saturate(1-r*r/max(.0001,radius*radius)));
 float3 body=float3(.002,.003,.006)+float3(.004,.006,.011)*sphere;
 c=lerp(c,body,disk);
 return float4(tone(c),1);
}
