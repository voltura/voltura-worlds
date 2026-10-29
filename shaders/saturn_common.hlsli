static const float saturnPi=3.14159265359;
static const float saturnPolar=54364.0/60268.0;
float saturnAngle(){return frac(time*movement/900)*(2*saturnPi);}
float3 saturnRX(float3 p,float a){float s,c;sincos(a,s,c);return float3(p.x,c*p.y-s*p.z,s*p.y+c*p.z);}
float3 saturnRZ(float3 p,float a){float s,c;sincos(a,s,c);return float3(c*p.x-s*p.y,s*p.x+c*p.y,p.z);}
float saturnElevation(){return radians(spare1.z>0?spare1.y:25);}
float3 saturnToBody(float3 p){return saturnRX(saturnRZ(p,-radians(15)),-saturnElevation());}
float3 saturnToWorld(float3 p){return saturnRZ(saturnRX(p,saturnElevation()),radians(15));}
float2 saturnScreen(float2 uv){
#ifdef JOURNEY
 return journeyScreen(uv);
#else

 float2 ext=2.322*sqrt(float2(pow(cos(radians(15)),2)+pow(sin(radians(15))*sin(saturnElevation()),2),pow(sin(radians(15)),2)+pow(cos(radians(15))*sin(saturnElevation()),2)));
 ext=max(ext,float2(1,1))+.035;
 float2 available=.425*resolution/min(resolution.x,resolution.y);
 float scale=min(available.x/ext.x,available.y/ext.y);
 float2 q=(uv-.5)*resolution/min(resolution.x,resolution.y);q.y=-q.y;
 return q/scale-float2(sin(saturnAngle()),sin(2*saturnAngle()))*.015*flow;

#endif
}
float3 saturnGrade(float3 c){return c*(palette<.5?float3(1,1,1):palette<1.5?float3(1.09,1.015,.9):palette<2.5?float3(.91,1.01,1.12):float3(.98,1.005,1.025));}
// NASA main-ring radii divided by the equatorial radius. Optical depths are
// representative artistic values within the measured ranges, not a retrieval.
float saturnBand(float r,float lo,float hi,float footprint){return smoothstep(lo-footprint,lo+footprint,r)*(1-smoothstep(hi-footprint,hi+footprint,r));}
float saturnRingDepth(float r,float footprint){
 float c=saturnBand(r,1.239,1.526,footprint)*.16;
 float b=saturnBand(r,1.526,1.950,footprint)*lerp(.5,1.7,smoothstep(1.526,1.93,r));
 float a=saturnBand(r,2.030,2.270,footprint)*.55;
 float f=saturnBand(r,2.319,2.322,footprint)*.12;
 // Integrate away subpixel radial frequencies, as in Orbit's ringNoise.
 float broad=sin(r*85+2*sin(r*23))*exp(-.5*pow(85*footprint,2));
 float bands=sin(r*220+sin(r*57))*exp(-.5*pow(220*footprint,2));
 float fine=sin(r*720)*exp(-.5*pow(720*footprint,2));
 float filaments=sin(r*2600+sin(r*170))*exp(-.5*pow(2600*footprint,2));
 float gaps=1-.85*saturnBand(r,2.2125,2.2175,footprint);
 return max(0,(c+b+a)*(.78+.16*broad+.08*bands+.06*fine+.03*filaments)*gaps+f);
}
float saturnHit(float3 o,float3 d,out float discriminant){
 float3 m=float3(1,1/(saturnPolar*saturnPolar),1);
 float a=dot(d*d,m),b=dot(o*d,m),c=dot(o*o,m)-1;
 discriminant=b*b-a*c;
 return (-b-sqrt(max(discriminant,0)))/a;
}
