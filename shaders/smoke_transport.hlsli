// Capture-only analytic fields. Both density and coordinate transport use
// identical velocities, including the same temporal midpoint.
float3 smokeDiagnosticFlow(float3 p){
 if(spare2.w>.5){
  float2 s=sin(p.xy*3.14159265359);
  float2 t=sin(p.xy*6.28318530718);
  // Divergence-free, tangent to the unit-box boundary. The time factor
  // integrates to zero over 12 seconds, returning exact trajectories home.
  return float3(s.x*s.x*t.y,-s.y*s.y*t.x,0)*(.2*cos((time-dt*.5)*3.14159265359/12));
 }
 float3 q=p-.5;
 return float3(-q.y,q.x,0)*(6.28318530718/12);
}
