#include "common.hlsli"
#include "orbit_terrain.hlsli"
float3 foregroundRock(float3 n,float key,float3 stretch){return n*(.62+.5*noise(n*3.1+key)+.13*noise(n*9.7+key))*stretch;}
struct Vertex {float4 position:SV_POSITION;float3 world:TEXCOORD0;float3 normal:TEXCOORD1;float2 uv:TEXCOORD2;};
Vertex VS(uint id:SV_VertexID,uint instance:SV_InstanceID){
 bool ringPlane=(pass>.5&&pass<1.5)||(pass>2.5&&pass<3.5)||(pass>4.5&&pass<5.5);
 float ringPhase=time*.007*movement;
 uint corner=id%6,quad=id/6;uint2 offsets[6]={uint2(0,0),uint2(1,0),uint2(0,1),uint2(0,1),uint2(1,0),uint2(1,1)};
 uint columns=pass>6.5?128:pass>5.5?48:pass>4.5?8:pass>3.5?96:pass>2.5?24:256,rows=pass>6.5?64:pass>5.5?24:pass>4.5?4:pass>3.5?48:pass>2.5?12:128;
 float2 uv=(float2(quad%columns,quad/columns)+offsets[corner])/float2(columns,rows);
 float3 p,n;
 if(pass<.5||pass>1.5){
  float theta=uv.x*6.283185,phi=uv.y*3.141593;n=float3(sin(phi)*cos(theta),cos(phi),sin(phi)*sin(theta));p=n*(pass>6.5?1.339:pass>1.5?1.365:1.33);
 }else{
  float angle=uv.x*6.283185-ringPhase,r=lerp(1.48,2.35,uv.y);p=float3(cos(angle)*r,0,sin(angle)*r);n=float3(0,1,0);
  float tilt=.07;float y=p.y*cos(tilt)-p.z*sin(tilt);p.z=p.y*sin(tilt)+p.z*cos(tilt);p.y=y;n=float3(0,cos(tilt),sin(tilt));
 }
 if((pass>2.5&&pass<3.5)||(pass>4.5&&pass<6.5)){
  float key=float(instance)+(pass>4.5?407:3);
  float angle=hash(float3(key,4,7))*6.283185-ringPhase;
  float radius=lerp(1.50,2.45,hash(float3(key,2,9)));
  float size=lerp(.003,.027,pow(hash(float3(key,8,1)),3));
  if(pass>4.5)size=lerp(.0007,.0045,pow(hash(float3(key,8,1)),2));
  float shape=.75+.45*noise(n*3+key);p=n*size*shape;
  // Resolved boulders occupy the loose debris above/below the thin dust sheet.
  // Keeping every center on the sheet made its alpha look like a straight cut
  // through each rock. The size term clears even the largest displaced vertex.
  float scatter=hash(float3(key,12,4));
  float side=hash(float3(key,15,6))<.25?-1:1;
  float height=pass>4.5?(scatter-.5)*.045:side*(size*1.25+.004+scatter*.028);
  p+=float3(cos(angle)*radius,height,sin(angle)*radius);
  float y=p.y*cos(.07)-p.z*sin(.07);p.z=p.y*sin(.07)+p.z*cos(.07);p.y=y;
 }
 float rot=.25+(ringPlane?0:time*.007*movement);float x=p.x*cos(rot)+p.z*sin(rot);p.z=-p.x*sin(rot)+p.z*cos(rot);p.x=x;
 float nx=n.x*cos(rot)+n.z*sin(rot);n.z=-n.x*sin(rot)+n.z*cos(rot);n.x=nx;
 float roll=.28;float px=p.x;p.x=px*cos(roll)-p.y*sin(roll);p.y=px*sin(roll)+p.y*cos(roll);float nx2=n.x;n.x=nx2*cos(roll)-n.y*sin(roll);n.y=nx2*sin(roll)+n.y*cos(roll);
 p.x+=.72;p.y+=.02;
 [branch]if(pass>3.5&&pass<4.5){
  float3 centers[3]={float3(-.35,-.28,1.9),float3(-.5,1.15,-.4),float3(1.9,.48,.7)};
  float sizes[3]={.08,.085,.075};
  float drift=time*.002*movement;
  float3 center=centers[instance];center.x+=sin(drift+instance)*.035;center.y+=(cos(drift+instance)-cos(float(instance)))*.025;
  p=center+n*sizes[instance]*(1+moonTerrain(n).w+craters(n*9).w*.014);
 }
 if(pass>5.5&&pass<6.5){
  float key=float(instance)+913;
  float3 center=float3(.65+hash(float3(key,3,8))*.7,-.45-hash(float3(key,7,4))*.46,1.4+hash(float3(key,9,2))*.75);
  float size=lerp(.018,.13,pow(hash(float3(key,2,6)),2));
  float3 stretch=float3(.8+hash(float3(key,1,3))*.6,.65+hash(float3(key,4,1))*.65,1);
  float3 tangent=normalize(cross(abs(n.y)<.95?float3(0,1,0):float3(1,0,0),n));
  float3 bitangent=cross(n,tangent);
  float3 surface=foregroundRock(n,key,stretch);
  float3 du=foregroundRock(normalize(n+tangent*.002),key,stretch)-surface;
  float3 dv=foregroundRock(normalize(n+bitangent*.002),key,stretch)-surface;
  p=center+surface*size;n=normalize(cross(du,dv));
 }
 float z=3.45-p.z;
 Vertex v;v.position=float4(p.x*2.0/(resolution.x/resolution.y),p.y*2.0,z*1.001-.1001,z);v.world=p;v.normal=n;v.uv=uv;return v;
}



