static const float moonPi=3.14159265359;
static const float moonEarthDistance=384400.0/1737.4;
static const float moonEarthRadius=6371.0/1737.4;
float moonClock(){return time*movement;}
float moonAngle(){return frac(moonClock()/900)*(2*moonPi);}
float3 moonLight(){float a=radians(60+(turbulence-1)*30);return float3(sin(a),0,cos(a));}
float2 moonUV(float3 p){return float2(atan2(p.x,p.z)/(2*moonPi)+.5,.5-asin(clamp(p.y,-1,1))/moonPi);}
float2 moonGradient(float2 v){v.x-=round(v.x);return v;}
void moonCamera(float2 uv,out float3 origin,out float3 ray){
 float a=moonAngle(),pitch=sin(a)*.012*flow;
 float3 outward=float3(sin(a)*cos(pitch),sin(pitch),cos(a)*cos(pitch));
 origin=outward*3;
 float3 forward=-outward,right=normalize(cross(forward,float3(0,1,0))),up=cross(right,forward);
 float2 center=float2(.075+sin(a)*.009*flow,.008+sin(2*a)*.006*flow);
 float2 q=(uv-.5)*resolution/min(resolution.x,resolution.y);q.y=-q.y;q-=center;
 // At distance 3, tan(angular radius)=1/sqrt(8). Radius fills .375
 // of the shorter viewport dimension, independently of aspect ratio.
 ray=normalize(forward+(right*q.x+up*q.y)/(.375*sqrt(8.0)));
}
float moonSun(float mu){
 float x=clamp(mu/.004651,-1,1);
 return .5+(asin(x)+x*sqrt(max(1-x*x,0)))/moonPi;
}
float3 moonGrade(float3 c){
 return c*(palette<.5?float3(1,1,1):palette<1.5?float3(1.045,1.01,.955):palette<2.5?float3(.96,1.005,1.05):float3(.99,1.005,1.01));
}
// Signed distance from the viewing ray to a sphere's projected limb.
// Perpendicular-vector form avoids catastrophic cancellation at Earth's
// distance; dot(center,center)-b*b loses crater-sized precision there.
void moonSphere(float3 origin,float3 ray,float3 center,float radius,out float distance,out float coverage,out float3 normal){
 float3 offset=center-origin;float b=dot(offset,ray);
 float3 perpendicular=offset-ray*b;
 float radial=length(perpendicular)/radius;
 float edge=max(fwidth(radial),.000001);
 coverage=saturate((1-radial)/edge+.5)*(b>0?1:0);
 distance=b-sqrt(max(radius*radius-dot(perpendicular,perpendicular),0));
 normal=normalize(origin+ray*distance-center);
}
