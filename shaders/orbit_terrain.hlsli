// Shared differentiable terrain: xyz is gradient, w is height.
float4 rockNoiseGradient(float3 p){
 float3 cell=floor(p),f=frac(p);
 float3 w=f*f*f*(f*(f*6-15)+10),dw=30*f*f*(f*(f-2)+1);
 float4 result=0;
 [unroll]for(int z=0;z<2;z++)[unroll]for(int y=0;y<2;y++)[unroll]for(int x=0;x<2;x++){
  float3 corner=float3(x,y,z),weight=lerp(1-w,w,corner),derivative=dw*(corner*2-1);
  result+=hash(cell+corner)*float4(derivative.x*weight.y*weight.z,
    weight.x*derivative.y*weight.z,weight.x*weight.y*derivative.z,weight.x*weight.y*weight.z);
 }
 return result;
}
float4 moonTerrain(float3 n){
 float4 broad=rockNoiseGradient(n*3.1+11.7);
 float4 ridges=rockNoiseGradient(n*7.3+27.4);
 return float4(broad.xyz*(3.1*.055)+ridges.xyz*(7.3*.018),
               (broad.w-.5)*.055+(ridges.w-.5)*.018);
}

float4 craters(float3 p){
 float3 cell=floor(p);float closest=10;float3 nearestOffset=0;
 [unroll]for(int z=-1;z<=1;z++)[unroll]for(int y=-1;y<=1;y++)[unroll]for(int x=-1;x<=1;x++){
  float3 id=cell+float3(x,y,z);
  float3 center=id+float3(hash(id),hash(id+17.3),hash(id+41.7));
  float craterScale=.65+.7*hash(id+57.9);
  float3 offset=p-center;float distance=length(offset)/craterScale;
  if(distance<closest){closest=distance;nearestOffset=offset/(craterScale*craterScale);}
 }
 float bowl=-exp(-closest*closest*18)*.6;
 float rim=exp(-pow((closest-.38)*17,2))*.18;
 float derivative=-36*closest*bowl-578*(closest-.38)*rim;
 return float4(nearestOffset*(derivative/max(closest,.00001)),bowl+rim);
}
