// World-space coast: metres along a gently sloping sandy beach. The surface is
// intersected in 3D; no sea, caustic or foam pixels come from the photograph.
float coastDistance(float2 p) {
 float along=dot(p,float2(.689655,-.724138));
 return (p.y+1.05*p.x-7.5)*.689655+.42*sin(along*.43)+.14*sin(along*1.1);
}
float coastAlong(float2 p) { return dot(p,float2(.689655,-.724138)); }
float coastBed(float2 p) { return -.20*coastDistance(p); }
float3 coastBedNormal(float2 p) {
 float along=coastAlong(p);
 float bend=.1806*cos(along*.43)+.154*cos(along*1.1);
 return float3(.20*(float2(.724138,.689655)+bend*float2(.689655,-.724138)),1.);
}
float coastTravel(float s) {
 // Match travel time AND its derivative at the still-water line. Clamping
 // distance at zero made the inland wave phase stop propagating, so run-up
 // appeared to accelerate abruptly as the water/bed intersection took over.
 if(s<0.)return (1.43/(2.*sqrt(.65)))*s-.30*s*s;
 return 1.43*(sqrt(s+.65)-sqrt(.65));
}
float coastArrival(float id,float along) {
 return id*6.7+(hash(float3(id,17,seed))-.5)*1.8
        +.22*sin(along*.48+id*2.3)+.09*sin(along*1.31+id);
}
float coastDrainTime(float id,float along) {
 // Different swashes settle at different rates; broad alongshore variation
 // keeps a whole strip of beach from emptying at the same instant.
 return 2.1+1.15*hash(float3(id,83,seed))+.22*sin(along*.37+id*1.9);
}
float coastMoisture(float2 p,float t,float strength) {
 float s=coastDistance(p),along=coastAlong(p),bed=coastBed(p);
 if(s>0.)return 1.;
 if(s< -2.5||strength<.0001)return 0.;
 float travel=coastTravel(s),event=floor((t+travel)/6.7);
 float moisture=0.;
 float dryingTime=22.+12.*noise(float3(p*.65,59.));
 // Reconstruct recent wetting at a fixed sand location. This persists across
 // waves and is independent of the visible foam's lifetime, including capture
 // and pause/resume: no frame-history texture or reset at a wave boundary.
 [unroll]for(int j=-7;j<=1;j++) {
  float id=event+j;
  float age=t+travel-coastArrival(id,along);
  float peak=(.16+.14*hash(float3(id,31,seed)))*exp(s*.45)*strength;
  float reached=smoothstep(-.012,.018,peak-bed);
  float deposited=smoothstep(-.35,.35,age);
  float retained=exp(-max(age,0.)/dryingTime)*(1.-smoothstep(36.,43.,age));
  moisture=max(moisture,reached*deposited*retained);
 }
 return moisture;
}
float coastHeight(float2 p,float t,float strength) {
 float s=coastDistance(p),along=coastAlong(p);
 float depth=max(0.,.20*s);
 float travel=coastTravel(s);
 float event=floor((t+travel)/6.7);
 float h=0.;
 [unroll]for(int j=-2;j<=1;j++) {
  float id=event+j;
  float r=hash(float3(id,31,seed));
  float age=t+travel-coastArrival(id,along);
  float width=.58+.28*r;
  // Crest arrives from offshore. The broader shallow sheet continues inland
  // and loses height; the bed intersection supplies run-up and drainage.
  float inland=max(-s,0.);
  float shallow=1.-smoothstep(.2,3.5,s);
  float arrivalWidth=width+.32*inland*inland/(1.+inland);
  // A quick incoming face feeds a broad, slowly draining sheet. Do not play
  // the arrival envelope backwards to remove the water after the crest.
  float drainWidth=lerp(arrivalWidth,coastDrainTime(id,along),shallow);
  // Both halves have zero slope at the peak; changing the outgoing
  // duration here preserves a monotonic fall instead of re-inflating the wash.
  float duration=age>0.?drainWidth:arrivalWidth;
  float pulse=exp(-pow(age/duration,2.))*(1.-smoothstep(10.,12.,age));
  float crest=(.16+.14*r)*pulse;
  float trough=.035*(1.-shallow*.85)*exp(-pow((age-1.7)/1.2,2.));
  h+=(crest-trough)*exp(min(s,0.)*.45);
 }
 float shoal=smoothstep(-.25,2.,s);
 float envelope=.7+.3*sin(p.x*.13+p.y*.09-t*.17);
 // Incommensurate directions and periods give a continuous offshore surface.
 float waves=.045*sin(s*1.43+t*1.67+along*.11)
            +.025*sin(s*2.31+t*2.13-along*.23+1.7)
            +.018*sin(p.x*2.9+p.y*1.7+t*2.74)
            +.009*sin(p.x*5.7-p.y*3.3-t*3.61);
 float footprint=dot(p,p)*.9/(2.4*resolution.y);
 h+=waves*shoal*envelope*min(1.,depth*2.)*exp(-footprint*footprint*12.);
 return h*strength;
}
// Each ripple contributes the gradient of one curved wave, rather than
// unrelated X/Y oscillations that produce a regular crosshatched normal map.
float2 coastRippleSlope(float2 p,float t,float2 direction,float speed,float amplitude,float offset,float footprint) {
 float2 bendDirection=float2(.61,-.37);
 float bend=dot(p,bendDirection)-t*.23+offset;
 float phase=dot(p,direction)+1.1*sin(bend)-t*speed+offset;
 float2 gradient=direction+1.1*cos(bend)*bendDirection;
 float gust=dot(p,float2(.27,.19))-t*.16+offset;
 float envelope=.65+.35*sin(gust);
 float2 envelopeGradient=.35*cos(gust)*float2(.27,.19);
 float filter=exp(-.5*dot(direction,direction)*footprint*footprint);
 return amplitude*(envelope*cos(phase)*gradient+sin(phase)*envelopeGradient)*filter;
}
float2 coastSmallSlope(float2 p,float t,float distance) {
 float footprint=max(.001,distance*distance*.9/(2.4*resolution.y));
 float2 slope=coastRippleSlope(p,t,float2(13,7),4.1,.0013,1.7,footprint);
 slope+=coastRippleSlope(p,t,float2(17,3),4.7,.0008,9.3,footprint);
 slope+=coastRippleSlope(p,t,float2(29,13),6.1,.0004,21.1,footprint);
 return slope*detail;
}
// Rotated, warped material coordinates remove visible Cartesian noise cells.
float2 coastMaterial(float2 p) {
 float2 q=float2(dot(p,float2(.819,.574)),dot(p,float2(-.574,.819)));
 return q+.23*float2(sin(q.y*2.7+q.x*.4),sin(q.x*2.3-q.y*.6));
}
float3 coastSky(float3 d) {
 float e=saturate(d.z);
 float3 c=lerp(float3(.055,.23,.38),float3(.095,.36,.70),sqrt(e));
 float2 q=d.xy/max(.18,d.z);
 float cloud=smoothstep(.50,.73,noise(float3(q*.8,7.))*.7+noise(float3(q*2.1,19.))*.3);
 return lerp(c,float3(.72,.81,.87),cloud*.35)*1.2;
}
float3 coastSand(float2 p,float footprint) {
 float broad=noise(float3(p*.7,13.));
 float grain=noise(float3(p*190.,7.))-.5;
 float fine=1.-smoothstep(.002,.016,footprint);
 return float3(.76,.61,.42)*(1.12+.075*broad+grain*.11*fine);
}
float coastFoam(float2 p,float t,float strength,float depth,float footprint) {
 [branch]if(depth>=1.20||depth<=.002||strength==0.)return 0.;
 float s=coastDistance(p),along=coastAlong(p);
 float travel=coastTravel(s),event=floor((t+travel)/6.7);
 float result=0.;
 [unroll]for(int j=-2;j<=1;j++) {
  float id=event+j;
  float age=t+travel-coastArrival(id,along);
  float r=hash(float3(id,31,seed));
  float breaking=exp(-pow((age-.15)/(.55+.2*r),2.));
  float drainTime=coastDrainTime(id,along);
  float residue=smoothstep(.1,.6,age)*(1.-smoothstep(drainTime*.65,drainTime+3.,age));
  // Material coordinates travel inland first, then drain offshore. Only flat
  // residual foam reverses; the incoming crest always travels toward shore.
  float materialAge=t-coastArrival(id,along);
  float runup=max(materialAge+.8,0.);
  float retreat=max(materialAge-drainTime*.8,0.);
  // One material clock across the wash avoids local phase compression. The
  // return current starts with zero speed and builds smoothly after run-up.
  float transport=1.05*(1.-exp(-runup*.85))
                 -(.16+.07*r)*(retreat-1.4*(1.-exp(-retreat/1.4)));
  float2 q=float2(along*.9,s+transport);
  q+=float2(id*13.71,id*7.13);
  q=coastMaterial(q);
  float cell=lerp(.5,noise(float3(q*7.,id*.37)),exp(-footprint*footprint*98.));
  float lace=abs(noise(float3(coastMaterial(q*1.7)*11.,id*.61))-.5);
  float bubbles=lerp(.28,1.-smoothstep(.045,.11,lace),exp(-footprint*footprint*700.));
  float patches=smoothstep(.29,.70,cell);
  // Thin foam breaks into patches before it vanishes, rather than clearing
  // the complete wash in one synchronized opacity sweep.
  float survival=1.-smoothstep(drainTime*(.55+.65*cell),drainTime+2.5,age);
  float coverage=breaking*(.23+.62*patches)+residue*survival*bubbles*patches*.55;
  result=max(result,coverage);
 }
 return saturate(result*(1.-smoothstep(.50,1.20,depth))*smoothstep(.002,.035,depth)*strength);
}
float3 coastRender(float2 p,float t,float strength) {
 float3 camera=float3(0,0,2.4);
 float3 ray=normalize(float3((p.x-.5)*1.7777778*.9,1.,(.505-p.y)*.9));
 float flatDistance=-camera.z/min(ray.z,-.0001);
 // Analytic beach-plane intersection, also used by refracted rays below.
 float3 bedNormal=float3(.1448276,.137931,1.);
 float bedConstant=1.0344828;
 float beachDistance=(bedConstant-dot(camera,bedNormal))/min(-.0001,dot(ray,bedNormal));
 if(dot(ray,bedNormal)>=0.)beachDistance=100000.;
 else {
  [unroll]for(int b=0;b<4;b++) {
   float3 q=camera+ray*beachDistance;
   beachDistance-=clamp((q.z-coastBed(q.xy))/min(-.025,dot(ray,coastBedNormal(q.xy))),-2.,2.);
  }
 }
 float distance=flatDistance;
 // Solve a displaced 3D height field. Distant subpixel geometry fades smoothly;
 // shading retains the broad water colour and angular sky reflection.
 float geometricFade=1.-smoothstep(45.,150.,flatDistance);
 // A bracketed intersection cannot oscillate between the front/back of a
 // steep crest, which otherwise shows up as little pixel blocks or jumps.
 float bound=(.48*strength+.03)*geometricFade;
 float nearDistance=(bound-camera.z)/min(ray.z,-.0001);
 float farDistance=(-bound-camera.z)/min(ray.z,-.0001);
 [unroll]for(int i=0;i<12;i++) {
  distance=(nearDistance+farDistance)*.5;
  float3 q=camera+ray*distance;
  float residual=q.z-coastHeight(q.xy,t,strength)*geometricFade;
  if(residual>0.)nearDistance=distance;else farDistance=distance;
 }
 distance=(nearDistance+farDistance)*.5;
 float3 hit=camera+ray*distance;
 float bed=coastBed(hit.xy);
 float depth=max(0.,hit.z-bed);
 float footprint=max(.001,distance*distance*.9/(2.4*resolution.y));
 float wet=smoothstep(-.025,.025,hit.z-bed);
 float3 beachHit=camera+ray*min(beachDistance,flatDistance);
 float3 sand=coastSand(beachHit.xy,footprint);
 // The wash exposes already-wet sand. Moisture stays put and relaxes over
 // tens of seconds rather than travelling back with the retreating foam.
 float damp=coastMoisture(beachHit.xy,t,strength);
 sand*=1.-.24*damp;
 if(wet<.001)return sand;
 float epsilon=max(.012,footprint*.7);
 float h=coastHeight(hit.xy,t,strength);
 float2 slope=float2(coastHeight(hit.xy+float2(epsilon,0),t,strength)-h,
                     coastHeight(hit.xy+float2(0,epsilon),t,strength)-h)/epsilon;
 slope=slope*geometricFade+coastSmallSlope(hit.xy,t,distance)*strength*smoothstep(.01,.12,depth);
 float3 normal=normalize(float3(-slope,1.));
 float3 refracted=refract(ray,normal,1./1.333);
 float bedPath=max(0.,(bedConstant-dot(hit,bedNormal))/min(-.05,dot(refracted,bedNormal)));
 [unroll]for(int b=0;b<3;b++) {
  float3 q=hit+refracted*bedPath;
  bedPath=max(0.,bedPath-(q.z-coastBed(q.xy))/min(-.15,dot(refracted,coastBedNormal(q.xy))));
 }
 float3 bottom=hit+refracted*bedPath;
 float3 transmission=exp(-float3(.65,.14,.085)*bedPath);
 // Refraction and travelling focusing bands animate the entire underwater view.
 float2 lightUV=coastMaterial(bottom.xy*.85+float2(-.12,.07)*t);
 float lightNoise=.65*noise(float3(lightUV*5.,17.))
                 +.35*noise(float3(coastMaterial(lightUV*1.7)*5.,29.));
 float focus=1.-smoothstep(.025,.12,abs(lightNoise-.52));
 float caustic=1.+.22*(focus-.25)*exp(-depth*.45)*strength*exp(-footprint*footprint*160.);
 float3 bottomColor=coastSand(bottom.xy,footprint)*caustic*exp(-float3(.15,.06,.035)*depth);
 float3 waterColor=float3(.008,.23,.29);
 float3 transmitted=bottomColor*transmission+waterColor*(1.-transmission);
 float fresnel=.0204+.9796*pow(1.-saturate(dot(normal,-ray)),5.);
 float3 reflected=reflect(ray,normal);
 float3 water=lerp(transmitted,coastSky(reflected),fresnel);
 float3 sun=normalize(float3(-.45,.35,.82));
 water+=float3(1.,.93,.78)*pow(saturate(dot(reflected,sun)),320.)*.9;
 float foam=coastFoam(hit.xy,t,strength,depth,footprint);
 float3 foamColor=float3(1.35,1.40,1.32)*(.85+.15*saturate(dot(normal,sun)));
 water=lerp(water,foamColor,foam);
 return lerp(sand,water,wet);
}
