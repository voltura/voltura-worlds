#include "common.hlsli"
Texture2D<float4> artwork:register(t0);
SamplerState linearClamp:register(s0);

float oval(float2 p,float2 center,float2 radius) {
 return 1.-smoothstep(.55,1.,length((p-center)/radius));
}
float3 photograph(float2 p) {return artwork.SampleLevel(linearClamp,p,0).rgb;}

float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET {
 // Cover without stretching. All motion is evaluated in original image space,
 // so the waterfall and banks stay registered on every display aspect ratio.
 float2 fit=float2(resolution.x/resolution.y/(16./9.),1.);
 fit/=max(1.,fit.x);
 float2 p=(uv-.5)*fit+.5;
 float t=time*movement;
 float audio=saturate(reaction*(energy*.6+bass*.4))*step(.0001,movement);
 float3 original=photograph(p);
 float green=smoothstep(.012,.085,original.g-max(original.r*.8,original.b));
 float foliage=green*(oval(p,float2(.10,.24),float2(.23,.40))
                      +oval(p,float2(.91,.26),float2(.22,.42))
                      +oval(p,float2(.91,.85),float2(.18,.22)));
 float gust=sin(t*.81+p.y*8.)*.65+sin(t*1.31+p.x*11.+p.y*6.)*.35;
 float2 leafOffset=float2(gust,sin(t*.73+p.x*12.)*.4)*.0017*density*foliage;

 // The colour gate keeps rocks and foreground leaves out of the pool field.
 float bankLeft=lerp(.16,.30,saturate((p.y-.64)/.36));
 float bankRight=lerp(1.,.73,saturate((p.y-.72)/.28));
 float pool=smoothstep(.632,.662,p.y)*smoothstep(bankLeft,bankLeft+.025,p.x)
            *(1.-smoothstep(bankRight-.025,bankRight,p.x));
 pool*=smoothstep(.005,.055,min(original.g,original.b)-original.r*.93);
 float depth=saturate((p.y-.63)/.37);
 float2 q=float2(p.x*9.,log(1.+depth*8.)*2.);
 float a=q.x*8.+q.y*11.-t*1.7;
 float b=q.x*-13.+q.y*17.-t*1.13;
 float2 ripple=float2(sin(a)+sin(b)*.45,cos(a*.83)+cos(b)*.5);
 float2 waterOffset=ripple*float2(.0017,.0025)*(.25+depth)*turbulence*(1.+audio*.3);

 // Restrained local displacement preserves the photographic waterfall outline.
 // Advected turbulence travels downwards continuously; no wrapping image strip.
 float fallRegion=smoothstep(.125,.16,p.y)*(1.-smoothstep(.59,.635,p.y))
                  *smoothstep(.528,.548,p.x)*(1.-smoothstep(.615,.634,p.x));
 float white=smoothstep(.12,.48,min(original.r,min(original.g,original.b)));
 float fall=fallRegion*white;
 float waterfallTime=t*(.7+flow*.8);
 float streams=noise(float3(p.x*560.,p.y*115.-waterfallTime*13.,3.));
 float fine=noise(float3(p.x*1100.,p.y*280.-waterfallTime*29.,9.));
 float2 fallOffset=float2(sin(p.y*110.-waterfallTime*10.)*.001,
                         (streams-.5)*.008)*fall;
 float3 c=photograph(p+leafOffset+waterOffset*pool+fallOffset);
 c*=1.+fall*((streams-.5)*.38+(fine-.5)*.17*detail);

 // Expanding elliptical disturbances originate where the falls meet the pool.
 float2 impact=(p-float2(.575,.637))*float2(1.,3.8);
 float radius=length(impact);
 float ring=sin(radius*210.-waterfallTime*5.5);
 float waves=ring*exp(-radius*9.)*pool;
 c+=float3(.032,.052,.047)*(waves*turbulence+pool*(sin(a+b)*.07)*detail);
 float mist=oval(p,float2(.574,.619),float2(.13,.028));
 float spray=noise(float3(p*float2(78.,180.)+float2(-t*.7,-t*1.3),5.));
 c=lerp(c,float3(.72,.83,.79),mist*(.025+.065*spray)*saturate(flow));
 if(palette>.5&&palette<1.5)c*=float3(1.09,1.02,.90);
 else if(palette>1.5&&palette<2.5)c*=float3(.90,1.035,1.10);
 else if(palette>2.5)c=lerp(c,dot(c,float3(.2126,.7152,.0722)).xxx,.20)*.88;
 // Compensate for the shared exponential display shoulder while retaining the
 // source photograph's exposure. Input is decoded from sRGB by the texture view.
 c=-log(max(1.-min(c,.98),.02));
 return float4(tone(c),1);
}
