#include "model.h"
#include "random_cycle.h"
#include <set>
#include "package.h"
#include "dds.h"
#include <iostream>
#include <fstream>
#include <limits>
#include <functional>
using namespace worlds;
static void require(bool v,const char* s){if(!v)throw std::runtime_error(s);}
static void rejects(const std::function<void()>& fn){bool threw=false;try{fn();}catch(...){threw=true;}require(threw,"Expected rejection");}
int main(){try{
 // Full compressed mip tails occupy at least one block, including 2x2/1x1.
 std::vector<uint8_t> dds(148+3*16);auto put=[&](unsigned i,uint32_t v){for(unsigned b=0;b<4;b++)dds[i*4+b]=uint8_t(v>>(b*8));};
 put(0,0x20534444);put(1,124);put(3,4);put(4,4);put(6,1);put(7,3);put(19,32);put(20,4);put(21,0x30315844);put(32,DXGI_FORMAT_BC7_UNORM_SRGB);put(33,3);put(35,1);
 auto parsed=parseDds(dds);require(parsed.mips.size()==3&&parsed.mips.back().offset==180&&parsed.mips.back().rowBytes==16,"Compressed mip tail layout");
 auto goodDds=dds;dds.pop_back();rejects([&]{parseDds(dds);});dds=goodDds;dds.push_back(0);rejects([&]{parseDds(dds);});dds=goodDds;
 for(auto change:std::vector<std::pair<unsigned,uint32_t>>{{4,0},{4,8193},{6,2},{7,32},{7,1},{28,0x200},{32,DXGI_FORMAT_R8G8B8A8_UNORM},{33,4},{34,4},{35,2}}){put(change.first,change.second);rejects([&]{parseDds(dds);});dds=goodDds;}
 put(32,DXGI_FORMAT_BC7_UNORM);require(parseDds(dds).format==DXGI_FORMAT_BC7_UNORM,"Linear BC7 terrain map");
 put(32,DXGI_FORMAT_BC5_UNORM);require(parseDds(dds).format==DXGI_FORMAT_BC5_UNORM&&parseDds(dds).mips.back().rowBytes==16,"Linear two-channel Earth material map");
 put(32,DXGI_FORMAT_BC4_UNORM);dds.resize(148+3*8);require(parseDds(dds).mips.back().rowBytes==8,"BC4 mip tail layout");
 PauseState p;require(!shouldPause(p,true),"Active desktop paused");p.battery=true;require(shouldPause(p,true),"Battery policy ignored");require(!shouldPause(p,false),"Battery override ignored");p.locked=true;require(shouldPause(p,false),"Lock policy ignored");
 require(boundedDelta(100)==.05f,"Resume timestep unbounded");require(boundedDelta(std::numeric_limits<float>::quiet_NaN())==0,"Nonfinite timestep");
 require(parseScene("plastic_metal")==Scene::PlasticMetal&&sceneName(Scene::PlasticMetal)=="plastic_metal","Plastic metal identity");
 require(parseScene("mercury")==Scene::Mercury&&!isProceduralWorld(Scene::Mercury),"Distinct planet identity");
 require(parseScene("sun")==Scene::Sun&&!isProceduralWorld(Scene::Sun),"Sun identity");
 require(std::find(randomScenes.begin(),randomScenes.end(),Scene::Sun)!=randomScenes.end(),"Sun is eligible for Random");
 require(parseScene("pluto")==Scene::Pluto&&!isProceduralWorld(Scene::Pluto),"Pluto planet identity");
 require(std::find(randomScenes.begin(),randomScenes.end(),Scene::Pluto)!=randomScenes.end(),"Pluto is eligible for Random");
 Parameters v;require(valid(v),"Default parameters invalid");v.brightness=std::numeric_limits<float>::infinity();require(!valid(v),"Infinite parameter accepted");rejects([]{parseScene("../evil");});
 auto dir=std::filesystem::temp_directory_path()/(L"VolturaWorlds-tests-"+std::to_wstring(GetCurrentProcessId()));std::filesystem::create_directories(dir);
 {
  Assignment random;random.random=true;random.randomInterval=30;RandomCycle cycle;
  require(cycle.advance(random,0,true),"Random starts immediately");
  require(!cycle.advance(random,30000,false),"Paused time does not consume interval");
  require(!cycle.advance(random,29999,true),"Random does not switch early");
  require(cycle.advance(random,1,true),"Random switches at exact interval");
  cycle.reset();std::set<Scene> visited;Scene previous=random.scene;
  for(size_t i=0;i<randomScenes.size()*3;i++){
   require(cycle.advance(random,i?30000:0,true),"Random advances on schedule");
   require(random.scene!=previous&&random.scene!=Scene::Off&&random.scene!=Scene::Video,"Random excludes Off, video and consecutive repeats");previous=random.scene;
   visited.insert(random.scene);if(i%randomScenes.size()==randomScenes.size()-1){require(visited.size()==randomScenes.size(),"Complete shuffle has no repeats");visited.clear();}
  }
  require(visited.empty()&&std::find(randomScenes.begin(),randomScenes.end(),Scene::Uranus)!=randomScenes.end(),"Uranus is eligible for Random");
  require(std::find(randomScenes.begin(),randomScenes.end(),Scene::Neptune)!=randomScenes.end(),"Neptune is eligible for Random");
  Settings shuffle;shuffle.displays.emplace_back(L"random-monitor",random);saveSettings(dir,shuffle);
  auto loadedRandom=assignmentFor(loadSettings(dir),L"random-monitor");require(loadedRandom.random&&loadedRandom.randomInterval==30,"Random settings persist");
  random.random=false;require(!cycle.advance(random,999999,true),"Manual mode stops cycle");
  shuffle.displays[0].second.randomInterval=0;rejects([&]{saveSettings(dir,shuffle);});
 }
 require(parseScene("off")==Scene::Off&&sceneName(Scene::Off)=="off","Off scene identity failed");
 {Settings off;Assignment a;a.scene=Scene::Off;a.video=L"retained.mp4";off.displays.emplace_back(L"off-monitor",a);saveSettings(dir,off);auto restored=assignmentFor(loadSettings(dir),L"off-monitor");require(restored.scene==Scene::Off&&restored.video==a.video,"Off settings roundtrip preserves video");}
 require(parseScene("bloom")==Scene::Bloom&&sceneName(Scene::Bloom)=="bloom","Bloom scene identity failed");
 for(auto scene:{Scene::Silk,Scene::Eclipse,Scene::Lattice,Scene::Drift,Scene::Contour,Scene::PlasticMetal,Scene::Ripples,Scene::Mars,Scene::Earth,Scene::Moon,Scene::Jupiter,Scene::Saturn,Scene::Mercury,Scene::Venus,Scene::Neptune,Scene::Uranus,Scene::Pluto,Scene::Sun}){
  require(parseScene(sceneName(scene))==scene&&(isProceduralWorld(scene)||scene==Scene::Mars||scene==Scene::Earth||scene==Scene::Moon||scene==Scene::Jupiter||scene==Scene::Saturn||scene==Scene::Mercury||scene==Scene::Neptune||scene==Scene::Venus||scene==Scene::Uranus||scene==Scene::Pluto||scene==Scene::Sun),"Procedural World scene identity failed");
  Settings settings;Assignment world;world.scene=scene;world.params.palette=2;world.params.strength=.7f;world.reaction=Reaction::Gentle;settings.displays.emplace_back(L"world-monitor",world);saveSettings(dir,settings);
  auto restoredWorld=assignmentFor(loadSettings(dir),L"world-monitor");require(restoredWorld.scene==scene&&restoredWorld.params.palette==2&&restoredWorld.params.strength==.7f&&restoredWorld.reaction==Reaction::Gentle,"Procedural World settings roundtrip failed");
 }
 {Settings bloom;Assignment folded;folded.scene=Scene::Bloom;bloom.displays.emplace_back(L"bloom-monitor",folded);saveSettings(dir,bloom);require(assignmentFor(loadSettings(dir),L"bloom-monitor").scene==Scene::Bloom,"Bloom settings roundtrip failed");}
 require(parseScene("smoke")==Scene::Smoke&&sceneName(Scene::Smoke)=="smoke","Smoke scene identity failed");
 Settings s;Assignment a;a.scene=Scene::Smoke;a.params.flow=.8f;s.displays.emplace_back(L"monitor-1",a);saveSettings(dir,s);auto restored=loadSettings(dir);require(restored.displays.size()==1&&assignmentFor(restored,L"monitor-1").scene==Scene::Smoke,"Settings roundtrip failed");require(assignmentFor(restored,L"new-monitor").reaction==Reaction::Off,"New monitor audio enabled");
 {std::ofstream out(dir/L"settings.json");out<<"{\"version\":999}";}rejects([&]{loadSettings(dir);});
 WorldPackage package;package.title="Test";package.assignment=a;package.preview={137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,2,0,0,0,2,8,6,0,0,0,114,182,13,36,0,0,0,21,73,68,65,84,120,156,99,100,104,248,255,159,129,129,129,129,9,68,128,48,0,37,25,2,130,143,80,141,6,0,0,0,0,73,69,78,68,174,66,96,130};
 {auto folded=package;folded.assignment.scene=Scene::Bloom;auto bloomFile=dir/L"bloom.world";writePackage(bloomFile,folded);require(readPackage(bloomFile).assignment.scene==Scene::Bloom,"Bloom package roundtrip failed");}
 for(auto scene:{Scene::Silk,Scene::Eclipse,Scene::Lattice,Scene::Drift,Scene::Contour,Scene::PlasticMetal,Scene::Ripples,Scene::Mars,Scene::Earth,Scene::Moon,Scene::Jupiter,Scene::Saturn,Scene::Mercury,Scene::Venus,Scene::Neptune,Scene::Uranus,Scene::Pluto,Scene::Sun}){auto world=package;world.assignment.scene=scene;if(scene==Scene::Sun){world.assignment.params={.8f,.6f,1.4f,.2f,1.7f,.3f,.9f,3};world.assignment.reaction=Reaction::Expressive;}auto worldFile=dir/(wide(sceneName(scene))+L".world");writePackage(worldFile,world);require(readPackage(worldFile).assignment.scene==scene,"Procedural World package roundtrip failed");if(scene==Scene::Sun){auto roundtrip=readPackage(worldFile).assignment;auto& p=roundtrip.params;require(p.brightness==.8f&&p.movement==.6f&&p.detail==1.4f&&p.flow==.2f&&p.turbulence==1.7f&&p.density==.3f&&p.strength==.9f&&p.palette==3&&roundtrip.reaction==Reaction::Expressive,"Sun package retains every setting");Settings solar;solar.displays.emplace_back(L"solar-monitor",roundtrip);saveSettings(dir,solar);auto saved=assignmentFor(loadSettings(dir),L"solar-monitor");auto& v=saved.params;require(saved.scene==Scene::Sun&&v.brightness==p.brightness&&v.movement==p.movement&&v.detail==p.detail&&v.flow==p.flow&&v.turbulence==p.turbulence&&v.density==p.density&&v.strength==p.strength&&v.palette==p.palette&&saved.reaction==roundtrip.reaction,"Sun settings retain every control");}}
 auto file=dir/L"test.world";writePackage(file,package);auto loaded=readPackage(file);require(loaded.title=="Test"&&loaded.assignment.scene==Scene::Smoke,"Package roundtrip failed");
 {std::fstream out(file,std::ios::binary|std::ios::in|std::ios::out);out.seekp(50);out.put('!');}rejects([&]{readPackage(file);});
 writePackage(file,package);
 {std::ifstream in(file,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(in)),{});in.close();size_t pos=0;while((pos=bytes.find("preview.png",pos))!=std::string::npos){bytes.replace(pos,11,"../evil.exe");pos+=11;}std::ofstream out(file,std::ios::binary|std::ios::trunc);out.write(bytes.data(),bytes.size());}rejects([&]{readPackage(file);});
 auto preview=package.preview;package.preview.resize(24);rejects([&]{writePackage(file,package);});package.preview=preview;package.preview[50]^=1;rejects([&]{writePackage(file,package);});package.preview=preview;package.preview.push_back(0);rejects([&]{writePackage(file,package);});package.preview=preview;
 auto installed=installPackage(dir/L"library",package);require(installed.scene==Scene::Smoke,"Staged preset import failed");for(auto& entry:std::filesystem::directory_iterator(dir/L"library")){require(entry.path().extension()!=L".pending","Pending import leaked");require(readPackage(entry.path()/L"source.world").title=="Test","Offline package missing");}
 package.assignment.params.strength=3;rejects([&]{writePackage(file,package);});
 std::filesystem::remove_all(dir);std::cout<<"PASS: pause policy, bounded timestep, strict parameters, settings roundtrip, package roundtrip, tamper and traversal rejection, full PNG validation, staged offline import\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
