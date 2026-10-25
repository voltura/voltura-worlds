#include "common.hlsli"
#include "smoke_detail_clock.hlsli"
Texture3D<float4> velocityField:register(t0);
Texture3D<float4> coordinates:register(t1);
RWTexture3D<float4> outputDetail:register(u1);
RWByteAddressBuffer distortionAudit:register(u0);
RWTexture3D<float4> potentialOutput:register(u7);
Texture3D<float4> potentialField:register(t3);
SamplerState linearClamp:register(s0);
float4 cubicWeights(float t){
 float t2=t*t,t3=t2*t;
 return float4(-.5*t+t2-.5*t3,1-2.5*t2+1.5*t3,.5*t+2*t2-1.5*t3,-.5*t2+.5*t3);
}
float4 reconstructReference(float3 p){
 float3 cell=p*spare1.yzw-.5;int3 base=(int3)floor(cell);float3 f=frac(cell);
 float4 wx=cubicWeights(f.x),wy=cubicWeights(f.y),wz=cubicWeights(f.z);
 float4 value=0,lo=1e10,hi=-1e10;
 [loop]for(int z=0;z<4;z++)[loop]for(int y=0;y<4;y++)[unroll]for(int x=0;x<4;x++){
  float4 sample=potentialField.Load(int4(clamp(base+int3(x,y,z)-1,0,(int3)spare1.yzw-1),0));
  value+=sample*(wx[x]*wy[y]*wz[z]);
  if(x>=1&&x<=2&&y>=1&&y<=2&&z>=1&&z<=2){lo=min(lo,sample);hi=max(hi,sample);}
 }
 return clamp(value,lo,hi);
}
float3 spectralPotential(float3 p){
 float3 result=0;
 [unroll]for(int i=0;i<6;i++){
  float3 k=normalize(float3(sin(i*7.13+1.7),cos(i*4.71+.3),sin(i*3.17+2.4)));
  float3 a=normalize(float3(cos(i*5.37+.8),sin(i*8.23+1.1),cos(i*2.71+.6)));
  float frequency=i<3?75:137;
  result+=a*sin(dot(k,p)*frequency+i*3.77)*((i<3?1:.63)/frequency);
 }
 return result;
}
float3 potential(int3 cell){
 float3 size=spare1.yzw;
 if(any(cell<0)||any(cell>=(int3)size))return 0;
 float3 p=(float3(cell)+.5)/size;
 float3 samplePos=clamp(p,.5/grid,1-.5/grid);
 float3 a=coordinates.SampleLevel(linearClamp,float3(samplePos.x*.5,samplePos.yz),0).xyz;
 float3 b=coordinates.SampleLevel(linearClamp,float3((samplePos.x+1)*.5,samplePos.yz),0).xyz;
 float weight=pow(sin(time*3.14159265359/smokeDetailPeriod()),2);
 float3 edge=min(p,1-p),fade=smoothstep(0,.08,edge);
 float3 v=velocityField.SampleLevel(linearClamp,p,0).xyz;
 if(spare1.x>.5){float3 q=p-.5;v=float3(-q.y,q.x,0)*(6.28318530718/12);}
 float energy=smoothstep(.01,.15,length(v));
 return lerp(spectralPotential(b),spectralPotential(a),weight)*(.04*turbulence)*fade.x*fade.y*fade.z*energy;
}
[numthreads(4,4,4)]
void CS(uint3 id:SV_DispatchThreadID){
 if(any(id>=(uint3)spare1.yzw))return;
 int3 p=id;
 if(spare2.y>1.5){
  float3 position=(float3(id)+.5)/spare1.yzw;
  // Clamp within chart zero before packing X; otherwise interpolation can
  // blend its last cell with the unrelated first cell of chart one.
  float3 mapPosition=clamp(position,.5/grid,1-.5/grid);
  float3 mapped=coordinates.SampleLevel(linearClamp,float3(mapPosition.x*.5,mapPosition.yz),0).xyz;
  float4 value=spare0.x>.5?reconstructReference(mapped):potentialField.SampleLevel(linearClamp,mapped,0);
  if(spare2.y>2.5){
   value=value*exp(-spare0.y*.22)+velocityField.Load(int4(id,0));
   float pigment=dot(value.xyz,1);
   if(pigment>0)value.xyz*=value.w/pigment;else value=0;
   value*=min(1,8/max(value.w,1e-20));
  }
  if(spare2.y>2.5&&pass>4.5&&all((id&1)==0)&&value.w>.012){
   int3 c=(int3)(position*grid);
   if(all(c>0)&&all(c<(int)grid-1)){
    float3 dx=(coordinates.Load(int4(c+int3(1,0,0),0)).xyz-coordinates.Load(int4(c-int3(1,0,0),0)).xyz)*(.5*grid);
    float3 dy=(coordinates.Load(int4(c+int3(0,1,0),0)).xyz-coordinates.Load(int4(c-int3(0,1,0),0)).xyz)*(.5*grid);
    float3 dz=(coordinates.Load(int4(c+int3(0,0,1),0)).xyz-coordinates.Load(int4(c-int3(0,0,1),0)).xyz)*(.5*grid);
    float determinant=dot(dx,cross(dy,dz));
    distortionAudit.InterlockedAdd(0,1);
    if(!isfinite(determinant)||determinant<.5||determinant>2||max(dot(dx,dx),max(dot(dy,dy),dot(dz,dz)))>9)distortionAudit.InterlockedAdd(4,1);
   }
  }
  potentialOutput[id]=value;return;
 }

 if(pass<.5){potentialOutput[p]=float4(potential(p),0);return;}
 // Curl the complete world-space potential, including deformation, energy
 // and boundary taper. Paired central derivatives commute on this grid.
 // Out-of-range Texture.Load returns zero, matching the potential boundary.
 float3 dx=(potentialField.Load(int4(p+int3(1,0,0),0)).xyz-potentialField.Load(int4(p-int3(1,0,0),0)).xyz)*(.5*spare1.y);
 float3 dy=(potentialField.Load(int4(p+int3(0,1,0),0)).xyz-potentialField.Load(int4(p-int3(0,1,0),0)).xyz)*(.5*spare1.z);
 float3 dz=(potentialField.Load(int4(p+int3(0,0,1),0)).xyz-potentialField.Load(int4(p-int3(0,0,1),0)).xyz)*(.5*spare1.w);
 outputDetail[id]=float4(dy.z-dz.y,dz.x-dx.z,dx.y-dy.x,0);
}
