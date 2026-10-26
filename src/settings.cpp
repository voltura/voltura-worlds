#include "model.h"
#include "random_cycle.h"
#include <shlobj.h>
#include <fstream>
#include "json.hpp"
namespace worlds {
std::string utf8(const std::wstring& s) {
 if(s.empty())return {};int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0,nullptr,nullptr);
 if(!n)throw std::runtime_error("Invalid text");std::string r(n,0);WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),(int)s.size(),r.data(),n,nullptr,nullptr);return r;
}
std::wstring wide(const std::string& s) {
 if(s.empty())return {};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0);
 if(!n)throw std::runtime_error("Invalid UTF-8");std::wstring r(n,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),r.data(),n);return r;
}
std::filesystem::path profilePath(){ PWSTR p{};if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&p)))throw std::runtime_error("User profile unavailable");std::filesystem::path r=p;CoTaskMemFree(p);return r/L"Voltura"/L"Worlds"; }
Settings loadSettings(const std::filesystem::path& dir) {
 Settings s;auto file=dir/L"settings.json";if(!std::filesystem::exists(file))return s;
 if(std::filesystem::file_size(file)>1024*1024)throw std::runtime_error("Settings exceed size limit");
 std::ifstream f(file);nlohmann::json j;f>>j;if(j.at("version")!=1)throw std::runtime_error("Unsupported settings version");
 int q=j.at("quality"),fps=j.at("fps");if(q<0||q>2||(fps!=30&&fps!=60))throw std::runtime_error("Invalid quality settings");
 s.quality=Quality(q);s.fps=fps;s.pauseBattery=j.at("pauseBattery");s.audioDevice=wide(j.value("audioDevice",std::string{}));
 if(j.at("displays").size()>32)throw std::runtime_error("Too many monitor assignments");
 for(auto& item:j.at("displays")){
  Assignment a;a.performance={s.quality,s.fps==30?FrameLimit::Fps30:FrameLimit::Fps60};
  if(item.contains("performance")){auto perf=item.at("performance");int quality=perf.at("quality"),limit=perf.at("frameLimit");if(quality<0||quality>2||limit<0||limit>2)throw std::runtime_error("Invalid display performance");a.performance={Quality(quality),FrameLimit(limit)};}
  auto output=item.value("output",std::string("auto"));if(output!="auto"&&output!="sdr")throw std::runtime_error("Invalid display output");a.output=output=="sdr"?OutputPreference::Sdr:OutputPreference::Auto;
  a.scene=parseScene(item.at("scene"));int r=item.at("reaction");if(r<0||r>2)throw std::runtime_error("Invalid audio reaction");a.reaction=Reaction(r);
  auto p=item.at("parameters");a.params={p.at("brightness"),p.at("movement"),p.at("detail"),p.at("flow"),p.at("turbulence"),p.at("density"),p.at("strength"),p.at("palette")};
  if(!valid(a.params))throw std::runtime_error("Invalid scene parameters");a.video=wide(item.value("video",std::string{}));a.fit=item.value("fit",false);
  if(item.contains("videoSettings")){const auto& v=item.at("videoSettings");if(!v.is_object())throw std::runtime_error("Invalid video settings");a.videoSettings={v.value("positionX",.5),v.value("positionY",.5),v.value("speed",1.)};if(!validVideoSettings(a.videoSettings))throw std::runtime_error("Invalid video settings");a.videoSettings.speed=canonicalVideoSpeed(a.videoSettings.speed);}
  a.showBodyStatistics=item.value("showBodyStatistics",false);
  auto pool=item.value("randomPool",std::string("all"));if(pool!="all"&&pool!="solarSystem")throw std::runtime_error("Invalid random pool");a.randomPool=pool=="solarSystem"?RandomPool::SolarSystem:RandomPool::All;
  a.random=item.value("random",false);a.randomInterval=item.value("randomInterval",300);
  if(!validRandomInterval(a.randomInterval))throw std::runtime_error("Invalid random interval");
  s.displays.emplace_back(wide(item.at("id")),a);
 }return s;
}
void saveSettings(const std::filesystem::path& dir,const Settings& s){
 nlohmann::json j={{"version",1},{"quality",int(s.quality)},{"fps",s.fps},{"pauseBattery",s.pauseBattery},{"audioDevice",utf8(s.audioDevice)},{"displays",nlohmann::json::array()}};
 for(auto& [id,a]:s.displays){if(a.output!=OutputPreference::Auto&&a.output!=OutputPreference::Sdr)throw std::runtime_error("Invalid display output");if(int(a.performance.quality)<0||int(a.performance.quality)>2||int(a.performance.frameLimit)<0||int(a.performance.frameLimit)>2)throw std::runtime_error("Invalid display performance");if(!validVideoSettings(a.videoSettings)||!valid(a.params)||!validRandomInterval(a.randomInterval)||(a.randomPool!=RandomPool::All&&a.randomPool!=RandomPool::SolarSystem))throw std::runtime_error("Invalid parameters");auto& p=a.params;
  j["displays"].push_back({{"id",utf8(id)},{"output",a.output==OutputPreference::Sdr?"sdr":"auto"},{"performance",{{"quality",int(a.performance.quality)},{"frameLimit",int(a.performance.frameLimit)}}},{"scene",sceneName(a.scene)},{"reaction",int(a.reaction)},{"video",utf8(a.video)},{"fit",a.fit},{"videoSettings",{{"positionX",a.videoSettings.positionX},{"positionY",a.videoSettings.positionY},{"speed",a.videoSettings.speed}}},{"parameters",{{"brightness",p.brightness},{"movement",p.movement},{"detail",p.detail},{"flow",p.flow},{"turbulence",p.turbulence},{"density",p.density},{"strength",p.strength},{"palette",p.palette}}}});
 }
 for(size_t i=0;i<s.displays.size();i++){j["displays"][i]["showBodyStatistics"]=s.displays[i].second.showBodyStatistics;j["displays"][i]["randomPool"]=s.displays[i].second.randomPool==RandomPool::SolarSystem?"solarSystem":"all";j["displays"][i]["random"]=s.displays[i].second.random;j["displays"][i]["randomInterval"]=s.displays[i].second.randomInterval;}
 std::filesystem::create_directories(dir);auto pending=dir/L"settings.pending",dest=dir/L"settings.json";
 auto bytes=j.dump(2);HANDLE h=CreateFileW(pending.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot save settings");
 DWORD written{};bool ok=WriteFile(h,bytes.data(),(DWORD)bytes.size(),&written,nullptr)&&written==bytes.size()&&FlushFileBuffers(h);CloseHandle(h);
 if(!ok||!MoveFileExW(pending.c_str(),dest.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot commit settings");
}
}
