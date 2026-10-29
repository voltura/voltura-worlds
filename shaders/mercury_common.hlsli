// Normalized Mercury radius; sunlight at the mean heliocentric distance.
static const float mercuryPi=3.14159265359;
static const float mercurySolar=1.0/(.38709893*.38709893);
float3 mercuryLight(){float a=radians(turbulence<=1?30*turbulence:30+60*(turbulence-1));return float3(sin(a),0,cos(a));}
float3 mercuryRotateY(float3 p,float a){float s,c;sincos(a,s,c);return float3(c*p.x+s*p.z,p.y,-s*p.x+c*p.z);}
float3 mercuryRotateX(float3 p,float a){float s,c;sincos(a,s,c);return float3(p.x,c*p.y-s*p.z,s*p.y+c*p.z);}
float3 mercuryRotateZ(float3 p,float a){float s,c;sincos(a,s,c);return float3(c*p.x-s*p.y,s*p.x+c*p.y,p.z);}
float mercuryClock(){return time*movement;}
float mercurySpin(){return radians(90)-mercuryClock()*(2*mercuryPi/900);}
float3 mercuryToBody(float3 p){
 p=mercuryRotateZ(p,-radians(0.034));
 p=mercuryRotateX(p,-radians(-8)+sin(mercuryClock()*.0017)*.012*flow);
 return mercuryRotateY(p,-mercurySpin());
}
float3 mercuryToWorld(float3 p){
 p=mercuryRotateY(p,mercurySpin());
 p=mercuryRotateX(p,radians(-8)-sin(mercuryClock()*.0017)*.012*flow);
 return mercuryRotateZ(p,radians(0.034));
}
float2 mercuryScreen(float2 uv){
#ifdef JOURNEY
 return journeyScreen(uv);
#else

 float2 p=(uv-.5)*resolution/min(resolution.x,resolution.y);
 p.y=-p.y;
 float t=mercuryClock();
 return (p-float2(.075+sin(t*.0011)*.009*flow,.008+sin(t*.0014)*.006*flow))/.375;

#endif
}
float3 mercuryGrade(float3 c){
 return c*(palette<.5?float3(1,1,1):palette<1.5?float3(1.045,1.01,.955):palette<2.5?float3(.96,1.005,1.05):float3(.99,1.005,1.01));
}
