// Venus is represented at its opaque cloud tops. Motion is accelerated for a
// wallpaper: retrograde superrotation completes a circuit in 15 minutes.
static const float venusPi=3.14159265359;
float venusClock(){return time*movement;}
float3 venusRY(float3 p,float a){float s,c;sincos(a,s,c);return float3(c*p.x+s*p.z,p.y,-s*p.x+c*p.z);}
float2 venusScreen(float2 uv){
 float2 p=(uv-.5)*resolution/min(resolution.x,resolution.y);p.y=-p.y;
 float t=venusClock();
 return (p-float2(.075+sin(t*.0011)*.009*flow,.008+sin(t*.0014)*.006*flow))/.375;
}
float3 venusLight(){return normalize(float3(-.58,.20,.79));}
float3 venusGrade(float3 c){
 return c*(palette<.5?float3(1,1,1):palette<1.5?float3(1.06,1.015,.91):palette<2.5?float3(.94,.99,1.06):float3(.99,1.005,1.02));
}
