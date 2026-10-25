float smokeEmitter(float3 uv,int e,float seconds,float step,float flowStrength,float audio){
 float phase=e*2.1+seconds*.13;
 float3 center=float3(.31+e*.22+.035*sin(phase),.18+.045*sin(phase*.7),.5+.08*cos(phase));
 float radius=.035+.008*sin(seconds*.1+e);
 return exp(-dot(uv-center,uv-center)/(radius*radius))*step*(1.4+audio*.7)*flowStrength*(.45+.55*pow(.5+.5*sin(seconds*.85+e*2.1),2));
}
