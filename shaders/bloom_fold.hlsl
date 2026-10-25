#include "bloom_surface.hlsli"
float4 PS(BloomVertex input,bool front:SV_IsFrontFace):SV_TARGET{
 float3 n=normalize(input.normal);
 float3 eye=normalize(float3(0,0,3.7)-input.world);
 if(dot(n,eye)<0)n=-n;
 float3 light=normalize(float3(-.7,1.3,1.8));
 float diffuse=saturate(dot(n,light));
 float rim=pow(1-saturate(dot(n,eye)),3);
 float spec=pow(saturate(dot(n,normalize(light+eye))),52);
 float soft=pow(saturate(dot(n,normalize(light+eye))),9);
 float3 base=palette<.5?float3(.008,.065,.85):palette<1.5?float3(.012,.42,.26):palette<2.5?float3(.68,.07,.025):float3(.25,.30,.43);
 float3 edge=palette<.5?float3(.03,.52,1):palette<1.5?float3(.08,.85,.65):palette<2.5?float3(1,.42,.12):float3(.65,.78,.95);
 float cavity=.40+.60*smoothstep(.0,.5,input.uv.y);
 float3 c=base*(.10+diffuse*1.5)*cavity+edge*(soft*.28+rim*.48)+float3(.65,.82,1)*spec*.7;
 float lip=smoothstep(.988,1,input.uv.y);
 c+=edge*lip*.22;
 return float4(tone(c),1);
}
