#include "common.hlsli"
Texture2D<float4> observation:register(t0);
Texture2D<float4> prominenceMap:register(t1);
SamplerState linearClamp:register(s0);

float3 sunColor(float intensity){
 float3 tint=palette<.5?float3(1,.055,.003):palette<1.5?float3(1,.52,.055):palette<2.5?float3(1,.15,.008):float3(.82,.85,.9);
 // The observation is EUV, so every palette is explicitly false color.
 float3 hot=palette<.5?float3(1,.50,.15):float3(1,.75,.37);
 return tint*intensity+hot*pow(saturate(intensity*.23),4)*.32*(palette<2.5?1:0);
}

// Actual AIA 304 emission supplies the shapes and natural fine structure.
// Only bounded image advection and emission variation are artistic animation.
float3 prominenceEmission(float2 q,float t,float active,int quality){
 float r=length(q);float2 radial=q/max(r,.001),tangent=float2(-radial.y,radial.x);
 float height=saturate((r-.91)/.40);
 float angle=atan2(q.y,q.x);
 float stream=noise(float3(q*9,t*.15));
 float breath=sin(t*.17+angle*3)*.013*height;
 float2 displacement=radial*(breath+(stream-.5)*.014*height);
 displacement+=tangent*(sin(t*.21+q.x*12+q.y*8)*.009*height);
 float2 moving=q+displacement*min(active,1.5);
 float2 coord=.5+moving*float2(1,-1)*(1605.0/4096.0);
 float3 observed=prominenceMap.Sample(linearClamp,coord).rgb;
 // Do not amplify tiny JPEG/BC7 chroma residuals into blue fringes on black.
 observed=max(observed-.0006,0);
 observed.b=min(observed.b,observed.g*.6);
 float cells=noise(float3(q*22,t*.23));
 if(quality>0)cells=cells*.75+noise(float3(q*45+7,t*.31))*.25;
 float variation=.85+.30*cells;
 // Retain observed wispy density. Tiny bright roots are mapped toward warm
 // white, with lower intensity yellow/orange/red emission fading into space.
 float3 gas=pow(max(observed,0),.82)*float3(2.8,3.2,3.6);
 float hot=smoothstep(.12,.65,observed.g);
 gas+=float3(.65,.72,.80)*hot;
 return gas*variation;
}

// Compact circulation patches with independent centers, directions, clocks,
// and strengths. Their smooth supports leave quiet gaps instead of moving
// the photograph with a disk-wide wave or a visible rectangular grid.
float2 surfaceIslands(float2 q,float t,float active){
 const float spacing=.17;
 float2 cell=floor(q/spacing),warp=0;
 [unroll]for(int y=-1;y<=1;y++){
  [unroll]for(int x=-1;x<=1;x++){
   float2 id=cell+float2(x,y);
   float a=hash(float3(id,17)),b=hash(float3(id,43));
   float c=hash(float3(id,79)),d=hash(float3(id,113));
   float2 center=(id+.5+float2(a-.5,b-.5)*.55)*spacing;
   float2 local=q-center;
   float radius=spacing*(.62+.23*c);
   float r2=dot(local,local)/(radius*radius);
   [branch]if(r2<1){
    float support=1-r2;support=support*support*support;
    float direction=d>.5?1:-1;
    float phase=t*(.24+.45*a)+b*6.2831853;
    float angle=direction*(.35+.28*c)*sin(phase)*active;
    float sn,cs;sincos(angle,sn,cs);
    float2 rotated=float2(cs*local.x-sn*local.y,sn*local.x+cs*local.y);
    warp+=(rotated-local)*support*(.55+.45*d);
   }
  }
 }
 return warp;
}

float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET{
 float t=time*movement;
 float2 p=(uv-.5)*resolution/min(resolution.x,resolution.y);p.y=-p.y;
 float2 drift=float2(sin(t*.011),sin(t*.008))*.006*flow;
 // Diameter 64% of the shorter edge leaves room for off-limb structures.
 float2 q=(p-drift)/.32;
 float r=length(q),edge=max(fwidth(r),.0001);
 if(r>1.48)return float4(0,0,0,1);
 float disc=1-smoothstep(1-edge,1+edge,r);
 float audio=movement>0?reaction*saturate(energy):0;
 float active=turbulence*(1+audio*.10);
 float2 warp=surfaceIslands(q,t,active)*saturate((1-r)*8);
 float2 coord=.5+(q+warp)*float2(1,-1)*(1605.0/4096.0);
 float source=observation.Sample(linearClamp,coord).r;
 float broad=observation.SampleLevel(linearClamp,coord,4).r;
 float detailSource=max(0,broad+(source-broad)*detail);
 float intensity=(.028+pow(detailSource,1.05)*3.5)*disc;
 // Observed off-limb emission remains image aligned. Density scales it and
 // the procedural envelope separately from the disk and activity controls.
 float outer=max(r-1,0);
 float angle=atan2(q.y,q.x);
 float rays=.68+.20*sin(angle*23+.35*sin(angle*7)+t*.014)
                 +.12*sin(angle*41-t*.02);
 float corona=exp(-outer/(.018+.013*density))*.045*rays;
 float observed=pow(max(source,0),.8)*2.3;
 // Restrain the photographic diffuse envelope so distinct prominences read
 // against black space rather than disappearing into an even luminous ring.
 intensity+=density*(1-disc)*(observed*.10+corona)*
            (1-smoothstep(1.32,1.46,r));
 // Local evolving emission cells and bright-region pulses remain visible with
 // Camera drift disabled. Broad observed features stay image aligned.
 float cells=noise(float3(q*28,t*.14))*.65+noise(float3(q*63+9,t*.23))*.35;
 intensity*=1+active*(cells-.5)*.30;
 int quality=int(spare0.x);
 float3 loops=prominenceEmission(q,t,active,quality);
 intensity*=1+audio*.045;
 if(palette>.5&&palette<1.5)loops*=float3(1,1.25,1.2);
 else if(palette>1.5&&palette<2.5)loops*=float3(1,.72,.70);
 else if(palette>2.5)loops=dot(loops,float3(.3,.59,.11))*float3(.82,.85,.9);
 float3 color=sunColor(max(intensity,0))+loops*active*(1-smoothstep(1.40,1.47,r))*(1+audio*.045);
 return float4(tone(color),1);
}
