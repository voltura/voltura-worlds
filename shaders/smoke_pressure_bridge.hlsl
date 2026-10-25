cbuffer Parameters:register(b0){uint grid,source,destination,rhs,operation,wrapBoundary,coarseGrid,coarseBase,coarseSource;};
RWStructuredBuffer<float> fields:register(u0);
Texture3D<float> divergenceField:register(t0);
RWTexture3D<float> pressureField:register(u1);
[numthreads(4,4,4)]void CS(uint3 id:SV_DispatchThreadID){
 if(any(id>=grid))return;uint i=(id.z*grid+id.y)*grid+id.x,cells=grid*grid*grid;
 if(operation==3){fields[i]=0;fields[cells+i]=0;fields[2*cells+i]=divergenceField.Load(int4(id,0));}
 else pressureField[id]=fields[source+i];
}
