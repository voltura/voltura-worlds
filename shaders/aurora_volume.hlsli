Texture3D<float> auroraNoiseTexture:register(t0);
SamplerState auroraNoiseSampler:register(s2);
float auroraNoise(float3 p){
 float3 cell=floor(p),f=frac(p);f=f*f*(3-2*f);
 return auroraNoiseTexture.SampleLevel(auroraNoiseSampler,(cell+f+.5)/128,0);
}
// Native-resolution emissive wisps. Each layer has independent nonperiodic
// density and curvature; stars are composed separately by aurora.hlsl.
float auroraFractal(float3 p){
 float f=0,amplitude=.53,totalWeight=0;
 float footprint=max(length(ddx(p)),length(ddy(p)));
 [unroll]for(int octave=0;octave<6;octave++){
  float weight=amplitude*(octave<3?1:saturate(detail))*(1-smoothstep(.35,1,footprint));f+=auroraNoise(p)*weight;totalWeight+=weight;p=float3(dot(p,float3(0,.8,.6)),dot(p,float3(-.8,.36,-.48)),dot(p,float3(-.6,-.48,.64)))*2.11+float3(7.3,3.8,11.1);amplitude*=.47;footprint*=2.11;
 }
 return totalWeight>.001?f*.9770655/totalWeight:.5;
}
float auroraCurveSlope(float x,float id){
 if(id<3)return -2*(1.25+id*1.55)*(x-(.67+.009*(id-1)));
 if(id<6){
  float steepness=3.8+(id-3)*2.7;
  float turn=tanh((x-.735+.006*(id-4))*steepness);
  return -.54*steepness*(1-turn*turn);
 }
 return 2*(.25+(id-6)*1.65)*(x-.71);
}
// Arc length keeps the texture scale consistent through steep bends.
// Four-point Gauss integration is smooth even as x crosses the anchor.
float auroraArc(float x,float id,float aspect){
 float midpoint=(x+.66)*.5,halfWidth=(x-.66)*.5;
 float nodes[4]={-.86113631,-.33998104,.33998104,.86113631};
 float weights[4]={.34785485,.65214515,.65214515,.34785485};
 float integral=0;
 [unroll]for(int i=0;i<4;i++){
  float slope=auroraCurveSlope(midpoint+halfWidth*nodes[i],id);
  integral+=weights[i]*sqrt(aspect*aspect+slope*slope);
 }
 return integral*halfWidth;
}
float3 auroraVolume(float2 uv){
 float aspect=resolution.x/resolution.y;
 float2 p=(uv-float2(.66,.53))*float2(aspect,1);
 float t=time*movement*.12;
 float3 color=0;
 [unroll]for(int layer=0;layer<9;layer++){
  float id=float(layer),seed=id*19.31;
  float2 pivot=layer<6?float2(.68,.47):float2(.74,.73);
  float angle=layer<3?-.40:layer<6?.08:.22;
  float sn,cs;sincos(angle,sn,cs);
  float2 offset=(uv-pivot)*float2(aspect,1);
  float2 localUV=pivot+float2(cs*offset.x-sn*offset.y,sn*offset.x+cs*offset.y)/float2(aspect,1);
  float x=localUV.x,center,slope;
  if(layer<3){
   float pivot=.67+.009*(id-1);
   float curvature=1.25+id*1.55;
   center=.47-curvature*(x-pivot)*(x-pivot)+.014*(id-1);
   slope=-2*curvature*(x-pivot);
  }else if(layer<6){
   float steepness=3.8+(id-3)*2.7;
   float turn=tanh((x-.735+.006*(id-4))*steepness);
   center=.48-.54*turn+.018*(id-4);
   slope=-.54*steepness*(1-turn*turn);
  }else{
   float curvature=.25+(id-6)*1.65;
   center=.72+curvature*(x-.71)*(x-.71)+.014*(id-7);
   slope=2*curvature*(x-.71);
  }
  center+=(noise(float3(x*3.3,seed,t*.24))-.5)*.025;
  float2 q=float2((localUV.x-.66)*aspect,(localUV.y-center)/sqrt(1+pow(slope/aspect,2)));
  float convergence=layer<6?.68:.74;
  float offsetFromFocus=x-convergence;
  float spread=sqrt(offsetFromFocus*offsetFromFocus+.0025)-.05;
  q.y/=.85+1.60*(1-exp(-spread*2.88));
  float envelope=exp(-q.y*q.y/(.0028+.0007*fmod(id,3)));
  [branch]if(envelope<.0004)continue;
  float2 flowCoordinate=float2(auroraArc(x,id,aspect)
    +(localUV.y-center)*slope/sqrt(aspect*aspect+slope*slope),q.y);
  [unroll]for(int vortex=0;vortex<2;vortex++){
   float key=seed+float(vortex)*37.3;
   float2 center=float2((hash(float3(key,11,7))-.5)*1.25,(hash(float3(key,17,3))-.5)*.13);
   center.x+=.025*sin(t*.18+key);
   float2 offset=flowCoordinate-center;
   float radius=.085+.085*hash(float3(key,23,2));
   float angle=(vortex==0?1:-1)*.55*exp(-dot(offset,offset)/(radius*radius));
   float sn,cs;sincos(angle,sn,cs);
   flowCoordinate=center+float2(cs*offset.x-sn*offset.y,sn*offset.x+cs*offset.y);
  }
  float3 field=float3(flowCoordinate.x*4.3-t*.36,flowCoordinate.y*29.0,seed+t*.09);
  float warp=auroraFractal(field*.76);
  field.xy+=float2(warp*.32,warp*1.4);
  float cloud=auroraFractal(field);
  float fine=auroraFractal(field*4.3+float3(11,7,13));
  // A continuous translucent sheet carries the detail. Isocontours of
  // fractal noise produced disconnected bright threads instead of light.
  float fold=auroraNoise(float3(flowCoordinate.x*2.1-t*.08,seed, t*.035));
  float sheetDistance=flowCoordinate.y+(fold-.5)*.085+(cloud-.5)*.003;
  float sheetWidth=.045+.055*auroraNoise(float3(flowCoordinate.x*2.7,seed+9,t*.04));
  float edgeWidth=max(fwidth(sheetDistance),.0004);
  float sheet=exp(-pow(sheetDistance/(sheetWidth*1.6),2));
  float filaments=pow(saturate(fine*1.8-.35),3);
  float textureDensity=pow(saturate(cloud*2.3-.65),4)*.45+pow(filaments,1.6)*1.2;
  float wisps=envelope*sheet*textureDensity*.65;
  // Independently wandering thin veils supply overlapping fine structure.
  // Their spacing and paths are irregular rather than a periodic stripe mask.
  float fineVeils=0;
  [unroll]for(int veil=0;veil<5;veil++){
   float veilSeed=seed+float(veil)*13.79+81;
   float along=flowCoordinate.x;
   float path=(auroraNoise(float3(along*2.9-t*.10,veilSeed,t*.035))-.5)*.12;
   path+=(hash(float3(veilSeed,17,3))-.5)*.055;
   float crossDistance=flowCoordinate.y-path;
   float breakup=auroraNoise(float3(along*13.7-t*.3,veilSeed+11,t*.06));
   float width=.0012+.0038*hash(float3(veilSeed,7,19));
   float filteredWidth=max(width,fwidth(crossDistance));
   float ridge=exp(-pow(crossDistance/filteredWidth,2))*(width/filteredWidth);
   float haze=exp(-pow((crossDistance-width*2)/(width*4),2));
   fineVeils+=(ridge*.09+haze*.035)*pow(saturate(breakup*1.7-.4),2);
  }
  wisps+=envelope*fineVeils*saturate(detail)*.35;
  float3 hues[9]={float3(.002,.58,.18),float3(.003,.43,.64),float3(.003,.36,.17),float3(.003,.65,.38),float3(.008,.10,.82),float3(.23,.003,.78),float3(.006,.17,.75),float3(.20,.004,.65),float3(.003,.45,.30)};
  float3 hue=hues[layer];
  if(palette>.5)hue=lerp(colorA(),colorB(),id/8);
  color+=hue*wisps*1.9;
  // Sparse luminous particles belong to the moving gas; the distant stars do not.
  float2 particlePosition=float2(flowCoordinate.x*105-t*.9,flowCoordinate.y*210);
  float2 cell=floor(particlePosition),local=frac(particlePosition);
  float particleId=hash(float3(cell,seed+207));
  float2 particleCenter=.2+.6*float2(hash(float3(cell,seed+211)),hash(float3(cell,seed+223)));
  float2 particleDx=ddx(particlePosition),particleDy=ddy(particlePosition);
  float determinant=particleDx.x*particleDy.y-particleDx.y*particleDy.x;
  float2 delta=local-particleCenter;
  float inverse=sign(determinant)/max(abs(determinant),.000001);
  float2 pixelDistance=float2(delta.x*particleDy.y-delta.y*particleDy.x,particleDx.x*delta.y-particleDx.y*delta.x)*inverse;
  float particle=exp(-dot(pixelDistance,pixelDistance)/.65)*step(.992,particleId)*step(.000001,abs(determinant));
  color+=float3(.85,.48,.10)*particle*envelope*(.15+filaments)*saturate(detail);

  // Sparse gold accents, with smoothly varying width and intensity.
  if(layer==1||layer==4||layer==7){
   float authoredWidth=.00045+.0008*noise(float3(q.x*5,seed,t*.3));
   float width=max(fwidth(q.y)*.7,authoredWidth);
   float goldDistance=q.y+.018+.020*sin(q.x*2.8+id*.7+t*.15);
   float filament=exp(-pow(goldDistance/width,2));
   float focalX=layer==1?.66:layer==4?.78:.55;
   float hotSpot=exp(-pow((uv.x-focalX)/.015,2));
   float intensity=.025+pow(noise(float3(q.x*2.7,seed+13,t*.2)),3)+hotSpot*2;
   color+=float3(1,.55,.14)*exp(-pow(goldDistance/.003,2))*hotSpot*.25;
   color+=float3(1,.67,.21)*filament*intensity*1.8*(authoredWidth/width);
   float split=.003+.013*noise(float3(q.x*3.8,seed+31,t*.23));
   float branchWidth=max(width*.55,fwidth(q.y)*.6);
   float branches=exp(-pow((goldDistance-split)/branchWidth,2))+exp(-pow((goldDistance+split*.65)/branchWidth,2));
   color+=float3(1,.61,.15)*branches*min(intensity,.45)*.20*(authoredWidth/branchWidth)*saturate(detail);
  }
 }
 float2 focusA=(uv-float2(.68,.47))*float2(aspect,1);
 float2 focusB=(uv-float2(.74,.73))*float2(aspect,1);
 float focus=1+2.2*exp(-dot(focusA,focusA)*85)+1.5*exp(-dot(focusB,focusB)*110);
 return color*focus*smoothstep(.01,.42,uv.x)*(1+energy*reaction*.3);
}








