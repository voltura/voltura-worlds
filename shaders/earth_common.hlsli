static const float earthPi=3.14159265359;
static const float earthRadius=6371000;
static const float earthCloudRadius=1.001256;
static const float earthAirRadius=1.015696;
float earthClock(){return time*movement;}
float3 earthLight(){return normalize(float3(.82,.24,.52));}
float3 earthRY(float3 p,float a){float s,c;sincos(a,s,c);return float3(c*p.x+s*p.z,p.y,-s*p.x+c*p.z);}
float3 earthRX(float3 p,float a){float s,c;sincos(a,s,c);return float3(p.x,c*p.y-s*p.z,s*p.y+c*p.z);}
float3 earthRZ(float3 p,float a){float s,c;sincos(a,s,c);return float3(c*p.x-s*p.y,s*p.x+c*p.y,p.z);}
// East longitude moves rightward on the front hemisphere: eastward rotation.
float earthSpin(){return radians(-15)+earthClock()*(2*earthPi/900);}
float earthPitch(){return radians(12)+sin(earthClock()*.0017)*.012*flow;}
float3 earthToBody(float3 p){return earthRY(earthRX(earthRZ(p,-radians(23.44)),-earthPitch()),-earthSpin());}
float3 earthToWorld(float3 p){return earthRZ(earthRX(earthRY(p,earthSpin()),earthPitch()),radians(23.44));}
float2 earthScreen(float2 uv){
#ifdef JOURNEY
 return journeyScreen(uv);
#else

 float2 p=(uv-.5)*resolution/min(resolution.x,resolution.y);p.y=-p.y;
 return (p-float2(.075+sin(earthClock()*.0011)*.009*flow,.008+sin(earthClock()*.0014)*.006*flow))/.375;

#endif
}
float2 earthUV(float3 p){return float2(atan2(p.x,p.z)/(2*earthPi)+.5,.5-asin(clamp(p.y,-1,1))/earthPi);}
float2 earthGradient(float2 v){v.x-=round(v.x);return v;}
float3 earthGrade(float3 c){
 return c*(palette<.5?float3(1,1,1):palette<1.5?float3(1.055,1.01,.95):palette<2.5?float3(.955,1.01,1.065):float3(.99,1.005,1.01));
}
// Finite 0.533-degree solar disc at Earth's mean distance, no artistic blur.
float earthSun(float mu){float x=clamp(mu/.004651,-1,1);return .5+(asin(x)+x*sqrt(max(1-x*x,0)))/earthPi;}
float earthErfc(float x){float t=1/(1+.3275911*x);return (((((1.061405429*t-1.453152027)*t)+1.421413741)*t-.284496736)*t+.254829592)*t;}
// Exponential atmospheric column including curvature at grazing incidence.
float earthColumn(float r,float mu,float h){
 float rho=exp(-max(r-1,0)/h);
 if(mu>.98)return rho*h/mu;
 float a=max((1-mu*mu)/(2*r*h),.00001),x=mu/(2*h*sqrt(a));
 float column=earthErfc(abs(x));if(x<0)column=2*exp(min(x*x,20))-column;
 return rho*sqrt(earthPi)/(2*sqrt(a))*column;
}
static const float earthHR=8000/earthRadius,earthHM=1200/earthRadius;
static const float3 earthBetaR=float3(5.802,13.558,33.1)*.000001*earthRadius;
static const float earthBetaMS=3.996*.000001*earthRadius,earthBetaME=4.4*.000001*earthRadius;
float3 earthSolarTransmission(float r,float mu){
 return exp(-density*(earthBetaR*earthColumn(r,mu,earthHR)+earthBetaME*earthColumn(r,mu,earthHM)));
}
