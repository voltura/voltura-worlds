#include "common.hlsli"
#include "ripples_surface.hlsli"
Texture2D<float4> forestReflection:register(t0);
SamplerState linearClamp:register(s0);

void forestWavePacket(inout WaterField field,float2 p,float2 direction,float amplitude,float speed,
                      float t,float offset,float3 patch,float3 curl) {
 // Spatial envelopes carry their own gradient. Flat patches remain genuinely
 // calm; the curved crests strengthen only where a local breeze passes.
 float u=saturate((patch.x+.08)/.36);
 float envelope=u*u*(3.-2.*u);
 float2 envelopeGradient=6.*u*(1.-u)/.36*patch.yz;
 float phase=dot(p,direction)+8.*curl.x-t*speed+offset;
 float2 phaseGradient=direction+8.*curl.yz;
 field.height+=amplitude*envelope*sin(phase);
 field.slope+=amplitude*(envelope*cos(phase)*phaseGradient+sin(phase)*envelopeGradient);
}

// One surface evaluator drives both reflected light and floating leaves.
WaterField forestSurface(float2 p,float t) {
 WaterField field=(WaterField)0;
 // Two independent, slowly advecting breeze patches replace the continuous
 // crosshatch of equal-strength waves across the whole pond.
 float windTime=t*(.65+.35*flow);
 float3 gust=waterGust(p*.73-float2(.031,.017)*windTime+7.3);gust.yz*=.73;
 float3 gust2=waterGust(p*1.17+float2(.019,-.026)*windTime+21.7);gust2.yz*=1.17;
 float3 curl=waterGust(p*1.9+float2(.012,-.015)*windTime+13.1);curl.yz*=1.9;
 waterWind(field,p,float2(4.,2.1),.00065*turbulence,-windTime*.31);
 forestWavePacket(field,p,float2(19,9),.00058*turbulence,.92,windTime,3.7,gust,curl);
 forestWavePacket(field,p,float2(36,17),.00029*turbulence,1.35,windTime,8.2,gust,curl);
 forestWavePacket(field,p,float2(-17,43),.00012*turbulence,1.8,windTime,17.3,gust2,curl);
 if(spare0.x>.5)forestWavePacket(field,p,float2(79,-24),.000065*turbulence,2.2,windTime,19.3,gust2,curl);
 // Impacts are independent of leaf density. Events fade completely
 // before renewal, so changing the next impact location cannot pop a wave.
 [unroll]for(int i=0;i<4;i++) {
  float period=14.+hash(float3(i,3.,41.))*17.;
  float clock=t*.72+hash(float3(i,9.,63.))*period+11.;
  float cycle=floor(clock/period);
  float age=clock-cycle*period;
  float2 center=float2(hash(float3(cycle,i,17.))-.5,hash(float3(cycle,i,37.)))*float2(8.,6.)+float2(0,1.);
  float amplitude=.00017+.00018*hash(float3(cycle,i,97.));
  waterDrop(field,p-center,age,amplitude,.75,103.+detail*26.);
 }
 float audio=saturate(reaction*(energy*.5+bass*.35+treble*.15))*step(.0001,movement);
 field.height*=.50*(1.+audio*.3);
 field.slope*=.50*(1.+audio*.3);
 return field;
}

// Photographic environment, sampled in angular space by the water's reflected
// ray. The forest remains stationary; continuous surface slopes distort it.
float3 forestEnvironment(float3 direction) {
 float azimuth=atan2(direction.x,direction.y);
 float elevation=atan2(max(direction.z,0.),length(direction.xy));
 float2 uv=float2(.5+azimuth/2.3,1.-elevation/1.1);
 float3 c=forestReflection.Sample(linearClamp,saturate(uv)).rgb;
 float luminance=dot(c,float3(.2126,.7152,.0722));
 float green=saturate((c.g-c.b)*9.)*saturate((c.g-c.r*.75)*9.);
 if(palette>.5&&palette<1.5)c=lerp(c,c*float3(.78,1.08,.74),green*.65);
 else if(palette>1.5&&palette<2.5)c=lerp(c,c*float3(1.55,.82,.42),green*.8);
 else if(palette>2.5)c=lerp(c,luminance*float3(.93,1.,1.03),.55)*.83;
 return c;
}

float3 pondLeaves(float3 color,float3 camera,float3 ray,float2 p,float t) {
 // Trace back the shared slow current before choosing persistent leaf IDs.
 // The lattice translates continuously; leaves never wrap or respawn at cells.
 float windTime=t*(.65+.35*flow);
 float2 current=float2(.0031,.0017)*windTime;
 float2 cells=(p-current)*2.1;
 float footprint=max(length(ddx(cells)),length(ddy(cells)));
 float2 cell=floor(cells);
 [unroll]for(int y=-1;y<=1;y++)[unroll]for(int x=-1;x<=1;x++) {
  float2 id=cell+float2(x,y);
  float r=hash(float3(id,91.));
  if(r>.17*density)continue;
  float r2=hash(float3(id,31.));
  float2 center=id+float2(.2+.6*r2,.2+.6*hash(float3(id,53.)));
  // Reject distant leaves before sampling their water surface. Bounds include
  // the leaf's extent, wave-driven displacement and raised-plane parallax.
  if(dot(cells-center,cells-center)>.49)continue;
  float2 anchor=center/2.1+current;
  WaterField local=forestSurface(anchor,t);
  // A restrained visual response makes small capillary waves legible on the
  // small leaves: their phase, bob and rocking all come from the same field.
  float2 displacement=-local.slope*1.7;
  anchor+=displacement;
  float height=local.height*4.;
  float2 tilt=local.slope*4.;
  float angle=r*93.+dot(local.slope,float2(9.,-13.));
  float3 leafNormal=normalize(float3(-tilt,1));
  float2 heading=float2(cos(angle),sin(angle));
  float3 axisX=normalize(float3(heading,dot(tilt,heading)));
  float3 axisY=cross(leafNormal,axisX);
  float distance=(height-camera.z+dot(tilt,camera.xy-anchor))/min(-.03,ray.z-dot(tilt,ray.xy));
  float3 hit=camera+ray*distance-float3(anchor,height);
  float size=(.065+.06*r2)/2.1;
  float2 q=float2(dot(hit,axisX),dot(hit,axisY))/size;
  if(dot(q,q)>6.)continue;
  float outline=abs(q.x)/.48+q.y*q.y;
  outline+=sin(q.y*31.)*.035;
  float aa=clamp(footprint/(size*2.1)*2.,.025,.35);
  float mask=1.-smoothstep(1.-aa,1.+aa,outline);
  float shadow=1.-smoothstep(.8,1.8,abs(q.x-.2)/.55+(q.y+.12)*(q.y+.12));
  color*=1.-shadow*.16;
  float3 leafColor=lerp(float3(.20,.12,.035),float3(.61,.43,.13),r2);
  if(palette> .5&&palette<1.5)leafColor=lerp(leafColor,float3(.20,.28,.058),.7);
  float vein=1.-smoothstep(.015,.05,abs(q.x));
  float veins=pow(saturate(cos((q.y-abs(q.x)*.6)*30.)),14.);
  leafColor*=.82+.18*noise(float3(q*11.,r*20.));
  leafColor+=float3(.11,.085,.025)*(vein+veins*.3);
  // Rocking changes exposure and the small contact highlight with the normal.
  leafColor*=.74+.34*saturate(dot(leafNormal,normalize(float3(-.35,.4,1.))));
  float wetEdge=pow(saturate(1.-abs(outline-.85)*7.),3.);
  leafColor+=float3(.035,.043,.035)*wetEdge*saturate(dot(leafNormal,-ray));
  color=lerp(color,leafColor,mask);
 }
 return color;
}

float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET {
 float2 screen=(pixel.xy-resolution*.5)/resolution.y;
 float3 camera=float3(0,-3.,3.);
 float3 forward=normalize(float3(0,4.,-3.));
 float3 up=normalize(cross(float3(1,0,0),forward));
 float3 ray=normalize(forward+screen.x*float3(1,0,0)*.78-screen.y*up*.78);
 float3 position=camera+ray*(-camera.z/min(ray.z,-.05));
 float t=time*movement;
 WaterField field=forestSurface(position.xy,t);
 float3 normal=normalize(float3(-field.slope,1));
 float3 reflected=reflect(ray,normal);
 float fresnel=.0204+.9796*pow(1.-saturate(dot(normal,-ray)),5.);
 float3 reflection=forestEnvironment(reflected);
 // Deep, tannin-darkened water: reflections dominate, with no sandy bottom.
 float3 water=float3(.017,.028,.019);
 float3 c=reflection*(.57+fresnel*.85)+water*(1.-fresnel);
 c=pondLeaves(c,camera,ray,position.xy,t);
 return float4(tone(c),1);
}
