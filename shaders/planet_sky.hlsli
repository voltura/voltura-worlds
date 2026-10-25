// Shared NASA Milky Way RGB plus Hipparcos/Tycho foreground in linear alpha.
// Alpha is emission data, not transparency. No time/audio-dependent twinkle.
float3 planetSky(float4 sky,float galaxyExposure){
 // The display toe makes even a single BC7 alpha code visible. Suppress
 // that compression floor smoothly, keeping distinct stars without blocks.
 float stars=sky.a*smoothstep(.02,.08,sky.a);
 return sky.rgb*galaxyExposure+stars*.18;
}
float2 planetSkyUV(float2 p){
 // A perspective direction avoids stretching the 2:1 panorama's stars into
 // vertical streaks. p.y increases down the screen, as does the texture's v.
 float3 ray=normalize(float3(p*.94280904158,1));
 return float2(.48+atan2(ray.x,ray.z)/6.28318530718,.52+asin(clamp(ray.y,-1,1))/3.14159265359);
}
