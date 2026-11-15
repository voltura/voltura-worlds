#include "common.hlsli"
#include "swedish_christmas_common.hlsli"
Texture2D<float4> nightArtwork : register(t0);
Texture2D<float4> dayArtwork : register(t1);
SamplerState linearClamp : register(s0);

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

float3 glitterSnow(float3 color,float2 p,float t,float pixelSize,float daylight) {
 [branch]if(p.y<.77||p.y>.94)return color;
 [unroll]for(int i=0;i<10;i++){
  float strength=pow(max(0.,sin(t*(.78+i*.057)+i*3.7)),10.);
  float size=.0030*(i%3==0?1.25:1.);
  float2 delta=(p-snowGlints[i])*artworkMetric;
  float2 quadrant=abs(delta);
  [branch]if(strength>.1&&all(quadrant<size*1.85)){
   float warm=smoothstep(.4,.8,p.x)*(1.-daylight);
   float3 reflection=lerp(float3(.66,.80,1.),float3(1.,.85,.62),warm);
   float core=1.-smoothstep(size*.15,min(size*.77,size*.25+pixelSize),length(delta));
   color+=reflection*strength*(.52*core+.13*radialGlow(delta,size*1.5));
  }
 }
 return color;
}

float4 PS(float4 pixel : SV_POSITION,float2 uv : TEXCOORD) : SV_TARGET {
 float2 cover=float2(resolution.x/resolution.y/artworkAspect,1.);
 cover/=max(1.,cover.x);
 float2 p=(uv-.5)*cover+float2(lerp(.5,.66,1.-cover.x),.5);
 float t=time*movement;
 float daylight=christmasDaylight(time);
 float twilight=4.*daylight*(1.-daylight);
 float3 night=nightArtwork.Sample(linearClamp,p).rgb;
 float3 day=dayArtwork.Sample(linearClamp,p).rgb;
 float3 color=lerp(night,day,daylight);
 color+=twilight*day*float3(.19,.055,.015)*(1.-smoothstep(.1,.75,p.y));

 float pixelSize=.5*cover.y/resolution.y;
 color=chimneySmoke(color,p,t,daylight);
 color=twinkleTree(color,p,t,pixelSize);
 color=glitterSnow(color,p,t,pixelSize,daylight);
 return float4(tone(color),1.);
}
