cbuffer Frame : register(b0) {
 float2 resolution; float time; float dt;
 float bass; float mid; float treble; float energy;
 float brightness; float movement; float detail; float flow;
 float turbulence; float density; float reaction; float palette;
 float grid; float pass; float impulse; float seed;
 float4 spare0; float4 spare1; float4 spare2;
};
float hash(float3 p) { p=frac(p*.1031);p+=dot(p,p.yzx+33.33);return frac((p.x+p.y)*p.z); }
float noise(float3 p) {
 float3 i=floor(p),f=frac(p);f=f*f*(3-2*f);
 return lerp(lerp(lerp(hash(i),hash(i+float3(1,0,0)),f.x),lerp(hash(i+float3(0,1,0)),hash(i+float3(1,1,0)),f.x),f.y),
 lerp(lerp(hash(i+float3(0,0,1)),hash(i+float3(1,0,1)),f.x),lerp(hash(i+float3(0,1,1)),hash(i+1),f.x),f.y),f.z);
}
float fbm(float3 p) {float v=0,a=.5;[unroll]for(int i=0;i<5;i++){v+=noise(p)*a;p=p*2.03+float3(4.1,1.7,9.2);a*=.5;}return v;}
// Scene shaders retain linear HDR energy. Display mapping occurs once in post.
float3 tone(float3 c) {return max(c,0)*brightness;}
// Screen position follows Orbit's projection as the viewport aspect changes.
float2 orbitSunUV(){return float2(.5+.5793464/(resolution.x/resolution.y),.2364706);}
float3 colorA(){return palette<.5?float3(.025,.65,.9):palette<1.5?float3(.1,.7,.35):palette<2.5?float3(.7,.13,.04):float3(.45,.45,.55);}
float3 colorB(){return palette<.5?float3(.6,.04,.65):palette<1.5?float3(.08,.2,.85):palette<2.5?float3(.9,.5,.08):float3(.6,.65,.75);}


#ifdef JOURNEY
// Projected center (short-edge units, y up) and globe radius supplied per body.
float2 journeyScreen(float2 uv){
 float2 p=(uv-.5)*resolution/min(resolution.x,resolution.y);p.y=-p.y;
 return (p-spare2.xy)/max(spare2.z,.000001);
}
#endif
