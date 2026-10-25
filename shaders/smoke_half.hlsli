// Explicit nearest-even rounding prevents repeated FP16 UAV stores from
// systematically truncating advected density toward zero.
float4 storeHalf(float4 value){
 float4 magnitude=min(abs(value),65504);
 uint4 bits=f32tof16(magnitude);
 float4 lower=f16tof32(bits),upper=f16tof32(min(bits+1,0x7bff));
 float4 below=magnitude-lower,above=upper-magnitude;
 uint4 increment=uint4(and(bits<0x7bff,or(below>above,and(below==above,(bits&1)!=0))));
 float4 rounded=f16tof32(bits+increment);
 return select(value<0,-rounded,rounded);
}
