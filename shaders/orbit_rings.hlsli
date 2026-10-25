// One optical-depth profile drives both the visible rings and their shadows.
// Filter each radial octave before it becomes smaller than a screen pixel.
float ringNoise(float radial,float frequency,float key){
 float footprint=fwidth(radial*frequency);
 return lerp(.5,noise(float3(radial*frequency,key,0)),exp(-.5*footprint*footprint));
}
float ringOpticalDepth(float radial){
 float broad=ringNoise(radial,27,5);
 float bands=ringNoise(radial,170,0);
 float fine=ringNoise(radial,740,11);
 float filaments=ringNoise(radial,3600,23);
 float depth=.012+.08*broad+.75*pow(bands,3)+.25*pow(fine,3)+.05*filaments;
 // A dense middle belt, translucent inner veil and separated outer belt.
 depth*=lerp(.25,1,smoothstep(.07,.29,radial));
 depth*=1+.4*smoothstep(.28,.34,radial)*(1-smoothstep(.52,.57,radial));
 depth*=lerp(.015,1,smoothstep(.007,.013,abs(radial-.62)));
 depth*=lerp(.22,1,smoothstep(.0015,.004,abs(radial-.83)));
 return depth*.36*smoothstep(0,.018,radial)*smoothstep(1,.975,radial);
}
float ringScattering(float opticalDepth,float muView,float muLight,bool sameSide){
 if(sameSide)
  return muLight/(muView+muLight)*(1-exp(-opticalDepth*(1/muView+1/muLight)));
 // Integral of a thin particulate slab viewed from its unlit side.
 if(abs(muLight-muView)<.002)
  return opticalDepth/muView*exp(-opticalDepth/muView);
 return muLight/(muLight-muView)*(exp(-opticalDepth/muLight)-exp(-opticalDepth/muView));
}

