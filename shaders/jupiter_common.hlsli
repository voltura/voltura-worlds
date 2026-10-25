static const float jupiterPi=3.14159265359;
static const float jupiterPolar=66854.0/71492.0;
static const float jupiterScaleHeight=27.0/71492.0;
static const float jupiterSolar=1.0/(5.204*5.204);
float jupiterClock(){return time*movement;}
float jupiterAngle(){return frac(jupiterClock()/900)*(2*jupiterPi);}
float3 jupiterRY(float3 p,float a){float s,c;sincos(a,s,c);return float3(c*p.x+s*p.z,p.y,-s*p.x+c*p.z);}
float3 jupiterRX(float3 p,float a){float s,c;sincos(a,s,c);return float3(p.x,c*p.y-s*p.z,s*p.y+c*p.z);}
float3 jupiterRZ(float3 p,float a){float s,c;sincos(a,s,c);return float3(c*p.x-s*p.y,s*p.x+c*p.y,p.z);}
float jupiterSpin(){return radians(-38)+jupiterAngle();}
float jupiterPitch(){return radians(8)+sin(jupiterAngle())*.012*flow;}
float3 jupiterToBody(float3 p){return jupiterRY(jupiterRX(jupiterRZ(p,-radians(3.13)),-jupiterPitch()),-jupiterSpin());}
float3 jupiterToWorld(float3 p){return jupiterRZ(jupiterRX(jupiterRY(p,jupiterSpin()),jupiterPitch()),radians(3.13));}
float2 jupiterScreen(float2 uv){
 float2 p=(uv-.5)*resolution/min(resolution.x,resolution.y);p.y=-p.y;
 float a=jupiterAngle();return (p-float2(.075+sin(a)*.009*flow,.008+sin(2*a)*.006*flow))/.375;
}
float3 jupiterLight(){
 // Near-frontal sunlight keeps the illuminated disc close to the real
 // oblate silhouette, instead of presenting a broad dark crescent.
 float2 side=normalize(float2(.65,.20))*sin(radians(8));
 return float3(side,cos(radians(8)));
}
float2 jupiterUV(float3 normal){
 // OPAL uses planetographic latitude (ellipsoid normal), not geocentric.
 // Source left is 0 west, decreasing right: positive-east longitude 0..360.
 return float2(frac(atan2(normal.x,normal.z)/(2*jupiterPi)),.5-asin(clamp(normal.y,-1,1))/jupiterPi);
}
float2 jupiterGradient(float2 v){v.x-=round(v.x);return v;}
float jupiterSun(float mu){float x=clamp(mu/(.004651/5.204),-1,1);return .5+(asin(x)+x*sqrt(max(1-x*x,0)))/jupiterPi;}
float3 jupiterGrade(float3 c){return c*(palette<.5?float3(1,1,1):palette<1.5?float3(1.04,1.005,.95):palette<2.5?float3(.96,1.005,1.045):float3(.99,1.005,1.01));}
float jupiterColumn(float mu){return rsqrt(max(mu,0)*max(mu,0)+2*jupiterScaleHeight);}
