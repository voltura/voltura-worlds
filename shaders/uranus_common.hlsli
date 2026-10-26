static const float uranusPi=3.14159265359;
static const float uranusPolar=24973.0/25559.0;
float uranusClock(){return time*movement;}
float3 uranusRY(float3 p,float a){float s,c;sincos(a,s,c);return float3(c*p.x+s*p.z,p.y,-s*p.x+c*p.z);}
float3 uranusRZ(float3 p,float a){float s,c;sincos(a,s,c);return float3(c*p.x-s*p.y,s*p.x+c*p.y,p.z);}
// The orbital normal is world +Y. Tilt its rotation axis in the image plane,
// viewed near equinox, so the characteristic sideways rotation is readable.
// Geometry does not spin: an oblate body's silhouette is rotation invariant.
float3 uranusToBody(float3 p){return uranusRZ(p,-radians(97.8));}
float3 uranusToWorld(float3 p){return uranusRZ(p,radians(97.8));}
float2 uranusScreen(float2 uv){float2 p=(uv-.5)*resolution/min(resolution.x,resolution.y);p.y=-p.y;return p/.375;}
float3 uranusLight(){return normalize(float3(-.58,.20,.79));}
float3 uranusGrade(float3 c){
 return c*(palette<.5?float3(1,1,1):palette<1.5?float3(.97,1.035,1.015):palette<2.5?float3(.90,.99,1.10):float3(1.07,1.015,.97));
}
