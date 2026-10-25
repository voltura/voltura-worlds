static const float SmokeExtinction=320;
static const float SmokeDensityCutoff=.012;
float smokeEnvelope(float3 p){
 float3 edge=min(p,1-p);
 return smoothstep(0,.12,edge.x)*smoothstep(0,.10,edge.z)*smoothstep(0,.055,edge.y);
}
float3 smokeLighting(float shadow,float3 gradient,float densityScale){
 float3 light=normalize(float3(.6,.6,-.7));
 float form=.15+.85*saturate(dot(-gradient/(length(gradient)+.00001),light));
 float direct=exp(-shadow*densityScale*SmokeExtinction);
 float broadScatter=exp(-shadow*densityScale*(SmokeExtinction*.1875))*.08;
 return float3(.008,.015,.025)+float3(1,.91,.83)*(direct*form*2.3+broadScatter);
}
