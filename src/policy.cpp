#include "model.h"
#include <cmath>
#include <algorithm>
namespace worlds {
bool shouldPause(const PauseState& s,bool battery) {
 return s.manual||s.locked||s.asleep||s.suspended||s.remote||s.covered||(battery&&s.battery);
}
bool valid(const Parameters& p) {
 for(float v:{p.brightness,p.movement,p.detail,p.flow,p.turbulence,p.density,p.strength})
  if(!std::isfinite(v)||v<0||v>2) return false;
 return p.palette>=0&&p.palette<=3&&p.strength<=1;
}
float boundedDelta(float t) { return std::isfinite(t)?std::clamp(t,0.f,.05f):0.f; }
Assignment assignmentFor(const Settings& s,const std::wstring& id) {
 for(const auto& [key,value]:s.displays) if(key==id) return value;
 return {};
}
std::string sceneName(Scene s) {
 switch(s) {case Scene::Off:return "off";case Scene::Aurora:return "aurora";case Scene::Orbit:return "orbit";case Scene::Smoke:return "smoke";case Scene::Video:return "video";case Scene::Bloom:return "bloom";}
 throw std::runtime_error("Unknown scene");
}
Scene parseScene(const std::string& s) {
 if(s=="off")return Scene::Off;if(s=="bloom")return Scene::Bloom;if(s=="aurora")return Scene::Aurora;if(s=="orbit")return Scene::Orbit;if(s=="smoke")return Scene::Smoke;if(s=="video")return Scene::Video;
 throw std::runtime_error("Unsupported World scene");
}
}
