#include "common.hlsli"
Texture2D<float4> artwork:register(t1);
SamplerState artworkSampler:register(s0);
float4 artworkCubicWeights(float t){
 return float4(-.5*t+t*t-.5*t*t*t,1-2.5*t*t+1.5*t*t*t,
               .5*t+2*t*t-1.5*t*t*t,-.5*t*t+.5*t*t*t);
}
float3 sampleArtwork(float2 p){
 uint width,height;artwork.GetDimensions(width,height);
 float2 size=float2(width,height),dx=ddx(p),dy=ddy(p);
 float bias=exp2((1-saturate(detail))*.6);
 // Keep mip filtering when reducing artwork. Reconstruct only magnification.
 float footprint=max(length(dx*size),length(dy*size))*bias;
 float3 filtered=artwork.SampleGrad(artworkSampler,p,dx*bias,dy*bias).rgb;
 if(footprint>=1)return filtered;
 float2 coordinate=p*size-.5;int2 base=(int2)floor(coordinate);
 float4 wx=artworkCubicWeights(frac(coordinate.x));
 float4 wy=artworkCubicWeights(frac(coordinate.y));
 // Combine the two positive middle weights using hardware bilinear filtering.
 // Negative outer lobes remain separate, preserving Catmull-Rom's sharpness.
 float3 weightX=float3(wx.x,wx.y+wx.z,wx.w);
 float3 weightY=float3(wy.x,wy.y+wy.z,wy.w);
 float3 positionX=(base.x+float3(-.5,.5+wx.z/weightX.y,2.5))/size.x;
 float3 positionY=(base.y+float3(-.5,.5+wy.z/weightY.y,2.5))/size.y;
 float3 result=0,low=1e10,high=-1e10;
 [unroll]for(int y=0;y<3;y++)[unroll]for(int x=0;x<3;x++)
  result+=artwork.SampleLevel(artworkSampler,float2(positionX[x],positionY[y]),0).rgb*weightX[x]*weightY[y];
 [unroll]for(int y=0;y<2;y++)[unroll]for(int x=0;x<2;x++){
  int2 texel=clamp(base+int2(x,y),0,int2(width,height)-1);
  float3 value=artwork.Load(int3(texel,0)).rgb;
  low=min(low,value);high=max(high,value);
 }
 // Bound ringing at luminous contours; interpolation must not invent hot halos.
 return lerp(clamp(result,low,high),filtered,smoothstep(.75,1,footprint));
}
float3 fixedStars(float2 screenUV) {
 // Distant sky uses only screen coordinates: no time, flow, palette or audio.
 float2 p=screenUV*float2(resolution.x/resolution.y,1)*90;
 float2 cell=floor(p),local=frac(p);
 float id=hash(float3(cell,71));
 float2 center=.25+.5*float2(hash(float3(cell,19)),hash(float3(cell,43)));
 float2 distancePixels=(local-center)*(resolution.y/90);
 float radius=lerp(.35,.70,hash(float3(cell,97)))*max(1,resolution.y/1080);
 float profile=exp(-dot(distancePixels,distancePixels)/(radius*radius));
 float strength=lerp(.02,.16,pow(hash(float3(cell,113)),3));
 float3 tint=lerp(float3(.65,.78,1),float3(1,.88,.68),hash(float3(cell,127)));
 float3 coarse=tint*profile*strength*step(.978,id);
 // A second stationary population restores fine gold detail lost when sky
 // points were removed from the moving artwork. Positions never use time.
 float2 finePosition=screenUV*float2(resolution.x/resolution.y,1)*180;
 float2 fineCell=floor(finePosition),fineLocal=frac(finePosition);
 float fineId=hash(float3(fineCell,211));
 float2 fineCenter=.2+.6*float2(hash(float3(fineCell,223)),hash(float3(fineCell,227)));
 float2 fineDistance=(fineLocal-fineCenter)*(resolution.y/180);
 float fineRadius=lerp(.35,.60,hash(float3(fineCell,229)))*max(1,resolution.y/1080);
 float fineProfile=exp(-dot(fineDistance,fineDistance)/(fineRadius*fineRadius));
 float composition=smoothstep(.25,.72,screenUV.x);
 float fineStrength=lerp(.35,1.2,pow(hash(float3(fineCell,233)),2));
 return coarse+float3(1,.60,.20)*fineProfile*fineStrength*composition*step(.992,fineId);
}
// The artwork contains no sky particles. All deformation is confined to it.
float3 authoredAurora(float2 uv){
 float t=time*movement;
 float aspect=resolution.x/resolution.y;
 float2 fit=float2(min(1,aspect/(1672.0/941.0)),min(1,(1672.0/941.0)/aspect));
 float2 p=(uv-.5)*fit+.5;
 float edge=saturate(16*p.x*(1-p.x)*p.y*(1-p.y));
 float2 displacement=float2(sin(p.y*5.3+t*.24)+.38*sin(p.y*11.1-t*.17),cos(p.x*5.7-t*.19)+.3*sin(p.y*7.3+t*.31));
 p+=displacement*float2(.018,.024)*edge*(1+energy*reaction*.2);
 float3 sampleColor=sampleArtwork(p);
 // Invert the existing display curve to preserve authored contrast and color.
 float3 encoded=lerp(12.92*sampleColor,1.055*pow(max(sampleColor,0),1/2.4)-.055,step(.0031308,sampleColor));
 float3 radiance=-log(max(.004,1-pow(saturate(encoded),2.2)));
 if(palette>.5 && palette<1.5)radiance=radiance.gbr;
 else if(palette>=1.5 && palette<2.5)radiance=radiance.brg;
 else if(palette>=2.5)radiance=dot(radiance,float3(.2126,.7152,.0722));
 return radiance*(1+energy*reaction*.12);
}
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET {
 float3 sky=fixedStars(uv);
 if(spare0.x>.5)return float4(tone(sky),1);
 return float4(tone(authoredAurora(uv)+sky),1);
}

