// World-space bottom: a shallow foreground shelf slopes into deeper water.
// Surface ripples only refract the view of it; they never move the stones.
float waterBedDepth(float2 p) {
 float shelf=p.y+.12*sin(p.x*1.8)+.08*noise(float3(p*1.4,12.));
 return .12+1.45*smoothstep(-.85,1.2,shelf)+.06*noise(float3(p*2.3,31.));
}

float3 waterBedColor(float2 p) {
 float sand=noise(float3(p*3.4,25.));
 float grain=noise(float3(p*150.,5.));
 float3 color=lerp(float3(.25,.22,.145),float3(.43,.38,.265),sand)*(.92+.08*grain);
 float2 q=p*10.,cell=floor(q);
 float closest=2.,radius=1.,identity=0.,toneIdentity=0.;float2 stoneDelta=0.;
 [unroll]for(int y=-1;y<=1;y++)[unroll]for(int x=-1;x<=1;x++) {
  float2 id=cell+float2(x,y);
  float h=hash(float3(id,72.));
  float patch=smoothstep(.32,.65,noise(float3(id*.21,16.)));
  if(h<patch*.8) {
   float2 centre=id+.05+.9*float2(hash(float3(id,18.)),hash(float3(id,51.)));
   float2 offset=q-centre;
   float angle=hash(float3(id,27.))*6.2831853;
   float2 d=float2(dot(offset,float2(cos(angle),sin(angle))),dot(offset,float2(-sin(angle),cos(angle))));
   d*=float2(.8+.9*hash(float3(id,42.)),.8+.8*hash(float3(id,39.)));
   float size=hash(float3(id,91.));
   float r=.09+.43*pow(size,1.6);
   float lengthD=length(d);
   float2 n=d/max(lengthD,.0001);
   // Rounded asymmetric outlines, with independent size and elongation.
   // Low-order angular harmonics stay smooth rather than making spiky rocks.
   float asymmetry=hash(float3(id,44.))*2.-1.;
   float outline=1.+.10*(n.x*n.x*n.x-3.*n.x*n.y*n.y)+.08*(2.*n.x*n.y)*asymmetry+.05*n.x;
   r*=outline;
   float distance=lengthD-r;
   if(distance<closest){closest=distance;radius=r;stoneDelta=d;identity=hash(float3(id,82.));toneIdentity=hash(float3(id,103.));}
  }
 }
 float aa=max(fwidth(closest),.008);
 float rock=1.-smoothstep(-aa,aa,closest);
 float2 xy=stoneDelta/max(radius,.01);
 float dome=sqrt(saturate(1.-dot(xy,xy)));
 float3 normal=normalize(float3(xy,dome+.15));
 float light=.62+.38*saturate(dot(normal,normalize(float3(-.38,.48,1.))));
 // Natural stone families remain separate from the water palette: slate,
 // charcoal, warm brown, pale quartz, muted green and sandy tan.
 float family=identity*6.;
 float3 stone=family<1.?float3(.25,.29,.31):family<2.?float3(.075,.09,.095):
              family<3.?float3(.32,.19,.12):family<4.?float3(.55,.52,.43):
              family<5.?float3(.19,.24,.17):float3(.40,.34,.23);
 stone*=.75+.5*toneIdentity;
 float mottling=noise(float3(p*83.,identity*8.));
 stone*=light*(.82+.12*grain+.12*mottling);
 color*=1.-.13*exp(-max(closest,0.)*18.);
 return lerp(color,stone,rock);
}
