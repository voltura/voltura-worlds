#include "common.hlsli"
#include "venus_common.hlsli"

float venusCloud(float3 n){
 float t=venusClock();
 // Cartesian sampling stays continuous across the longitude seam and poles.
 // Latitude-dependent wind shears broad cloud decks into sweeping chevrons.
 float shear=(1-n.y*n.y)*.65+sin(n.y*5)*.14;
 // Positive inverse rotation moves clouds westward, opposite Earth.
 float3 p=venusRY(n,t*(2*venusPi/900)+shear);
 float drift=t*.00022*turbulence;
 float3 warp=float3(fbm(p*2.4+float3(7,0,drift)),fbm(p*2.4+float3(0,9,drift)),fbm(p*2.4+float3(drift,0,13)))-.5;
 float3 w=p+warp*.40;
 float broad=fbm(w*float3(3.8,7.5,3.8)+float3(0,0,drift));
 float ribbons=sin(w.y*27+5*w.x*w.z+fbm(w*7)*3);
 float fine=fbm(w*24+float3(drift,0,0))-.5;
 float polar=smoothstep(.65,.98,abs(n.y));
 return saturate(.48+(broad-.48)*1.35+ribbons*.055+fine*.24*detail-polar*.08);
}
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET{
 float2 q=venusScreen(uv);float r=length(q),edge=max(fwidth(r),.00001);
 // A narrow scattering envelope corresponds to tens of km above a 6052 km
 // planet. Density adjusts haze, never reveals terrain through the cloud deck.
 float hazeWidth=.004+.006*density;
 float outer=1+hazeWidth*3;
 float coverage=saturate((outer-r)/edge+.5);
 clip(coverage-.00001);
 float3 n=normalize(float3(q,sqrt(max(1-dot(q,q),.000001))));
 float mu=dot(n,venusLight());
 float shell=exp(-max(r-1,0)/hazeWidth);
 float day=smoothstep(-.12,.30,mu);
 float3 haze=float3(.83,.67,.39)*shell*day*(.11+.07*density);
 float disc=saturate((1-r)/edge+.5);
 float cloud=venusCloud(n);
 // Visible-light clouds have subdued contrast, unlike UV false-color maps.
 float3 albedo=lerp(float3(.66,.52,.32),float3(.96,.88,.68),.40+cloud*.60);
 float incidence=max(mu,0);
 float diffuse=incidence*(.65+.35/max(incidence+max(n.z,.02),.08));
 float limb=pow(1-saturate(n.z),3);
 float3 color=albedo*diffuse*1.25;
 color=lerp(color,albedo*(.24+.25*incidence),limb*.55*day);
 // The hot surface is hidden: no lava emission or artificial night glow.
 float audio=movement>0?reaction*saturate(energy)*.025:0;
 color=(color+haze*.35)*(1+audio);
 float alpha=lerp(saturate(shell)*day*.65,1,disc)*coverage;
 float3 premult=lerp(haze*.65,color,disc);
 return float4(tone(venusGrade(premult/max(alpha,.00001))),alpha);
}
