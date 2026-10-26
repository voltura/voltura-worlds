#include "common.hlsli"
#include "neptune_common.hlsli"

float4 neptuneCloud(float3 n){
 float t=neptuneClock();
 // A fixed spherical storm center makes both the oval and its surrounding
 // swirl continuous across longitude seams. The storm rotates with the globe.
 float3 center=normalize(float3(-.32,-.30,1));
 float3 east=normalize(cross(float3(0,1,0),center));
 float3 north=cross(center,east);
 float2 local=float2(dot(n,east)/.24,dot(n,north)/.115);
 float radius=length(local);
 float front=smoothstep(.70,.90,dot(n,center));
 float storm=exp(-dot(local,local)*1.5)*front;
 float spin=exp(-dot(local,local)*.38)*front*(1.3+t*.016*turbulence);
 // Rodrigues rotation, localized around the storm instead of UV distortion.
 float s,c;sincos(spin,s,c);
 float3 swirl=n*c+cross(center,n)*s+center*dot(center,n)*(1-c);
 // Latitude-dependent jets overtake the globe at different rates.
 float wind=(-.0018+.0032*n.y*n.y+.0007*sin(n.y*9))*t*turbulence;
 float3 p=neptuneRY(swirl,wind);
 float warp=fbm(p*5)-.5;
 float bands=sin(p.y*32+warp*1.4)*.5+.5;
 float broad=fbm(p*float3(3,16,3));
 float ribbons=fbm(p*float3(3,38,3)+float3(4,0,9));
 float fine=noise(p*float3(34,150,34));
 float clouds=smoothstep(.62,.77,ribbons+warp*.10)*smoothstep(.08,.22,abs(n.y))*(.65+.35*detail);
 clouds=saturate(clouds*(.92+.16*fine*detail));
 // Companion white clouds wrap the outer edge of the dark vortex.
 float rim=exp(-pow((radius-1.4)*3,2))*front*(.3+.7*smoothstep(-.5,.7,local.y));
 return float4(bands,broad,saturate(clouds+rim*.22),storm);
}
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET{
 float2 q=neptuneScreen(uv);
 float r=length(q/float2(1,neptunePolar));
 float edge=max(fwidth(r),.00001);
 float hazeWidth=.0025+.0035*density;
 float coverage=saturate((1+hazeWidth*4-r)/edge+.5);clip(coverage-.00001);
 float z=sqrt(max(1-r*r,.000001));
 float3 n=normalize(float3(q.x,q.y/(neptunePolar*neptunePolar),z));
 float mu=dot(n,neptuneLight());
 float day=smoothstep(-.06,.25,mu);
 float shell=exp(-max(r-1,0)/hazeWidth);
 float disc=saturate((1-r)/edge+.5);
 float4 cloud=neptuneCloud(neptuneBody(n));
 float3 albedo=neptuneBase()*(.87+.14*cloud.x+.16*cloud.y);
 albedo=lerp(albedo,float3(.66,.79,.88),cloud.z*.85);
 albedo*=1-cloud.w*.80;
 float incidence=max(mu,0);
 float diffuse=incidence*(.65+.35/max(incidence+max(n.z,.02),.08));
 float limb=pow(1-saturate(n.z),3);
 float3 color=albedo*diffuse*1.7;
 float3 haze=float3(.07,.32,.64)*shell*day*(.09+.04*density);
 color+=float3(.04,.18,.32)*limb*day*.12*density;
 float audio=movement>0?reaction*saturate(energy)*.045:0;
 color=(color+haze*.18)*(1+audio);
 float alpha=lerp(shell*day*.6,1,disc)*coverage;
 float3 premult=lerp(haze*.6,color,disc);
 return float4(tone(premult/max(alpha,.00001)),alpha);
}
