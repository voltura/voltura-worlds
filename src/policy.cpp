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
 switch(s) {case Scene::TropicalParadise:return "tropical_paradise";case Scene::ForestWater:return "forest_water";case Scene::Sun:return "sun";case Scene::Pluto:return "pluto";case Scene::Neptune:return "neptune";case Scene::Uranus:return "uranus";case Scene::Venus:return "venus";case Scene::Mercury:return "mercury";case Scene::Off:return "off";case Scene::Aurora:return "aurora";case Scene::Orbit:return "orbit";case Scene::Smoke:return "smoke";case Scene::Video:return "video";case Scene::Bloom:return "bloom";case Scene::Silk:return "silk";case Scene::Eclipse:return "eclipse";case Scene::Lattice:return "lattice";case Scene::Drift:return "drift";case Scene::Contour:return "contour";case Scene::PlasticMetal:return "plastic_metal";case Scene::Ripples:return "ripples";case Scene::Mars:return "mars";case Scene::Earth:return "earth";case Scene::Moon:return "moon";case Scene::Jupiter:return "jupiter";case Scene::Saturn:return "saturn";}
 throw std::runtime_error("Unknown scene");
}
Scene parseScene(const std::string& s) {
 if(s=="tropical_paradise")return Scene::TropicalParadise;if(s=="forest_water")return Scene::ForestWater;if(s=="sun")return Scene::Sun;if(s=="pluto")return Scene::Pluto;if(s=="uranus")return Scene::Uranus;if(s=="neptune")return Scene::Neptune;if(s=="venus")return Scene::Venus;if(s=="mercury")return Scene::Mercury;if(s=="saturn")return Scene::Saturn;if(s=="jupiter")return Scene::Jupiter;if(s=="moon")return Scene::Moon;if(s=="earth")return Scene::Earth;if(s=="mars")return Scene::Mars;if(s=="off")return Scene::Off;if(s=="bloom")return Scene::Bloom;if(s=="aurora")return Scene::Aurora;if(s=="orbit")return Scene::Orbit;if(s=="smoke")return Scene::Smoke;if(s=="video")return Scene::Video;
 for(auto scene:{Scene::Silk,Scene::Eclipse,Scene::Lattice,Scene::Drift,Scene::Contour,Scene::PlasticMetal,Scene::Ripples})if(s==sceneName(scene))return scene;
 throw std::runtime_error("Unsupported World scene");
}
bool isProceduralWorld(Scene scene){return (scene>=Scene::Silk&&scene<=Scene::Ripples)||scene==Scene::ForestWater||scene==Scene::TropicalParadise;}
}
