#pragma once
#include "model.h"
#include <cmath>
#include <algorithm>

namespace worlds {
// Deliberately cinematic units: neither radii nor separations are to scale.
struct JourneyBody { Scene scene; float x,y,z,radius; };
inline constexpr std::array journeyBodies{
 JourneyBody{Scene::Sun,0,0,0,2.3f},
 JourneyBody{Scene::Mercury,15,1,3,.65f},
 JourneyBody{Scene::Venus,29,-2,-2,1.f},
 JourneyBody{Scene::Earth,44,1,1,1.f},
 JourneyBody{Scene::Moon,48,2,2,.42f},
 JourneyBody{Scene::Mars,62,-1,-3,.8f},
 JourneyBody{Scene::Jupiter,80,2,2,2.f},
 JourneyBody{Scene::Saturn,102,-1,-1,1.55f},
 JourneyBody{Scene::Uranus,123,2,3,1.3f},
 JourneyBody{Scene::Neptune,142,-2,0,1.25f},
 JourneyBody{Scene::Pluto,160,1,-2,.55f}
};
struct JourneyFrame {
 size_t stop{};
 float opacity=0;
 std::array<float,3> camera{},target{};
};
inline float journeyEase(float t){t=std::clamp(t,0.f,1.f);return t*t*t*(t*(t*6-15)+10);}
inline JourneyFrame journeyFrame(double seconds,float drift,float aspect){
 constexpr double leg=37,cycle=leg*journeyBodies.size();
 double phase=std::fmod(std::max(0.,seconds),cycle);
 JourneyFrame frame;frame.stop=size_t(phase/leg);
 float local=float(phase-frame.stop*leg),u=journeyEase((local-25)/12);
 const auto& a=journeyBodies[frame.stop];const auto& b=journeyBodies[(frame.stop+1)%journeyBodies.size()];
 // Saturn needs room for its rings. Portrait framing leaves the lower card
 // clear; both camera endpoints use the same framing convention.
 auto distance=[&](const JourneyBody& body){return body.radius*(body.scene==Scene::Saturn?7.8f:4.8f)/std::min(1.f,aspect);};
 const float ax[3]={a.x,a.y,a.z},bx[3]={b.x,b.y,b.z};
 float arch=std::sin(u*3.14159265f);
 for(int i=0;i<3;i++)frame.camera[i]=frame.target[i]=ax[i]+(bx[i]-ax[i])*u;
 frame.camera[2]+=distance(a)+(distance(b)-distance(a))*u+arch*(frame.stop==10?48.f:8.f);
 frame.camera[1]+=arch*(frame.stop==10?14.f:2.f);
 const float driftRadius=a.radius+(b.radius-a.radius)*u;
 frame.camera[0]+=std::sin(float(seconds*.045))*driftRadius*.055f*drift;
 frame.camera[1]+=std::sin(float(seconds*.033))*driftRadius*.035f*drift;
 frame.opacity=local<25?journeyEase(local/2)*journeyEase((25-local)/2):0;
 return frame;
}
}
