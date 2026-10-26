#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace worlds {
// Independent double-precision reference used by numerical/GPU readback tests.
inline std::array<double,3> hdrOracle(std::array<double,3> c,double white,double peak,bool orbit=false){
 for(auto& v:c)v=std::isfinite(v)?std::max(v,0.):0.;
 double k=std::max({c[0],c[1],c[2]}),head=std::max(peak/white-1.,0.),e=std::max(k-1.,0.);
 double h=head>0?head*(-std::expm1(-e*e/(head*(1+e)))):0;
 std::array<double,3> out{};
 for(int i=0;i<3;i++){double x=c[i],m=.62*(-std::expm1(-x))+.38*x*(-std::expm1(-k))/std::max(k,1e-5);
 if(orbit){double z=x*.85;m=z*(2.51*z+.03)/(z*(2.43*z+.59)+.14);}
 out[i]=std::clamp((std::clamp(m,0.,1.)+h*x/std::max(k,1e-5))*white/80.,0.,peak/80.);}
 return out;
}
}
