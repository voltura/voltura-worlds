#include "common.hlsli"
#include "ripples_surface.hlsli"
#include "ripples_bottom.hlsli"

float waterCloud(float2 coordinate,float t) {
 float2 p=coordinate*1.7-float2(.009,.0035)*t;
 float z=8.+t*.0007;
 float cloud=noise(float3(p,z))*.57;
 cloud+=noise(float3(p*2.03+float2(4.1,1.7),z+9.2))*.29;
 cloud+=noise(float3(p*4.1209+float2(12.423,5.151),z+18.4))*.14;
 return smoothstep(.43,.61,cloud);
}

// Reflected sky is evaluated by direction, rather than painting a highlight
// onto each ring. Moving normals distort the same continuous environment.
float3 waterSky(float3 direction,float t) {
 float elevation=saturate(direction.z);
 float3 sky=lerp(float3(.64,.74,.79),float3(.16,.34,.53),sqrt(elevation));
 float2 cloudUV=direction.xy/max(.18,direction.z);
 float cloud=waterCloud(cloudUV,t);
 sky=lerp(sky,float3(.86,.88,.85),cloud*.82);
 // Broken foliage reflections from the bank supply detail for small waves
 // to distort; the bank itself stays outside the view of the water surface.
 float canopyEdge=cloudUV.x+.22*sin(cloudUV.y*2.1)-.48;
 float leaves=fbm(float3(cloudUV*12.,33.));
 float canopy=smoothstep(-.10,.10,canopyEdge+(leaves-.5)*.55);
 float3 foliage=lerp(float3(.018,.032,.023),float3(.075,.105,.052),leaves);
 sky=lerp(sky,foliage,canopy*.92);
 // A distant, irregular tree line is visible only in low-angle reflections.
 float treeLine=.14+.09*noise(float3(cloudUV.x*7.,0,14.));
 float trees=1.-smoothstep(treeLine-.025,treeLine+.025,direction.z);
 sky=lerp(sky,float3(.022,.047,.032),trees);
 return sky;
}

// Intersect the camera ray with one displaced water surface. Shared height,
// refraction and normals all include every overlapping wave.
float4 PS(float4 pixel:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET {
 float aspect=resolution.x/resolution.y;
 float2 screen=(pixel.xy-resolution*.5)/resolution.y;
 float3 camera=float3(0,-1.3,1.8);
 float3 forward=normalize(-camera);
 float3 up=normalize(cross(float3(1,0,0),forward));
 float3 ray=normalize(forward+screen.x*float3(1,0,0)*.8-screen.y*up*.8);
 float3 position=camera+ray*(-camera.z/ray.z);
 float2 p=position.xy;
 float t=time*movement;
 float audio=saturate(reaction*(.5*energy+.35*bass+.15*treble))*step(.0001,movement);
 WaterField field=waterField(p,t,audio,aspect);
 // Intersect the local tangent plane of the combined height field. The waves
 // are small enough for this bounded parallax step; High refines it once.
 // No horizontal compression or crest sharpening is applied at crossings.
 float3 flatPosition=position;
 float denominator=min(-.2,ray.z-dot(field.slope,ray.xy));
 position=flatPosition+ray*(field.height/denominator);
 if(spare0.x>1.5) {
  p=position.xy;
  field=waterField(p,t,audio,aspect);
  denominator=min(-.2,ray.z-dot(field.slope,ray.xy));
  float tangentHeight=field.height+dot(field.slope,flatPosition.xy-p);
  position=flatPosition+ray*(tangentHeight/denominator);
 }
 float height=position.z;
 float3 normal=normalize(float3(-field.slope,1));
 float3 view=-ray;
 float3 reflected=reflect(ray,normal);
 float fresnel=.0204+.9796*pow(1.-saturate(dot(normal,view)),5.);
 float3 reflection=waterSky(reflected,t)*4.;
 // Follow the refracted ray to the shelving bottom. The same physical depth
 // determines parallax, absorption and the loss of visible bottom detail.
 float3 refracted=refract(ray,normal,1./1.333);
 float2 bed=position.xy;
 float path=0.;
 [unroll]for(int step=0;step<3;step++) {
  path=(waterBedDepth(bed)+height)/max(.2,-refracted.z);
  bed=position.xy+refracted.xy*path;
 }
 float depth=waterBedDepth(bed);
 float3 bottom=waterBedColor(bed);
 float lightPath=depth/.86;
 float3 absorption=float3(1.5,.67,.5);
 float3 transmission=exp(-absorption*path);
 float3 bottomLight=bottom*exp(-absorption*lightPath);
 float3 waterTint=palette<.5?float3(.018,.095,.082):lerp(colorA(),colorB(),.35)*.14;
 // The same advected cloud field casts broad, soft illumination changes on
 // the water and bed, including areas untouched by the current drop packets.
 float cloudLight=lerp(1.,.84,waterCloud(position.xy*.22+float2(.38,-.48),t));
 float3 underwater=(bottomLight*transmission+waterTint*(1.-transmission))*cloudLight;
 float3 c=lerp(underwater,reflection,fresnel);
 // A small sun reflection breaks into glints on the disturbed water.
 float3 sun=normalize(float3(-.38,.48,1.));
 float sunReflection=pow(saturate(dot(reflected,sun)),900.);
 c+=float3(1.,.91,.73)*sunReflection*.65*cloudLight;
 return float4(tone(c),1);
}
