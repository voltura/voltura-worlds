#include "common.hlsli"
Texture2D<float4> nightArtwork : register(t0);
Texture2D<float4> dayArtwork : register(t1);
SamplerState linearClamp : register(s0);

float flakes(float2 uv, float t, float scale, float speed, float salt) {
 float2 gridPosition=float2(uv.x*resolution.x/resolution.y,uv.y)*scale;
 gridPosition.x+=sin(t*.31+gridPosition.y*.22+salt)*.38+t*.07;
 gridPosition.y-=t*speed;
 float2 cell=floor(gridPosition);
 float2 offset=float2(hash(float3(cell,salt)),hash(float3(cell,salt+1.)));
 float radius=lerp(.012,.043,hash(float3(cell,salt+3.)));
 return 1.-smoothstep(radius,radius+.02,length(frac(gridPosition)-lerp(.2,.8,offset)));
}

// Positions and animation timings follow the Julkalender artwork overlays.
static const float artworkAspect=1672./941.;
static const float2 artworkMetric=float2(artworkAspect,1.);
static const float2 chimney=float2(.652,.327);
static const float3 lamps[]={
 float3(.861,.491,0),float3(.850,.532,1),float3(.886,.531,2),
 float3(.851,.592,1),float3(.897,.581,0),float3(.870,.645,2),
 float3(.835,.666,0),float3(.880,.686,1),float3(.922,.699,0),
 float3(.833,.738,2),float3(.881,.745,0)
};
static const float3 lampColors[]={float3(1.,.08,.03),float3(.08,.40,1.),float3(.04,.89,.28)};
static const float2 snowGlints[]={
 float2(.16,.82),float2(.26,.91),float2(.35,.88),float2(.47,.84),float2(.57,.79),
 float2(.63,.92),float2(.68,.84),float2(.73,.86),float2(.81,.81),float2(.91,.90)
};

float radialGlow(float2 offset,float radius) {
 return saturate(1.-length(offset)/radius);
}

float3 chimneySmoke(float3 color,float2 p,float t,float daylight) {
 // Limit the puff work to the chimney and the sky immediately above it.
 [branch]if(any(p<float2(.630,.115))||any(p>float2(.724,.354)))return color;
 float2 position=(p-chimney)*artworkMetric;
 float transmission=1.;
 [unroll]for(int i=0;i<9;i++){
  float age=frac((t+i)/9.);
  float2 center=artworkAspect*float2(
   .04*age+sin(age*8.-t*.58)*.007*age,
   -.08*age-.014*age*age);
  float spread=artworkAspect*(.004+.012*age);
  float opacity=lerp(.25,.32,daylight)*sin(3.141593*age)*(1.-.46*age);
  float2 delta=position-center;
  transmission*=1.-opacity*radialGlow(delta,spread);
  transmission*=1.-opacity*.60*radialGlow(delta-spread*float2(-.52,.37),spread*.72);
  transmission*=1.-opacity*.54*radialGlow(delta-spread*float2(.43,-.48),spread*.85);
 }
 float3 smokeColor=lerp(float3(.58,.69,.86),float3(.93,.85,.72),daylight);
 return lerp(smokeColor,color,transmission);
}

float3 twinkleTree(float3 color,float2 p,float t,float pixelSize) {
 const float interval=1.65;
 float progress=frac(t/interval)*interval/.85;
 [branch]if(progress>=1.||any(p<float2(.81,.47))||any(p>float2(.94,.77)))return color;
 uint slot=(uint)floor(t/interval);
 float3 lamp=lamps[(slot*7u)%11u];
 float2 delta=(p-lamp.xy)*artworkMetric;
 float pulse=sin(3.141593*progress);
 float radius=artworkAspect*.0038;
 color=lerp(color,lampColors[(uint)lamp.z],.82*pulse*radialGlow(delta,radius*2.));
 color=lerp(color,float3(1.,.96,.80),pulse*radialGlow(delta,radius*1.35));
 float core=1.-smoothstep(-pixelSize,pixelSize,length(delta)-radius*.53);
 return lerp(color,float3(1.,.98,.88),.88*pulse*core);
}

float3 glitterSnow(float3 color,float2 p,float t,float pixelSize) {
 [branch]if(p.y<.77||p.y>.94)return color;
 [unroll]for(int i=0;i<10;i++){
  float strength=pow(max(0.,sin(t*(1.05+i*.026)+i*3.7)),5.);
  float size=.0085*(i%3==0?1.25:1.);
  float2 delta=(p-snowGlints[i])*artworkMetric;
  float2 quadrant=abs(delta);
  [branch]if(strength>.1&&all(quadrant<size*1.85)){
   color=lerp(color,float3(1.,.79,.38),strength*.65*radialGlow(delta,size*1.85));
   // The two edges in each quadrant meet at the notch of the four-point star.
   float verticalEdge=dot(quadrant,float2(.83,.19))-.19*size;
   float horizontalEdge=dot(quadrant,float2(.17,.58))-.1309*size;
   float star=1.-smoothstep(-pixelSize,pixelSize,min(verticalEdge/.851,horizontalEdge/.604));
   color=lerp(color,float3(1.,.96,.80),strength*star);
  }
 }
 return color;
}

float4 PS(float4 pixel : SV_POSITION,float2 uv : TEXCOORD) : SV_TARGET {
 float2 cover=float2(resolution.x/resolution.y/artworkAspect,1.);
 cover/=max(1.,cover.x);
 float2 p=(uv-.5)*cover+float2(lerp(.5,.66,1.-cover.x),.5);
 float t=time*movement;
 float daylight=smoothstep(0.,900.,time);
 float dawn=4.*daylight*(1.-daylight);
 float3 night=nightArtwork.Sample(linearClamp,p).rgb;
 float3 day=dayArtwork.Sample(linearClamp,p).rgb;
 float3 color=lerp(night,day,daylight);
 color+=dawn*day*float3(.19,.055,.015)*(1.-smoothstep(.1,.75,p.y));

 float pixelSize=.5*cover.y/resolution.y;
 color=chimneySmoke(color,p,t,daylight);
 color=twinkleTree(color,p,t,pixelSize);
 color=glitterSnow(color,p,t,pixelSize);
 float falling=flakes(uv,t,24.,.48,3.)*.32+flakes(uv,t,43.,.69,5.)*.13;
 color+=falling*float3(.83,.91,1.)*lerp(1.,.58,daylight);
 return float4(tone(color),1.);
}
