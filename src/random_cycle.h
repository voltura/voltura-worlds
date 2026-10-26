#pragma once
#include "model.h"
#include <algorithm>
#include <random>

namespace worlds {
inline constexpr std::array randomScenes{Scene::Aurora,Scene::Orbit,Scene::Mars,Scene::Earth,Scene::Moon,Scene::Jupiter,Scene::Saturn,Scene::Mercury,Scene::Venus,Scene::Neptune,Scene::Uranus,Scene::Pluto,Scene::Sun,Scene::Smoke,Scene::Bloom,Scene::Silk,Scene::Eclipse,Scene::Lattice,Scene::Drift,Scene::Contour,Scene::PlasticMetal,Scene::Ripples};
inline constexpr std::array randomIntervals{30,60,120,300,600,900,1800};
inline bool validRandomInterval(int seconds){return std::find(randomIntervals.begin(),randomIntervals.end(),seconds)!=randomIntervals.end();}
// Each display owns its shuffle and countdown; elapsed time is active playback time.
class RandomCycle {
 std::mt19937 engine_{std::random_device{}()};
 std::array<Scene,randomScenes.size()> bag_{};
 size_t next_=bag_.size();
 uint64_t elapsed_=0;
 bool started_=false;
 int interval_=0;
public:
 void reset(bool keepScene=false){next_=bag_.size();elapsed_=0;started_=keepScene;}
 bool advance(Assignment& a,uint64_t milliseconds,bool active){
  if(!a.random){reset();return false;}
  if(interval_!=a.randomInterval){interval_=a.randomInterval;elapsed_=0;}
  if(!active)return false;
  if(started_){elapsed_+=milliseconds;if(elapsed_<uint64_t(a.randomInterval)*1000)return false;}
  elapsed_=0;started_=true;
  if(next_==bag_.size()){
   bag_=randomScenes;std::shuffle(bag_.begin(),bag_.end(),engine_);next_=0;
   // Avoid an immediate repeat, including at the boundary between shuffles.
   if(bag_[0]==a.scene)std::swap(bag_[0],bag_[1]);
  }
  a.scene=bag_[next_++];return true;
 }
};
}
