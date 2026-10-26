#include "performance.h"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <limits>
using namespace worlds;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
 auto dir=std::filesystem::temp_directory_path()/(L"VolturaWorldsPerformanceTests-"+std::to_wstring(GetCurrentProcessId()));std::filesystem::create_directories(dir);
 Settings original;original.quality=Quality::Eco;original.fps=30;original.displays={{L"old",Assignment{}}};saveSettings(dir,original);
 auto path=dir/L"settings.json";nlohmann::json legacy;{std::ifstream in(path);in>>legacy;}legacy["displays"][0].erase("performance");{std::ofstream out(path);out<<legacy;}
 auto migrated=loadSettings(dir);require(migrated.displays[0].second.performance==DisplayPerformance{Quality::Eco,FrameLimit::Fps30},"Legacy per-display migration");
 require(assignmentFor(migrated,L"new").performance==DisplayPerformance{},"New displays use Auto/Auto");
 Assignment a,b;a.performance={Quality::High,FrameLimit::Fps60};b.performance={Quality::Eco,FrameLimit::Fps30};original.displays={{L"one",a},{L"two",b}};saveSettings(dir,original);
 auto saved=loadSettings(dir);require(saved.displays[0].second.performance==a.performance&&saved.displays[1].second.performance==b.performance,"Independent performance roundtrip");
 for(double hz:{24.,30.,59.94,60.,144.}){auto copied=copyForDisplay(a,b,hz);require(copied.performance.quality==Quality::High,"Copy quality to all");require(copied.performance.frameLimit==(hz<59?FrameLimit::Fps30:FrameLimit::Fps60),"Copy FPS only where supported");}
 a.performance.frameLimit=FrameLimit::Auto;require(copyForDisplay(a,b,24).performance.frameLimit==FrameLimit::Auto,"Copy Auto universally");
 require(frameCeiling({},144)==60&&frameCeiling({},24)==24&&frameCeiling({},59.94)==59.94,"Refresh ceilings preserve fractional rates");
 require(frameCeiling({},std::numeric_limits<double>::quiet_NaN())==60,"Invalid refresh fallback");
 auto levels=cadenceLevels({},144);require(levels==std::vector<double>{48,36,28.8,24},"Cadence divisors and floor");
 require(cadenceLevels({},23.976).size()==1,"Below-floor display retains refresh");
 require(std::abs(cadenceLevels({},59.94)[1]-29.97)<.0001,"Fractional divisor");
 FrameDeadline slow,medium,fast;unsigned low=0,mid=0,high=0;for(unsigned i=0;i<10000;i++){double now=i*.001;if(slow.due(now)){++low;slow.submitted(now,24);}if(medium.due(now)){++mid;medium.submitted(now,30);}if(fast.due(now)){++high;fast.submitted(now,60);}}
 require(mid>=299&&mid<=301,"Independent 30 FPS deadline");
 require(low>=239&&low<=241&&high>=599&&high<=601,"Independent deadlines");
 // A blocked slow display never advances or suppresses the other deadline.
 fast={};high=0;for(unsigned i=0;i<10000;i++){double now=i*.001;if(fast.due(now)){++high;fast.submitted(now,60);}}require(high>=599,"Blocked presentation isolation");
 PerformanceState p;p.configure({},60,Scene::Smoke,0);p.setActive(true,0);AdapterPerformance controller;controller.lastEvaluation=0;
 auto tick=[&](double now,double work,bool misses=false){p.gpuSum=work;p.frames=60;p.misses=misses?30:0;controller.evaluate(now,{&p});};
 for(int t=1;t<=4;t++)tick(t,700);require(p.cadence==1&&p.detail==2,"Overload lowers cadence before detail after warmup");
 double t=5;while(p.cadence+1<p.rates.size()){for(int j=0;j<5;j++)tick(t++,700);}for(int j=0;j<5;j++)tick(t++,700);require(p.detail<2,"Detail is last fallback");
 int before=p.detail;for(int j=0;j<18;j++)tick(t++,100);require(p.detail>before&&p.cadence==p.rates.size()-1,"Restore detail before cadence");
 for(int j=0;j<3;j++)tick(t++,700);require(p.detail==before,"Failed restoration rolls back");
 p.setActive(false,t);auto cadence=p.cadence;for(int j=0;j<30;j++)tick(t++,0);require(p.cadence==cadence,"Paused displays do not adapt");
 p.configure({Quality::High,FrameLimit::Fps30},60,Scene::Earth,t);p.setActive(true,t);for(int j=0;j<30;j++)tick(t++,1000,true);require(p.fps()==30&&p.detail==2,"Manual quality and FPS remain fixed");
 p.configure({Quality::High,FrameLimit::Auto},60,Scene::Smoke,t);p.setActive(true,t);for(int j=0;j<25;j++)tick(t++,700);require(p.fps()==24&&p.detail==2,"Auto FPS respects fixed High detail");
 p.configure({},60,Scene::Earth,t);p.setActive(true,t);for(int j=0;j<30;j++)tick(t++,700);require(p.detail==2,"No invented quality adjustment for Earth");
 p.sample(t,std::numeric_limits<double>::quiet_NaN(),true);require(p.frames==0,"Invalid GPU timing ignored");
 p.configure({},60,Scene::Silk,t);require(p.detail==2&&p.cadence==0&&p.warmUntil==t+2,"Scene switch resets and warms controller");
 // Combined adapter load matters even when each display is below 50% alone.
 PerformanceState large,small;large.configure({},60,Scene::Smoke,0);small.configure({},60,Scene::Silk,0);large.setActive(true,0);small.setActive(true,0);AdapterPerformance shared;
 for(int second=1;second<=4;second++){large.gpuSum=400;small.gpuSum=200;large.frames=small.frames=60;shared.evaluate(second,{&large,&small});}
 require(large.cadence==1&&small.cadence==0,"Largest GPU contributor adapts on combined load");
 // A fixed display still contributes to the budget, while only Auto changes.
 large.configure({Quality::High,FrameLimit::Fps60},60,Scene::Smoke,10);small.configure({},60,Scene::Silk,10);shared={10};
 for(int second=11;second<=14;second++){large.gpuSum=450;small.gpuSum=150;large.frames=small.frames=60;shared.evaluate(second,{&large,&small});}
 require(large.cadence==0&&small.cadence==1,"Fixed display remains fixed while Auto sibling adapts");
 std::filesystem::remove_all(dir);
 std::cout<<"PASS: migration, persistence, refresh/copy policy, independent deadlines, warmup, overload, recovery, rollback, fixed modes, pauses and invalid timing\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
