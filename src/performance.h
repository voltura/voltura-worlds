#pragma once
#include "model.h"
#include <algorithm>
#include <cmath>
#include <optional>

namespace worlds {
inline double displayRefresh(double hz){return std::isfinite(hz)&&hz>=1&&hz<=1000?hz:60.;}
inline double frameCeiling(const DisplayPerformance& p,double hz){return std::min(displayRefresh(hz),p.frameLimit==FrameLimit::Fps30?30.:60.);}
inline bool canCopyFrameLimit(FrameLimit limit,double hz){return limit==FrameLimit::Auto||displayRefresh(hz)+.12>=(limit==FrameLimit::Fps30?30.:60.);}
inline Assignment importForDisplay(const Assignment& source,const Assignment& destination){auto copy=source;copy.performance=destination.performance;copy.output=destination.output;copy.showBodyStatistics=destination.showBodyStatistics;return copy;}
inline Assignment copyForDisplay(const Assignment& source,const Assignment& destination,double hz){auto copy=source;if(!canCopyFrameLimit(copy.performance.frameLimit,hz))copy.performance.frameLimit=destination.performance.frameLimit;return copy;}
inline bool adaptiveDetail(Scene scene){return scene==Scene::Smoke||scene==Scene::Silk||scene==Scene::Eclipse||scene==Scene::Drift||scene==Scene::Contour||scene==Scene::Ripples||scene==Scene::Sun||scene==Scene::ForestWater||scene==Scene::TropicalIsland;}
inline std::vector<double> cadenceLevels(const DisplayPerformance& p,double hz){
 hz=displayRefresh(hz);double top=frameCeiling(p,hz);std::vector<double> rates;
 // Fixed caps also use refresh divisors while adapting; exact cap remains the
 // initial cadence for completely manual settings.
 if(p.quality!=Quality::Auto&&p.frameLimit!=FrameLimit::Auto)return {top};
 double floor=std::min(24.,top);
 for(unsigned d=1;d<=unsigned(std::ceil(hz/floor));d++){double rate=hz/d;if(rate<=top+.001&&rate>=floor-.001)rates.push_back(rate);}
 if(rates.empty()||rates.back()>floor+.001)rates.push_back(floor);
 return rates;
}
struct FrameDeadline {
 double next=0;
 bool due(double now)const{return now>=next;}
 void submitted(double now,double fps){double step=1./fps;next=next>0&&now-next<step?next+step:now+step;}
 void reset(double now){next=now;}
};
struct PerformanceState {
 DisplayPerformance requested{};Scene scene=Scene::Off;double refresh=60;
 std::vector<double> rates;size_t cadence=0;int detail=2;
 bool active=false;double warmUntil=0,gpuSum=0,lastGpuMs=0;unsigned frames=0,misses=0;
 FrameDeadline deadline;
 std::optional<std::pair<size_t,int>> trial;
 double fps()const{return rates.empty()?60:rates[cadence];}
 bool adaptive()const{return requested.quality==Quality::Auto||requested.frameLimit==FrameLimit::Auto;}
 bool configure(DisplayPerformance p,double hz,Scene nextScene,double now){
  hz=displayRefresh(hz);if(!rates.empty()&&requested==p&&refresh==hz&&scene==nextScene)return false;
  requested=p;refresh=hz;scene=nextScene;rates=cadenceLevels(p,hz);cadence=0;detail=p.quality==Quality::Eco?0:2;trial.reset();restart(now);return true;
 }
 void restart(double now){warmUntil=now+2;deadline.reset(now);gpuSum=0;frames=misses=0;lastGpuMs=0;}
 void setActive(bool value,double now){if(active!=value){active=value;restart(now);}}
 void sample(double now,double gpuMs,bool missed){if(now<warmUntil||!std::isfinite(gpuMs)||gpuMs<=0||gpuMs>10000)return;gpuSum+=gpuMs;lastGpuMs=gpuMs;++frames;misses+=missed;}
 bool lower(double now){
  if(!adaptive())return false;
  if(scene==Scene::TropicalIsland&&requested.quality==Quality::Auto&&detail>0)--detail;
  else if(cadence+1<rates.size())++cadence;
  else if(requested.quality==Quality::Auto&&adaptiveDetail(scene)&&detail>0)--detail;
  else return false;
  trial.reset();restart(now);return true;
 }
 bool restore(double now){
  if(!adaptive())return false;auto previous=std::pair{cadence,detail};
  if(scene==Scene::TropicalIsland&&cadence>0)--cadence;
  else if(requested.quality==Quality::Auto&&detail<2)++detail;
  else if(cadence>0)--cadence;
  else return false;
  trial=previous;restart(now);return true;
 }
};
// One instance per adapter; all inputs are measurements from its own worker.
struct AdapterPerformance {
 double lastEvaluation=0;unsigned overload=0,headroom=0;
 void evaluate(double now,const std::vector<PerformanceState*>& displays){
  if(now-lastEvaluation<1)return;double interval=now-lastEvaluation;lastEvaluation=now;
  double work=0;bool missed=false,eligible=false;
  for(auto* p:displays)if(p->active){if(now<p->warmUntil){for(auto* q:displays){q->gpuSum=0;q->frames=q->misses=0;}overload=headroom=0;return;}work+=p->gpuSum;missed|=p->frames>=3&&p->misses>p->frames/5;eligible|=p->frames>0;}
  double utilization=work/(interval*1000);
  bool hot=eligible&&(utilization>.5||missed),cool=eligible&&utilization<.35&&!missed;
  overload=hot?overload+1:0;headroom=cool?headroom+1:0;
  if(hot){for(auto* p:displays)if(p->active&&p->trial){p->cadence=p->trial->first;p->detail=p->trial->second;p->trial.reset();p->restart(now);overload=headroom=0;break;}}
  if(overload>=3){
   std::vector<PerformanceState*> candidates=displays;std::stable_sort(candidates.begin(),candidates.end(),[](auto* a,auto* b){return a->gpuSum>b->gpuSum;});
   for(auto* p:candidates)if(p->active&&p->lower(now))break;
   overload=headroom=0;
  }else if(headroom>=15){
   // Beach restores motion first; other scenes restore spatial detail first.
   bool changed=false;for(auto* p:displays)if(p->active&&p->requested.quality==Quality::Auto&&p->detail<2){changed=p->restore(now);if(changed)break;}
   if(!changed)for(auto* p:displays)if(p->active&&p->restore(now))break;
   overload=headroom=0;
  }
  for(auto* p:displays){if(cool)p->trial.reset();p->gpuSum=0;p->frames=p->misses=0;}
 }
};
}
