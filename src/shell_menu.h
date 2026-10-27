#pragma once
#include "model.h"
#include <algorithm>
#include <iterator>
#include <string_view>

namespace worlds::shellmenu {
// String IDs are the IPC contract; scene enum values and array offsets are not.
struct Visualization { Scene scene; const wchar_t* name; const wchar_t* id; bool solar=false; };
inline constexpr Visualization visualizations[]={
 {Scene::Aurora,L"Aurora",L"aurora"},{Scene::Orbit,L"Orbit",L"orbit"},
 {Scene::Mars,L"Mars",L"mars",true},{Scene::Earth,L"Earth",L"earth",true},
 {Scene::Moon,L"Moon",L"moon",true},{Scene::Jupiter,L"Jupiter",L"jupiter",true},
 {Scene::Saturn,L"Saturn",L"saturn",true},{Scene::Mercury,L"Mercury",L"mercury",true},
 {Scene::Venus,L"Venus",L"venus",true},{Scene::Neptune,L"Neptune",L"neptune",true},
 {Scene::Uranus,L"Uranus",L"uranus",true},{Scene::Pluto,L"Pluto",L"pluto",true},
 {Scene::Sun,L"Sun",L"sun",true},{Scene::Smoke,L"Smoke",L"smoke"},
 {Scene::Bloom,L"Bloom",L"bloom"},{Scene::Silk,L"Silk",L"silk"},
 {Scene::Eclipse,L"Eclipse",L"eclipse"},{Scene::Lattice,L"Lattice",L"lattice"},
 {Scene::Drift,L"Drift",L"drift"},{Scene::Contour,L"Contour",L"contour"},
 {Scene::PlasticMetal,L"Plastic metal",L"plastic-metal"},{Scene::Ripples,L"Ripples",L"ripples"},
 {Scene::ForestWater,L"Forest Water",L"forest-water"},
 {Scene::TropicalParadise,L"Tropical Paradise",L"tropical-paradise"},
 {Scene::Off,L"Random",L"random"},{Scene::Off,L"Random Solar System",L"random-solar"},
 {Scene::Off,L"Off",L"off"},{Scene::Video,L"Imported video",L"video"}
};
inline constexpr const wchar_t* solarOrder[]={L"sun",L"mercury",L"venus",L"earth",L"moon",L"mars",L"jupiter",L"saturn",L"uranus",L"neptune",L"pluto"};
inline constexpr UINT invokeMessage=WM_APP+40;
inline constexpr DWORD protocolVersion=1,requestMagic=0x56574D31;
struct DisplayState { wchar_t id[256]{},device[32]{},choice[48]{}; BOOL hasVideo{}; };
struct State {
 DWORD version=protocolVersion,processId{}; ULONGLONG timestamp{};
 BOOL paused{},automaticUpdates{},updateReady{}; DWORD displayCount{};
 DisplayState displays[32]{};
};
struct Request {
 DWORD magic=requestMagic,version=protocolVersion,invoke{};
 wchar_t command[48]{},displayId[256]{};
};
struct Response { State state; DWORD accepted{}; };
struct Entry {
 std::wstring id,title; bool checked=false,enabled=true;
 std::vector<Entry> children;
 bool separator()const{return title.empty();}
};
inline const Visualization* visualization(const std::wstring& id){
 for(const auto& v:visualizations)if(id==v.id)return &v;return nullptr;
}
inline bool validCommand(const std::wstring& id){
 return visualization(id)||id==L"open"||id==L"pause"||id==L"resume"||id==L"automatic-on"||id==L"automatic-off"||id==L"restart-update"||id==L"exit";
}
inline std::vector<Entry> visualizationEntries(const DisplayState* display){
 Entry solar{L"",L"Solar System"};
 auto choice=[&](const wchar_t* id,const wchar_t* title){return Entry{id,title,display&&std::wstring(display->choice)==id,display!=nullptr};};
 solar.children.push_back(choice(L"random-solar",L"Random"));solar.children.push_back({});
 for(auto id:solarOrder){auto v=visualization(id);solar.children.push_back(choice(id,v->name));}
 solar.checked=std::any_of(solar.children.begin(),solar.children.end(),[](const Entry& e){return e.checked;});
 solar.enabled=display!=nullptr;
 std::vector<Entry> result;
 for(const auto& v:visualizations){
  if(v.solar||std::wstring(v.id)==L"random-solar")continue;
  if(v.scene==Scene::Video&&(!display||!display->hasVideo))continue;
  result.push_back(choice(v.id,v.name));
  if(v.scene==Scene::Orbit)result.push_back(solar);
 }
 return result;
}
inline std::vector<Entry> controlEntries(const State* state){
 std::vector<Entry> result{{L"open",L"Open Voltura Worlds"}};
 if(!state)return result;
 result.push_back({state->paused?L"resume":L"pause",state->paused?L"Resume":L"Pause"});
 result.push_back({state->automaticUpdates?L"automatic-off":L"automatic-on",L"Automatic updates",state->automaticUpdates!=FALSE});
 if(state->updateReady)result.push_back({L"restart-update",L"Restart to update"});
 result.push_back({L"exit",L"Exit Voltura Worlds"});return result;
}
// The tray retains its existing hierarchy; desktop groups are independent roots.
inline std::vector<Entry> entries(const State* state,const DisplayState* display){
 auto result=controlEntries(state);
 if(state)result.insert(result.begin()+1,{L"",L"Visualizations",false,display!=nullptr,visualizationEntries(display)});
 return result;
}
enum class DesktopGroup { Controls,Visualizations,SolarSystem };
inline constexpr DesktopGroup desktopGroups[]={DesktopGroup::Controls,DesktopGroup::Visualizations,DesktopGroup::SolarSystem};
inline const wchar_t* desktopTitle(DesktopGroup group){
 switch(group){
 case DesktopGroup::Visualizations:return L"Voltura Worlds Visualizations";
 case DesktopGroup::SolarSystem:return L"Voltura Worlds Solar System";
 default:return L"Voltura Worlds";
 }
}
// Each independently registered root must fit the observed 16-command budget.
// Count available video as well, so adding artwork cannot silently hide commands.
inline constexpr size_t desktopVisualCount=[] {
 size_t count=0;for(const auto& v:visualizations)if(!v.solar&&std::wstring_view(v.id)!=L"random-solar")++count;return count;
}();
static_assert(desktopVisualCount<=16,"Split the desktop visualization menu before adding more commands.");
static_assert(std::size(solarOrder)+1<=16,"Split the Solar System desktop menu before adding more commands.");
inline std::vector<Entry> desktopEntries(const State* state,const DisplayState* display,DesktopGroup group){
 if(group==DesktopGroup::Controls)return controlEntries(state);
 if(!state)return {};
 auto choice=[&](const wchar_t* id,const wchar_t* title){return Entry{id,title,display&&std::wstring(display->choice)==id,display!=nullptr};};
 std::vector<Entry> result;
 if(group==DesktopGroup::SolarSystem){
  result.push_back(choice(L"random-solar",L"Random Solar System"));result.push_back({});
  for(auto id:solarOrder)result.push_back(choice(id,visualization(id)->name));
 }else{
  result.push_back(choice(L"random",L"Random"));result.push_back({});
  for(const auto& v:visualizations){
   if(v.solar||std::wstring_view(v.id)==L"random-solar"||std::wstring_view(v.id)==L"random")continue;
   if(v.scene==Scene::Video&&(!display||!display->hasVideo))continue;
   result.push_back(choice(v.id,v.name));
  }
 }
 return result;
}
}
