// Normalized Mars radius; sunlight at the mean heliocentric distance.
static const float marsPi=3.14159265359;
static const float marsSolar=1.0/(1.524*1.524);
float3 marsLight(){return normalize(float3(.76,.30,.58));}
float3 marsRotateY(float3 p,float a){float s,c;sincos(a,s,c);return float3(c*p.x+s*p.z,p.y,-s*p.x+c*p.z);}
float3 marsRotateX(float3 p,float a){float s,c;sincos(a,s,c);return float3(p.x,c*p.y-s*p.z,s*p.y+c*p.z);}
float3 marsRotateZ(float3 p,float a){float s,c;sincos(a,s,c);return float3(c*p.x-s*p.y,s*p.x+c*p.y,p.z);}
float marsClock(){return time*movement;}
float marsSpin(){return radians(210)-marsClock()*(2*marsPi/900);}
float3 marsToBody(float3 p){
 p=marsRotateZ(p,-radians(25.19));
 p=marsRotateX(p,-radians(-8)+sin(marsClock()*.0017)*.012*flow);
 return marsRotateY(p,-marsSpin());
}
float3 marsToWorld(float3 p){
 p=marsRotateY(p,marsSpin());
 p=marsRotateX(p,radians(-8)-sin(marsClock()*.0017)*.012*flow);
 return marsRotateZ(p,radians(25.19));
}
float2 marsScreen(float2 uv){
#ifdef JOURNEY
 return journeyScreen(uv);
#else

 float2 p=(uv-.5)*resolution/min(resolution.x,resolution.y);
 p.y=-p.y;
 float t=marsClock();
 return (p-float2(.075+sin(t*.0011)*.009*flow,.008+sin(t*.0014)*.006*flow))/.375;

#endif
}
float3 marsGrade(float3 c){
 // Exposure/white-balance variations retain the source geography and colors.
 float3 grade=palette<.5?float3(1,1,1):palette<1.5?float3(1.075,1.015,.92):palette<2.5?float3(.95,1.015,1.09):float3(.97,1.025,1.035);
 return c*grade;
}
