Texture2D<float4> card:register(t3);
SamplerState linearClamp:register(s0);
float4 PS(float4 position:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET {
 return card.SampleLevel(linearClamp,uv,0);
}
