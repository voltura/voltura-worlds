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

static const float3 lamps[]={
 float3(.861,.491,0),float3(.850,.532,1),float3(.886,.531,2),
 float3(.851,.592,1),float3(.897,.581,0),float3(.870,.645,2),
 float3(.835,.666,0),float3(.880,.686,1),float3(.922,.699,0),
 float3(.833,.738,2),float3(.881,.745,0)
};
static const float3 lampColors[]={float3(1.,.12,.045),float3(.08,.46,1.),float3(.06,1.,.36)};

float4 PS(float4 pixel : SV_POSITION,float2 uv : TEXCOORD) : SV_TARGET {
 float2 cover=float2(resolution.x/resolution.y/(1672./941.),1.);
 cover/=max(1.,cover.x);
 float2 p=(uv-.5)*cover+float2(lerp(.5,.66,1.-cover.x),.5);
 float t=time*movement;
 float daylight=smoothstep(0.,900.,time);
 float dawn=4.*daylight*(1.-daylight);
 float3 night=nightArtwork.Sample(linearClamp,p).rgb;
 float3 day=dayArtwork.Sample(linearClamp,p).rgb;
 float3 color=lerp(night,day,daylight);
 color+=dawn*day*float3(.19,.055,.015)*(1.-smoothstep(.1,.75,p.y));

 [unroll]for(int i=0;i<11;i++){
  float2 delta=(p-lamps[i].xy)*float2(1.777,1.);
  float glow=exp(-dot(delta,delta)*21000.);
  float shimmer=.36+.23*sin(t*1.8+i*4.3)+.1*sin(t*.64-i);
  color+=glow*lampColors[(int)lamps[i].z]*shimmer*lerp(.20,.10,daylight);
 }

 float2 glintGrid=p*float2(93.,48.);
 float2 cell=floor(glintGrid);
 float glint=1.-smoothstep(.012,.045,length(frac(glintGrid)-.5));
 float glintPhase=hash(float3(cell,13.));
 float pulse=pow(max(0.,sin(t*.95+glintPhase*38.)),18.);
 float snowGate=smoothstep(.74,.83,p.y)*smoothstep(.12,.36,min(color.r,min(color.g,color.b)));
 color+=snowGate*glint*pulse*.22;
 float falling=flakes(uv,t,24.,.48,3.)*.32+flakes(uv,t,43.,.69,5.)*.13;
 color+=falling*float3(.83,.91,1.)*lerp(1.,.58,daylight);
 return float4(tone(color),1.);
}
