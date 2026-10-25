// Standalone pressure audit kernel. Not selected by the wallpaper renderer.
cbuffer Parameters:register(b0){
 uint grid,source,destination,rhs,operation,wrapBoundary,coarseGrid,coarseBase,coarseSource;
};
RWStructuredBuffer<float> fields:register(u0);
uint indexAt(int3 p,uint n){
 p=wrapBoundary?((p%(int)n+(int)n)%(int)n):clamp(p,0,(int)n-1);
 return (p.z*n+p.y)*n+p.x;
}
float valueAt(int3 p,uint base,uint n){return fields[base+indexAt(p,n)];}
float laplacian(int3 p){
 return valueAt(p+int3(1,0,0),source,grid)+valueAt(p-int3(1,0,0),source,grid)
 +valueAt(p+int3(0,1,0),source,grid)+valueAt(p-int3(0,1,0),source,grid)
 +valueAt(p+int3(0,0,1),source,grid)+valueAt(p-int3(0,0,1),source,grid)-6*valueAt(p,source,grid);
}
[numthreads(4,4,4)]
void CS(uint3 id:SV_DispatchThreadID){
 uint extent=operation==1?coarseGrid:grid;if(any(id>=extent))return;
 if(operation==0){
  uint at=indexAt(id,grid);float p=fields[source+at];
  fields[destination+at]=p+(laplacian(id)-fields[rhs+at])*(.8/6);return;
 }
 if(operation==1){
  float residual=0;
  [unroll]for(int z=0;z<2;z++)[unroll]for(int y=0;y<2;y++)[unroll]for(int x=0;x<2;x++){
   int3 p=id*2+int3(x,y,z);residual+=fields[rhs+indexAt(p,grid)]-laplacian(p);
  }
  uint at=indexAt(id,coarseGrid),cells=coarseGrid*coarseGrid*coarseGrid;
  fields[coarseBase+at]=0;fields[coarseBase+cells+at]=0;fields[coarseBase+2*cells+at]=residual*.5;return;
 }
 float3 q=(float3(id)+.5)*.5-.5;int3 base=(int3)floor(q);float3 f=frac(q);float correction=0;
 [unroll]for(int z=0;z<2;z++)[unroll]for(int y=0;y<2;y++)[unroll]for(int x=0;x<2;x++){
  float3 w=lerp(1-f,f,float3(x,y,z));correction+=valueAt(base+int3(x,y,z),coarseSource,coarseGrid)*w.x*w.y*w.z;
 }
 uint at=indexAt(id,grid);fields[destination+at]=fields[source+at]+correction;
}
