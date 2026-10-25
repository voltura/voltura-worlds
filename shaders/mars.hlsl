#include "common.hlsli"
#include "mars_common.hlsli"
Texture2D<float4> surfaceMap:register(t0);
Texture2D<float4> normalMap:register(t1);
SamplerState planetSampler:register(s1);

float2 marsGradient(float2 v){v.x-=round(v.x);return v;}
// Fraction of the finite solar disc above a spherical horizon. The Sun's
// angular radius at 1.524 AU is 0.175 degrees, not an artistic blur width.
float solarDisc(float mu){
 float x=clamp(mu/.003054,-1,1);
 return .5+(asin(x)+x*sqrt(max(1-x*x,0)))/marsPi;
}
float erfcScaledPositive(float x){
 // exp(x*x)*erfc(x), evaluated without the large exponential for x>=0.
 float t=1/(1+.3275911*x);
 return (((((1.061405429*t-1.453152027)*t)+1.421413741)*t-.284496736)*t+.254829592)*t;
}
float solarColumn(float r,float mu){
 // Chapman-style column for an exponential atmosphere, retaining spherical
 // curvature at the horizon. The planar 1/mu approximation diverges there.
 const float scaleHeight=.0032;
 float rho=exp(-max(r-1,0)/scaleHeight);
 if(mu>.98)return rho*scaleHeight/mu;
 float a=max((1-mu*mu)/(2*r*scaleHeight),.00001);
 float x=mu/(2*scaleHeight*sqrt(a));
 float column=erfcScaledPositive(abs(x));
 if(x<0)column=2*exp(min(x*x,20))-column;
 return rho*sqrt(marsPi)/(2*sqrt(a))*column;
}
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET{
 float2 q=marsScreen(uv);
 float r2=dot(q,q),radius=sqrt(r2);
 float edge=max(fwidth(radius),.00001);
 float outer=1.018;
 clip(outer+edge-radius);
 float3 light=marsLight();
 // Dust absorbs blue somewhat more strongly. Use the same extinction for
 // sunlight arriving at the ground and for the atmospheric integration.
 float3 extinctionColor=float3(.92,1.08,1.35);
 float3 color=0;
 float surfaceCoverage=saturate((1-radius)/edge+.5);
 // Evaluate derivatives before divergent branches and wrap longitude gradients.
 float3 n=float3(q,sqrt(max(1-r2,.000001)));
 n=normalize(n);
 float3 body=marsToBody(n);
 float dust=1+turbulence*.12*(noise(body*11+marsClock()*.0004)-.5);
 float extinction=5*density*dust;
 float longitude=atan2(body.z,body.x),latitude=asin(clamp(body.y,-1,1));
 float2 coord=float2(longitude/(2*marsPi)+.5,.5-latitude/marsPi);
 float2 dx=marsGradient(ddx(coord)),dy=marsGradient(ddy(coord));
 if(surfaceCoverage>0){
  float3 albedo=surfaceMap.SampleGrad(planetSampler,coord,dx,dy).rgb;
  float3 tangentNormal=normalize(normalMap.SampleGrad(planetSampler,coord,dx,dy).xyz*2-1);
  tangentNormal=normalize(float3(tangentNormal.xy*(detail*2.4),max(tangentNormal.z,.1)));
  float3 east=float3(-sin(longitude),0,cos(longitude));
  float3 north=float3(-cos(longitude)*sin(latitude),cos(latitude),-sin(longitude)*sin(latitude));
  float3 bump=marsToWorld(normalize(east*tangentNormal.x+north*tangentNormal.y+body*tangentNormal.z));
  // A Lommel-Seeliger/Lambert mixture better approximates particulate regolith
  // than a smooth Lambert ball. The normal map has no displaced geometry to
  // keep an independently lit ridge above the spherical horizon. Taper its
  // direct-light perturbation on the day side so finite ridge brightness does
  // not suddenly hit the shared solar-disc cutoff. The terminator, atmospheric
  // twilight and outer silhouette keep their existing positions and widths.
  float mu=dot(n,light);
  float mu0=max(mu,0),muView=max(n.z,.001);
  float slope=clamp(dot(bump,light)-mu,-.15,.15);
  float reliefVisibility=smoothstep(0,.08,mu0);
  float incidence=max(mu0+slope*reliefVisibility,0)*solarDisc(mu);
  float diffuse=incidence*(.45+.55/max(mu0+muView,.08));
  float3 sunlight=exp(-solarColumn(1,max(mu,0))*extinction*extinctionColor);
  color=albedo*marsSolar*diffuse*sunlight;
 }
 // A thin, exponential dusty atmosphere. Integrate only the segment in front
 // of the opaque globe, or the complete limb chord outside the surface.
 float zOuter=sqrt(max(outer*outer-r2,0));
 float zInner=r2<1?sqrt(max(1-r2,0)):-zOuter;
 float stride=max(zOuter-zInner,0)/32;
 float optical=0;float3 scattering=0;
 float audio=movement>0?reaction*saturate(energy)*.08:0;
 // Light travels from the Sun along -light and exits towards the observer.
 // Dust is forward-scattering; the current gibbous view mostly backscatters.
 float cosScatter=-light.z,g=.6;
 float phase=(1-g*g)/pow(1+g*g-2*g*cosScatter,1.5);
 float3 scatterColor=float3(.72,.43,.22)*(.35+.65*phase);
 [unroll]for(int i=0;i<32;i++){
  float3 position=float3(q,zOuter-(i+.5)*stride);
  float height=max(length(position)-1,0);
  float air=exp(-height/.0032)*density;
  float3 radial=normalize(position);
  float horizon=sqrt(saturate(1-1/max(dot(position,position),1)));
  float muSun=dot(radial,light);
  float sun=solarDisc(muSun+horizon);
  float segment=air*stride*5*dust;
  float3 viewTransmission=exp(-optical*extinctionColor);
  float3 solarTransmission=exp(-solarColumn(length(position),muSun)*extinction*extinctionColor);
  // Exact constant-density segment weight avoids bright quadrature bands.
  scattering+=viewTransmission*(1-exp(-segment))*sun*solarTransmission*scatterColor*marsSolar*(1+audio);
  optical+=segment;
 }
 float3 transmittance=exp(-optical*extinctionColor);
 // Cinematic exposure follows distance-based radiance; no emissive night map.
 float exposure=3.2;
 float3 radiance=color*surfaceCoverage*transmittance+scattering;
 float alpha=surfaceCoverage+(1-surfaceCoverage)*(1-exp(-optical));
 alpha*=saturate((outer-radius)/edge+.5);
 return float4(tone(marsGrade(radiance*exposure))/max(alpha,.00001),alpha);
}
