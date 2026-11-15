#include "common.hlsli"

struct SnowSprite {
 float4 position : SV_POSITION;
 float2 local : TEXCOORD0;
 nointerpolation float4 shape : TEXCOORD1;
 nointerpolation float4 light : TEXCOORD2;
};

static const float2 corners[]={
 float2(-1,-1),float2(1,-1),float2(-1,1),
 float2(-1,1),float2(1,-1),float2(1,1)
};

SnowSprite VS(uint vertex : SV_VertexID,uint id : SV_InstanceID) {
 float depth=pass*.5;
 float aspect=resolution.x/resolution.y;
 float t=time*movement;
 float3 key=float3(id,pass,17.);
 float size=hash(key);
 float phase=6.2831853*hash(key+1.);
 float fallSpeed=lerp(.017,.062,depth)*lerp(.66,1.55,hash(key+2.));
 float flutter=sin(t*lerp(1.7,3.8,hash(key+3.))+phase);
 float travel=hash(key+4.)+t*fallSpeed/1.20;
 float y=frac(travel)*1.20-.10+flutter*lerp(.002,.009,depth);

 // Restart above the frame with a fresh horizontal position after each descent.
 // Some flakes share a broad band of air, leaving shifting gaps between flurries.
 float cycle=floor(travel);
 float spawn=hash(float3(id,pass,cycle+43.));
 float band=floor(hash(key+5.)*5.);
 float bandCenter=hash(float3(band,pass,71.))+t*.009;
 float clustered=frac(bandCenter+(spawn-.5)*.22);
 float origin=hash(key+6.)<.30?clustered:spawn;

 // Travelling gusts bend nearby paths together. Individual flakes flutter
 // through the flow and react more or less strongly depending on their depth.
 float windPhase=t*.79+3.7*y-3.1*origin;
 float wind=.042*sin(windPhase)+.047*sin(t*.36-2.9*y+2.4*origin);
 wind+=.013*sin(2.*windPhase+.8);
 float drift=t*lerp(.004,.013,depth)+wind*lerp(.52,1.45,depth);
 drift+=lerp(.003,.019,depth)*sin(t*1.37+phase);
 float x=frac(origin+drift/(aspect+.30))*(aspect+.30)-.15;

 float radius=lerp(.00090,.0047,depth*depth)*lerp(.66,1.62,size);
 float softness=lerp(.18,.48,depth*hash(key+7.));
 float aa=.65/(radius*resolution.y);
 float extent=radius*(1.65+softness+aa);
 float angle=phase+t*lerp(.25,1.7,hash(key+8.));
 float s,c;
 sincos(angle,s,c);
 float2 corner=corners[vertex];
 float2 uv=float2((x+corner.x*extent)/aspect,y+corner.y*extent);

 float daylight=smoothstep(0.,900.,time);
 float2 cover=float2(aspect/(1672./941.),1.);
 cover/=max(1.,cover.x);
 float2 p=(float2(x/aspect,y)-.5)*cover+float2(lerp(.5,.66,1.-cover.x),.5);
 float windowGlow=1.-smoothstep(.035,.24,length((p-float2(.64,.65))*float2(1.78,1.)));
 float3 snowLight=lerp(float3(.63,.76,.93),float3(1.,.78,.55),windowGlow*(1.-daylight)*.70);
 snowLight=lerp(snowLight,float3(.82,.88,.97),daylight);
 float opacity=lerp(.39,.70,depth)*lerp(.55,1.,hash(key+9.));

 SnowSprite sprite;
 sprite.position=float4(2.*uv.x-1.,1.-2.*uv.y,0.,1.);
 sprite.local=float2(dot(corner,float2(c,s)),dot(corner,float2(-s,c)))*(extent/radius);
 sprite.shape=float4(lerp(.58,1.,.5+.5*flutter),softness,aa,hash(key+10.));
 sprite.light=float4(tone(snowLight),opacity);
 return sprite;
}

float4 PS(SnowSprite sprite) : SV_TARGET {
 // Uneven lobes give nearby clumps an irregular silhouette as they tumble.
 float2 p=sprite.local;
 p.x/=sprite.shape.x;
 float d=length(p)-.72;
 d=min(d,length(p-float2(.44,.25))-.42);
 d=min(d,length(p-float2(-.37,lerp(-.53,-.20,sprite.shape.w)))-.38);
 float edge=sprite.shape.y+sprite.shape.z;
 float coverage=1.-smoothstep(-edge,edge,d);
 return float4(sprite.light.rgb,coverage*sprite.light.a);
}
