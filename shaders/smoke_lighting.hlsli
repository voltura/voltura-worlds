static const float SmokeExtinction=320;
static const float SmokeDensityCutoff=.012;
float smokeEnvelope(float3 p){
 float3 edge=min(p,1-p);
 return smoothstep(0,.12,edge.x)*smoothstep(0,.10,edge.z)*smoothstep(0,.055,edge.y);
}
float3 smokeLighting(float shadow,float3 gradient,float densityScale){
 float3 light=normalize(float3(.6,.6,-.7));
 float3 normal=-gradient/(length(gradient)+.00001);
 float facing=dot(normal,light);
 // A wrapped key keeps thin edges translucent; stronger shadow attenuation
 // retains dark cores. This uses the existing density gradient and light cache.
 float form=.10+.90*saturate((facing+.18)/1.18);
 float direct=exp(-shadow*densityScale*SmokeExtinction);
 float broadScatter=exp(-shadow*densityScale*(SmokeExtinction*.23))*.065;
 return float3(.006,.012,.022)+float3(1,.91,.83)*(direct*form*2.45+broadScatter);
}
