cbuffer OutputData:register(b1){float outputHdr,whiteScale,peakRatio,outputReserved;};
Texture2D<float4> card:register(t3);
SamplerState linearClamp:register(s0);
float4 PS(float4 position:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET {
 float4 c=card.SampleLevel(linearClamp,uv,0);
 if(outputHdr>.5)c.rgb=float3(c.r<=.04045?c.r/12.92:pow((c.r+.055)/1.055,2.4),c.g<=.04045?c.g/12.92:pow((c.g+.055)/1.055,2.4),c.b<=.04045?c.b/12.92:pow((c.b+.055)/1.055,2.4))*whiteScale;
 c.a*=outputReserved;
 return c;
}
