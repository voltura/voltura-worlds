// A northward observer keeps the observed encounter hemisphere prominent.
static const float plutoPi=3.14159265359;
float plutoClock(){return time*movement;}
float3 plutoRY(float3 p,float a){float s,c;sincos(a,s,c);return float3(c*p.x+s*p.z,p.y,-s*p.x+c*p.z);}
float3 plutoRX(float3 p,float a){float s,c;sincos(a,s,c);return float3(p.x,c*p.y-s*p.z,s*p.y+c*p.z);}
float2 plutoScreen(float2 uv){
 float2 p=(uv-.5)*resolution/min(resolution.x,resolution.y);p.y=-p.y;
 float t=plutoClock();
 return (p-float2(.075+sin(t*.0011)*.009*flow,.008+sin(t*.0014)*.006*flow))/.375;
}
float3 plutoBody(float3 n){return plutoRY(plutoRX(n,-radians(25)), -plutoClock()*(2*plutoPi/900));}
float2 plutoUV(float3 body){return float2(.5+atan2(body.x,body.z)/(2*plutoPi),.5-asin(clamp(body.y,-1,1))/plutoPi);}
float2 plutoGradient(float2 v){v.x-=round(v.x);return v;}
float3 plutoLight(){float a=radians(35+(turbulence-1)*25);return normalize(float3(sin(a),.18,cos(a)));}
float3 plutoGrade(float3 c){return c*(palette<.5?float3(1,1,1):palette<1.5?float3(1.07,1.015,.94):palette<2.5?float3(.94,1.01,1.09):float3(.985,1.0,1.025));}
