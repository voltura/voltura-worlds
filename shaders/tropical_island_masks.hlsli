// Masks registered to the original Tropical Island artwork, in image UV space.
// A geometric shoreline avoids colour-keying white surf or shallow sand away.
float islandShore(float x) { return .548 + .54*x + .16*x*x; }
float islandPalms(float2 p) {
 float left=(1.-smoothstep(.28,.36,p.x))*(1.-smoothstep(.40,.51,p.y));
 float right=smoothstep(.65,.76,p.x)*(1.-smoothstep(.32,.43,p.y));
 // Anchor each crown and exclude the two foreground trunks from the breeze.
 float leftRoot=smoothstep(.025,.15,length((p-float2(.155,.015))*float2(1.,.7)));
 float rightRoot=smoothstep(.025,.15,length((p-float2(.916,.018))*float2(1.,.7)));
 float leftTrunk=smoothstep(.012,.040,abs(p.x-(.155-.35*p.y)));
 float rightTrunk=smoothstep(.017,.043,abs(p.x-(.916+.29*p.y)));
 return saturate(left*leftRoot*leftTrunk+right*rightRoot*rightTrunk);
}

// Dry beach only: moving shadow transmission must not affect rocks, wet sand,
// foam or the sea. The soft boundary stays well inside the photographed beach.
float islandDrySand(float2 p) {
 float inland=smoothstep(.025,.070,p.y-islandShore(p.x));
 float beach=smoothstep(.60,.68,p.y)*(1.-smoothstep(.62,.73,p.x));
 return inland*beach;
}
