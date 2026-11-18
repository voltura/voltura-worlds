#include "common.hlsli"
RWTexture3D<float> pressureIn:register(u4);
RWTexture3D<float> pressureOut:register(u5);
RWTexture3D<float> divergence:register(u6);

int3 bound(int3 p){return clamp(p,0,(int)grid-1);}

[numthreads(4,4,4)]
void CS(uint3 id:SV_DispatchThreadID){
 int3 p=id;
 pressureOut[p]=(pressureIn[bound(p+int3(1,0,0))]+pressureIn[bound(p-int3(1,0,0))]+
                 pressureIn[bound(p+int3(0,1,0))]+pressureIn[bound(p-int3(0,1,0))]+
                 pressureIn[bound(p+int3(0,0,1))]+pressureIn[bound(p-int3(0,0,1))]-divergence[p])/6;
}
