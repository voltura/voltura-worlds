#include "common.hlsli"
#include "earth_common.hlsli"
Texture2D<float4> surfaceMap:register(t0);
Texture2D<float> cloudMap:register(t1);
Texture2D<float2> materialMap:register(t2);
SamplerState globeSampler:register(s1);

float2 cloudUV(float3 world){
 float3 body=earthToBody(normalize(world));
 // A rigid spherical transform keeps storms intact, with no UV stretching.
 body=earthRY(body,-earthClock()*.000045*turbulence);
 return earthUV(body);
}
float cloudAt(float3 world,float2 dx,float2 dy){return saturate(cloudMap.SampleGrad(globeSampler,cloudUV(world),dx,dy)*1.12-.045);}
float shellDistance(float3 p,float3 light,float radius){
 float b=dot(p,light);return max(-b+sqrt(max(b*b+radius*radius-dot(p,p),0)),0);
}

float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET{
 float2 q=earthScreen(uv);float r2=dot(q,q),radius=sqrt(r2);
 float edge=max(fwidth(radius),.00001);clip(earthAirRadius+edge-radius);
 float3 light=earthLight();float probe=spare1.x;
 float surfaceCoverage=saturate((1-radius)/edge+.5);
 float3 n=normalize(float3(q,sqrt(max(1-r2,.000001))));
 float3 body=earthToBody(n);float2 coord=earthUV(body);
 float2 dx=earthGradient(ddx(coord)),dy=earthGradient(ddy(coord));
 float cloudZ=sqrt(max(earthCloudRadius*earthCloudRadius-r2,.000001));
 float3 cloudPoint=float3(q,cloudZ),cloudNormal=normalize(cloudPoint);
 float2 cloudCoord=cloudUV(cloudPoint);
 float2 cdx=earthGradient(ddx(cloudCoord)),cdy=earthGradient(ddy(cloudCoord));
 float3 ground=0,city=0,glint=0,albedo=0;float water=0,shadow=1;
 if(surfaceCoverage>0){
  albedo=surfaceMap.SampleGrad(globeSampler,coord,dx,dy).rgb;
  float2 material=materialMap.SampleGrad(globeSampler,coord,dx,dy);
  // Snow and sea ice stay diffuse even where the geographic mask is water.
  water=material.r*(1-smoothstep(.16,.45,min(albedo.r,min(albedo.g,albedo.b))));
  float mu=dot(n,light),sun=earthSun(mu);
  float3 sunTransmission=earthSolarTransmission(1,max(mu,0));
  float3 shadowPoint=n+light*shellDistance(n,light,earthCloudRadius);
  float shadowCloud=cloudAt(shadowPoint,cdx,cdy);
  shadow=exp(-shadowCloud*1.8/max(dot(normalize(shadowPoint),light),.22));
  float diffuse=max(mu,0)*sun;
  ground=albedo*(diffuse/earthPi)*sunTransmission*(probe==5?1:shadow);
  // A normalized microfacet lobe and water Fresnel, gated by the same Sun.
  float3 halfVector=normalize(light+float3(0,0,1));
  float nh=max(dot(n,halfVector),0),nv=max(n.z,.001),nl=max(mu,.001);
  float a2=.0121,denom=nh*nh*(a2-1)+1;
  float distribution=a2/(earthPi*denom*denom);
  float visibility=1/(4*(nv*(1-.06)+.06)*(nl*(1-.06)+.06));
  float fresnel=.0204+.9796*pow(1-saturate(dot(halfVector,light)),5);
  glint=water*fresnel*distribution*visibility*diffuse*sunTransmission*shadow;
  float viewCloud=cloudAt(cloudPoint,cdx,cdy);
  city=float3(1,.63,.28)*pow(material.g,1.35)*(.42/earthPi)*(1-smoothstep(-.12,.02,mu))*exp(-viewCloud*2.5/max(nv,.22));
  if(probe==6)city=0;
  ground=(ground+glint+city)*surfaceCoverage;
 }
 // Cloud shell at 8 km with footprint-filtered relief and directional shadow.
 float cloudCoverage=saturate((earthCloudRadius-radius)/edge+.5);
 if(cloudCoverage>0&&probe!=4&&probe!=5&&probe!=7){
  float coverage=cloudAt(cloudPoint,cdx,cdy);
  float mu=dot(cloudNormal,light);
  float3 shadowPoint=cloudPoint+light*.0007;
  float sunward=cloudAt(shadowPoint,cdx,cdy);
  float relief=clamp((coverage-sunward)*detail*2.5,-.22,.22);
  float incidence=max(mu+relief*smoothstep(0,.12,mu),0)*earthSun(mu);
  float selfShadow=exp(-sunward*.65*detail/max(mu,.25));
  float twilight=.018*exp(-abs(mu)/.035)*density;
  float3 cloudLight=earthSolarTransmission(earthCloudRadius,max(mu,0))*(incidence*(.8+.2*selfShadow)+twilight)/earthPi;
  float opacity=(1-exp(-coverage*3.3/max(cloudNormal.z,.24)))*cloudCoverage;
  ground=ground*(1-opacity)+cloudLight*opacity;
 }
 // Integrate the atmosphere in front of the opaque sphere, or the full limb.
 float zOuter=sqrt(max(earthAirRadius*earthAirRadius-r2,0));
 float zInner=r2<1?sqrt(max(1-r2,0)):-zOuter;
 float stride=max(zOuter-zInner,0)/24;
 float3 optical=0,scattering=0;
 float cosTheta=-light.z;
 float phaseR=3*(1+cosTheta*cosTheta)/(16*earthPi);
 float g=.76,phaseM=(1-g*g)/(4*earthPi*pow(1+g*g-2*g*cosTheta,1.5));
 float audio=movement>0?1+reaction*saturate(energy)*.06:1;
 [unroll]for(int i=0;i<24;i++){
  float3 p=float3(q,zOuter-(i+.5)*stride);
  float r=length(p),height=max(r-1,0);
  float rayleigh=exp(-height/earthHR)*density,mie=exp(-height/earthHM)*density;
  float3 segment=(earthBetaR*rayleigh+earthBetaME*mie)*stride;
  float3 source=(earthBetaR*rayleigh*phaseR+earthBetaMS*mie*phaseM)*stride;
  float mu=dot(p/r,light),horizon=sqrt(saturate(1-1/(r*r)));
  float sunlight=earthSun(mu+horizon);
  scattering+=exp(-optical)*((1-exp(-segment))/max(segment,.000001))*source*earthSolarTransmission(r,mu)*sunlight*audio;
  optical+=segment;
 }
 float3 radiance=ground*exp(-optical)+scattering;
 float alpha=surfaceCoverage+(1-surfaceCoverage)*(1-exp(-max(optical.r,max(optical.g,optical.b))));
 if(probe>0){
  alpha=surfaceCoverage;
  if(probe==1)radiance=city*surfaceCoverage;
  if(probe==2)radiance=glint*surfaceCoverage;
  if(probe==3)radiance=water*surfaceCoverage;
  if(probe==4||probe==5)radiance=ground;
  if(probe==7)radiance=max(ground-glint-city,0)*surfaceCoverage;
  if(probe==8)radiance=albedo*surfaceCoverage/earthPi;
 }
 alpha*=saturate((earthAirRadius-radius)/edge+.5);
 return float4(tone(earthGrade(radiance*(2.4*earthPi)))/max(alpha,.00001),alpha);
}
