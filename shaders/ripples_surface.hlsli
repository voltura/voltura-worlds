// One continuous, damped gravity-capillary wave-height field. Heights and
// gradients superpose before surface intersection, refraction or reflection.
struct WaterField { float height; float2 slope; };

void waterBand(inout WaterField field,float2 delta,float age,float amplitude,float flowScale,float k) {
 if(age<0||age>=7.)return;
 // A smooth radial coordinate avoids a cusp at the impact centre.
 float r=sqrt(dot(delta,delta)+.000004);
 float2 direction=delta/r;
 // Deep-water gravity-capillary dispersion: phase and group velocities
 // differ. The envelope carries energy while crests move within the packet.
 // omega^2 = g*k + (surface tension / density)*k^3.
 // https://ocw.mit.edu/courses/18-357-interfacial-phenomena-fall-2010/
 float tension=.000073;
 float omega=sqrt(9.81*k+tension*k*k*k);
 float group=(9.81+3.*tension*k*k)/(2.*omega);
 float propagationTime=age*.55*flowScale;
 float q=r-propagationTime*group;
 float width=.028+.005*age;
 float inverseWidth=1./(width*width);
 float envelope=exp(-q*q*inverseWidth);
 float fade=smoothstep(0.,.25,age)*(1.-smoothstep(4.,7.,age));
 float attenuation=rsqrt(1.+35.*r);
 float viscousDamping=exp(-2.*.000001*k*k*propagationTime);
 float a=amplitude*fade*attenuation*envelope*viscousDamping;
 float phase=k*r-omega*propagationTime;
 float wave=sin(phase),quadrature=cos(phase);
 float logarithmicSlope=-2.*q*inverseWidth-17.5/(1.+35.*r);
 field.height+=a*wave;
 field.slope+=a*(k*quadrature+logarithmicSlope*wave)*direction;
}

void waterDrop(inout WaterField field,float2 delta,float age,float amplitude,float flowScale,float k) {
 if(age<0||age>=7.)return;
 // A drop excites a band of wavelengths, not a rigid monochromatic ring train.
 // Their different group velocities spread the pulse and avoid a regular grid
 // of identical steep peaks where two trains cross.
 waterBand(field,delta,age,amplitude*.5,flowScale,k);
 waterBand(field,delta,age,amplitude*.3,flowScale,k*.72);
 waterBand(field,delta,age,amplitude*.2,flowScale,k*1.32);
 float dimple=-amplitude*(.0012/.0014)*exp(-dot(delta,delta)/.00018)*smoothstep(0.,.04,age)*exp(-age*12.);
 field.height+=dimple;
 field.slope+=dimple*(-2./.00018)*delta;
}

void waterWind(inout WaterField field,float2 p,float2 direction,float amplitude,float phase) {
 float angle=dot(p,direction)+phase;
 field.height+=amplitude*sin(angle);
 field.slope+=amplitude*cos(angle)*direction;
}

// Smooth value noise and its analytic spatial gradient. Gust envelopes and
// curved crests use these derivatives so their heights and normals agree.
float3 waterGust(float2 p) {
 float2 cell=floor(p),f=frac(p);
 float2 u=f*f*f*(f*(f*6.-15.)+10.);
 float2 du=30.*f*f*(f-1.)*(f-1.);
 float a=hash(float3(cell,63.));
 float b=hash(float3(cell+float2(1,0),63.))-a;
 float c=hash(float3(cell+float2(0,1),63.))-a;
 float d=hash(float3(cell+1.,63.))-a-b-c;
 return float3(a+b*u.x+c*u.y+d*u.x*u.y-.5,
               du.x*(b+d*u.y),du.y*(c+d*u.x));
}

void waterWindPacket(inout WaterField field,float2 p,float t,float2 direction,float amplitude,float phaseSpeed,float offset,float3 gust,float3 curl) {
 float envelope=.65+.6*gust.x;
 float2 envelopeGradient=.6*2.3*gust.yz;
 float phase=dot(p,direction)+1.35*curl.x-t*phaseSpeed+offset;
 float2 phaseGradient=direction+1.35*4.1*curl.yz;
 field.height+=amplitude*envelope*sin(phase);
 field.slope+=amplitude*(envelope*cos(phase)*phaseGradient+sin(phase)*envelopeGradient);
}

void waterAmbient(inout WaterField field,float2 p,float t) {
 float windTime=t*(.65+.35*flow);
 // Share the wind patch and curl field across scales, as one local breeze.
 float3 gust=waterGust(p*2.3-float2(windTime*.024,windTime*.011)+2.4);
 float3 curl=waterGust(p*4.1-float2(windTime*.016,-windTime*.009)+19.4);
 // Broad drift under short, patchy, gently curved wind ripples. All layers
 // move continuously in world space; none wrap or renew at cell boundaries.
 waterWind(field,p,float2(5,3),.0011*turbulence,-windTime*.24);
 waterWind(field,p,float2(-3,8),.00065*turbulence,-windTime*.31+1.7);
 waterWindPacket(field,p,windTime,float2(34,13),.00023*turbulence,.85,2.4,gust,curl);
 waterWindPacket(field,p,windTime,float2(19,47),.00014*turbulence,1.15,8.1,gust,curl);
 if(spare0.x>.5)waterWindPacket(field,p,windTime,float2(71,-23),.00007*turbulence,1.55,14.3,gust,curl);
 if(spare0.x>1.5)waterWindPacket(field,p,windTime,float2(42,109),.000035*turbulence,2.1,23.6,gust,curl);
}

WaterField waterField(float2 p,float t,float audio,float aspect) {
 WaterField field=(WaterField)0;
 float flowScale=.5+.5*flow;
 float k=145.+65.*detail;
 // Native capture diagnostics use the same drop evaluator and surface shader.
 // 1/2: isolated pair, 3: crossing pair, 4: no drops, 5: exact phase cancellation,
 // 6: coincident in-phase drops. These modes never enter ordinary playback.
 if(spare0.y>.5) {
  int probe=(int)spare0.y;
  float age=3.2+t*.1;
  float amplitude=.0012*(1.+audio*.35);
  if(probe==1||probe==3||probe==5||probe==6)waterDrop(field,p-float2(-.18,0),age,amplitude,flowScale,k);
  if(probe==2||probe==3)waterDrop(field,p-float2(.18,0),age,amplitude,flowScale,k);
  if(probe==5)waterDrop(field,p-float2(-.18,0),age,-amplitude,flowScale,k);
  if(probe==6)waterDrop(field,p-float2(-.18,0),age,amplitude,flowScale,k);
  if(probe==4)waterAmbient(field,p,t);
  return field;
 }
 float period=12./(.45+.65*density);
 [unroll]for(int lane=0;lane<10;lane++) {
  float clock=t+18.+period*lane/10.;
  float epoch=floor(clock/period);
  [unroll]for(int previous=0;previous<2;previous++) {
   float event=epoch-previous;
   float age=clock-event*period;
   if(age>=0&&age<7.) {
    float3 id=float3(lane,event,seed+37.);
    float2 center=(float2(hash(id),hash(id+float3(51,17,23)))-.5)*float2(aspect*1.65+.2,2.1);
    float amplitude=.0012*(.65+.7*hash(id+71.))*(1.+audio*.35);
    waterDrop(field,p-center,age,amplitude,flowScale,k*(.92+.16*hash(id+19.)));
   }
  }
 }
 waterAmbient(field,p,t);
 return field;
}
