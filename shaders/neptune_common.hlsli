// Cinematic cloud-top reconstruction. A 15-minute rotation and accelerated
// zonal winds are wallpaper motion, not a physical atmospheric simulation.
static const float neptunePi=3.14159265359;
static const float neptunePolar=.983;
float neptuneClock(){return time*movement;}
float3 neptuneRY(float3 p,float a){float s,c;sincos(a,s,c);return float3(c*p.x+s*p.z,p.y,-s*p.x+c*p.z);}
float2 neptuneRotate(float2 p,float a){float s,c;sincos(a,s,c);return float2(c*p.x-s*p.y,s*p.x+c*p.y);}
float2 neptuneScreen(float2 uv){
 float2 p=(uv-.5)*resolution/min(resolution.x,resolution.y);p.y=-p.y;
 float t=neptuneClock();
 p=(p-float2(.075+sin(t*.0011)*.009*flow,.008+sin(t*.0014)*.006*flow))/.375;
 return neptuneRotate(p,radians(-28.3));
}
float3 neptuneLight(){float2 p=neptuneRotate(float2(-.48,.22),radians(-28.3));return normalize(float3(p,.85));}
float3 neptuneBody(float3 n){return neptuneRY(n,neptuneClock()*(2*neptunePi/900));}
float3 neptuneBase(){
 return palette<.5?float3(.025,.17,.48):palette<1.5?float3(.20,.44,.49):palette<2.5?float3(.018,.095,.38):float3(.12,.27,.43);
}
