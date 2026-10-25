#include "common.hlsli"
#include "orbit_rings.hlsli"
#include "orbit_terrain.hlsli"
Texture2D<float4> earthMap:register(t0);SamplerState planetSampler:register(s1);
Texture2D<float4> cloudMap:register(t1);
Texture2D<float4> nightMap:register(t2);
float2 cloudCoordinate(float2 coordinate){
 // Rotate the weather shell as a sphere, preserving the source storm shapes.
 float longitude=(coordinate.x-.27)*6.283185,latitude=coordinate.y*3.141593;
 float3 direction=float3(sin(latitude)*cos(longitude),cos(latitude),sin(latitude)*sin(longitude));
 float3 axis=float3(-.587785,0,.809017);
 float angle=.16;
 direction=direction*cos(angle)+cross(axis,direction)*sin(angle)+axis*dot(axis,direction)*(1-cos(angle));
 return float2(atan2(direction.z,direction.x)/6.283185,acos(clamp(direction.y,-1,1))/3.141593);
}
float2 cloudGradient(float2 gradient){gradient.x-=round(gradient.x);return gradient;}
float ringVisibility(float3 position,float3 light){
 // Debris travels within this fixed plane; the plane itself does not precess.
 float rotation=.25;
 float3 axis=float3(sin(.07)*sin(rotation),cos(.07),sin(.07)*cos(rotation));
 axis.xy=float2(axis.x*cos(.28)-axis.y*sin(.28),axis.x*sin(.28)+axis.y*cos(.28));
 float denominator=dot(axis,light);
 if(abs(denominator)<.001)return 1;
 float distance=-dot(position-float3(.72,.02,0),axis)/denominator;
 float3 crossing=position+light*max(distance,0)-float3(.72,.02,0);
 float radius=length(crossing),coordinate=(radius-1.48)/(2.35-1.48);
 float opticalDepth=ringOpticalDepth(coordinate);
 return distance>0?exp(-opticalDepth/max(abs(denominator),.03)):1;
}
float4 PS(float4 pixel:SV_POSITION,float3 world:TEXCOORD0,float3 normal:TEXCOORD1,float2 uv:TEXCOORD2):SV_TARGET {
 float3 n=normalize(normal),light=normalize(float3(1,.65,-.2)),view=normalize(float3(0,0,3.45)-world);
 float lit=saturate(dot(n,light)),rim=pow(1-saturate(dot(n,view)),3);
 if(pass>6.5){
  clip(dot(n,view));
  float2 cloudUV=cloudCoordinate(float2(uv.x+.16+time*.00015*movement,uv.y));
  float2 cloudDx=cloudGradient(ddx(cloudUV)),cloudDy=cloudGradient(ddy(cloudUV));
  float coverage=smoothstep(.045,.70,cloudMap.SampleGrad(planetSampler,cloudUV,cloudDx,cloudDy).r);
  float3 dx=ddx(world),dy=ddy(world),r1=cross(dy,n),r2=cross(n,dx);
  float determinant=dot(dx,r1);
  float inverse=sign(determinant)/(abs(determinant)+.000001);
  float2 sunUV=(dot(light,r1)*cloudDx+dot(light,r2)*cloudDy)*inverse;
  float2 footprintX=cloudDx*float2(8192,4096),footprintY=cloudDy*float2(8192,4096);
  float lod=max(0,.5*log2(max(dot(footprintX,footprintX),dot(footprintY,footprintY))));
  float2 texel=exp2(lod)/float2(8192,4096);
  float east=cloudMap.SampleLevel(planetSampler,cloudUV+float2(texel.x,0),lod).r;
  float west=cloudMap.SampleLevel(planetSampler,cloudUV-float2(texel.x,0),lod).r;
  float north=cloudMap.SampleLevel(planetSampler,cloudUV+float2(0,texel.y),lod).r;
  float south=cloudMap.SampleLevel(planetSampler,cloudUV-float2(0,texel.y),lod).r;
  float2 slopeUV=float2(east-west,north-south)/(2*texel);
  float3 slope=(r1*dot(slopeUV,cloudDx)+r2*dot(slopeUV,cloudDy))*inverse;
  float3 cloudNormal=normalize(n-slope*.003);
  float occlusion=0;
  [unroll]for(int sampleIndex=1;sampleIndex<=2;sampleIndex++){
   float neighbor=smoothstep(.045,.70,cloudMap.SampleLevel(planetSampler,cloudUV+sunUV*(sampleIndex*.004),lod).r);
   occlusion+=max(0,neighbor-coverage-.06*sampleIndex);
  }
  float horizonLight=smoothstep(-.02,.10,dot(n,light));
  float direct=saturate(dot(cloudNormal,light))*horizonLight*exp(-occlusion*3.5)*ringVisibility(world,light);
  float3 fillDirection=normalize(float3(-.6,.4,.8));
  float2 fillUV=(dot(fillDirection,r1)*cloudDx+dot(fillDirection,r2)*cloudDy)*inverse;
  float fillOcclusion=0;
  [unroll]for(int fillIndex=1;fillIndex<=3;fillIndex++){
   float neighbor=smoothstep(.045,.70,cloudMap.SampleLevel(planetSampler,cloudUV+fillUV*(fillIndex*.003),lod).r);
   fillOcclusion+=max(0,neighbor-coverage-.045*fillIndex);
  }
  float fill=saturate(dot(cloudNormal,fillDirection))*exp(-fillOcclusion*5);
  float3 cloudColor=float3(.003,.006,.010)+float3(.025,.042,.058)*fill;
  cloudColor+=float3(1,.75,.46)*direct*1.5;
  return float4(tone(cloudColor),coverage*.96);
 }
 if(pass>1.5&&pass<2.5){
  clip(dot(n,view));
  float3 origin=float3(0,0,3.45)-float3(.72,.02,0),ray=normalize(world-float3(0,0,3.45));
  float b=dot(origin,ray),c=dot(origin,origin),outer=b*b-c+1.365*1.365;
  float nearT=-b-sqrt(max(outer,0)),farT=-b+sqrt(max(outer,0));
  float inner=b*b-c+1.33*1.33;if(inner>0)farT=min(farT,-b-sqrt(inner));
  float stride=max(0,farT-nearT)/12;float3 scattering=0;float optical=0;
  [unroll]for(int j=0;j<12;j++){
   float3 position=origin+ray*(nearT+(j+.5)*stride);
   float height=max(0,length(position)-1.33);float air=exp(-height/.008);
   float horizon=sqrt(saturate(1-1.33*1.33/dot(position,position)));
   float sun=smoothstep(-.035,.035,dot(normalize(position),light)+horizon);
   float forwardRim=pow(saturate(dot(normalize(position),light)),5);
   float3 scatterColor=lerp(float3(.028,.15,.28),float3(1.25,.65,.22),forwardRim);
   scattering+=air*stride*sun*scatterColor*9;
   optical+=air*stride;
  }
  float alpha=saturate(optical*2.5);return float4(tone(scattering/max(alpha,.0001)),alpha);
 }
 if(pass>3.5&&pass<4.5){
  // Analytic crater slopes avoid screen-quad derivatives, whose shared
  // 2x2 gradients made the relief appear as square pixels near the limb.
  float4 largeCrater=craters(n*9),smallCrater=craters(n*29);
  float footprint=max(length(ddx(n)),length(ddy(n)));
  float smallWeight=1-smoothstep(.025,.06,footprint);
  float4 terrain=moonTerrain(n);
  float3 gradient=terrain.xyz+largeCrater.xyz*(9*.014)+smallCrater.xyz*(29*.0022)*smallWeight;
  float3 bump=normalize(n-(gradient-n*dot(n,gradient))/(1+terrain.w+largeCrater.w*.014));
  float mottling=fbm(n*13);
  float3 rock=lerp(float3(.065,.060,.052),float3(.18,.14,.10),mottling);
  float lightness=max(dot(bump,light),0);
  float facing=saturate(dot(n,light)*5);
  // Moons retain artistic focus; opaque HDR alpha stores near blur radius.
  return float4(tone(rock*(float3(1,.83,.63)*lightness*facing*1.65+float3(.012,.023,.04))),0);
 }
 if(pass>2.5){
  float3 geometric=cross(ddx(world),ddy(world));
  geometric*=dot(geometric,n)<0?-1:1;
  float3 rockNormal=normalize(geometric+1e-8*n);
  if(pass>5.5){
   float4 coarse=rockNoiseGradient(world*90),fine=rockNoiseGradient(world*270+17);
   float footprint=max(length(ddx(world)),length(ddy(world)));
   float coarseWeight=1-smoothstep(.35,1,footprint*90);
   float fineWeight=1-smoothstep(.35,1,footprint*270);
   float3 gradient=coarse.xyz*(.42*coarseWeight)+fine.xyz*(.16*fineWeight);
   rockNormal=normalize(n-(gradient-n*dot(n,gradient)));
  }
  lit=saturate(dot(rockNormal,light));
  float rough=noise(normal*17+world*13);float grain=noise(normal*63+world*21);
  float3 rock=lerp(float3(.025,.018,.013),float3(.24,.18,.12),rough)*(.45+grain*.55);
  if(pass>5.5)rock=lerp(float3(.018,.020,.024),float3(.14,.12,.10),rough)*(.62+grain*.38);
  if(pass>4.5&&pass<5.5)rock=lerp(float3(.07,.08,.10),float3(.48,.38,.24),rough);
  float focus=saturate(1.8*max(0,1/max(.01,3.45-world.z)-1/2.15));
  return float4(tone(rock*(lit*3+.018)),focus);
 }
 if(pass>.5){
  float3 rel=world-float3(.72,.02,0);
  float shadow=dot(rel,light)<0?lerp(.015,1,smoothstep(1.30,1.36,length(cross(rel,light)))):1;
  float opticalDepth=ringOpticalDepth(uv.y);
  float viewCos=dot(n,view),lightCos=dot(n,light);
  float muView=max(abs(viewCos),.025),muLight=max(abs(lightCos),.025);
  float alpha=1-exp(-opticalDepth/muView);
  float scattering=ringScattering(opticalDepth,muView,muLight,viewCos*lightCos>0);
  float phaseCos=dot(light,-view),g=.45;
  float phase=(1-g*g)/pow(max(.05,1+g*g-2*g*phaseCos),1.5);
  float material=ringNoise(uv.y,27,5);
  float3 albedo=lerp(float3(.23,.19,.14),float3(.76,.59,.37),material);
  float3 grainPosition=world*210;
  float grainFootprint=max(length(ddx(grainPosition)),length(ddy(grainPosition)));
  float dust=lerp(.5,noise(grainPosition),1-smoothstep(.35,1.2,grainFootprint));
  float3 radiance=albedo*(float3(1,.86,.65)*scattering*phase*1.1*shadow+float3(.025,.042,.065)*alpha);
  float sunward=smoothstep(-.4,.85,dot(normalize(rel),light));
  radiance*=(.85+.3*dust)*lerp(.35,1,sunward*sunward);
  // RGB is unpremultiplied for the shared alpha-blend pipeline.
  return float4(tone(radiance/max(alpha,.0001)),alpha);
 }
 float3 surface=earthMap.Sample(planetSampler,float2(uv.x+.16,uv.y)).rgb;
 float land=saturate((surface.r+surface.g-surface.b)*12);
 float3 relative=world-float3(.72,.02,0);
 float along=dot(relative,light);
 float cloudDistance=-along+sqrt(max(0,along*along+1.339*1.339-dot(relative,relative)));
 float3 shadowPoint=relative+light*cloudDistance;
 float roll=.28,rotation=.25+time*.007*movement;
 shadowPoint.xy=float2(shadowPoint.x*cos(roll)+shadowPoint.y*sin(roll),-shadowPoint.x*sin(roll)+shadowPoint.y*cos(roll));
 shadowPoint.xz=float2(shadowPoint.x*cos(rotation)-shadowPoint.z*sin(rotation),shadowPoint.x*sin(rotation)+shadowPoint.z*cos(rotation));
 shadowPoint=normalize(shadowPoint);
 float2 shadowUV=float2(atan2(shadowPoint.z,shadowPoint.x)/6.283185+.16+time*.00015*movement,acos(clamp(shadowPoint.y,-1,1))/3.141593);
 shadowUV=cloudCoordinate(shadowUV);
 float shadowCloud=smoothstep(.04,.7,cloudMap.SampleGrad(planetSampler,shadowUV,cloudGradient(ddx(shadowUV)),cloudGradient(ddy(shadowUV))).r);
 float3 base=surface*lerp(float3(.65,1.12,.65),float3(1,1,1),land);
 float ndl=dot(n,light);float day=smoothstep(-.06,.12,ndl);
 float spec=pow(saturate(dot(reflect(-light,n),view)),180)*(1-land)*.18;
 float city=pow(nightMap.Sample(planetSampler,float2(uv.x+.16,uv.y)).r,1.65)*pow(1-day,3)*.8;
 float ringLight=ringVisibility(world,light);
 float3 sunColor=float3(1,.78,.50);
 float fill=saturate(dot(n,normalize(float3(-.6,.4,.8))));
 float3 ambient=float3(.003,.007,.012)+float3(.020,.045,.070)*fill;
 float3 c=base*(max(ndl,0)*3.7*sunColor*ringLight*(1-shadowCloud*.65)+ambient)+spec*day*ringLight+float3(1,.42,.08)*city;
 float3 rimColor=lerp(float3(.006,.065,.11),float3(.7,.35,.09),pow(saturate(ndl),5));
 c+=rimColor*rim*(max(ndl,0)+.015)*1.5;
 return float4(tone(c),0);
}






