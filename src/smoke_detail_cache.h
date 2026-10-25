#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace worlds {
// Residency foundation for the experimental fine-density atlas. Not yet wired
// to rendering. Density fallback/fades and GPU page-table publication belong
// to the renderer; release must follow its last submitted use of a page.
struct SmokeAtlasLayout {
 uint32_t tileEdge{}, tilesPerAxis{}, capacity{}, textureEdge{};
 uint64_t fieldBytes{};
 static SmokeAtlasLayout make(uint32_t interior, uint64_t budget) {
  if(interior<2 || interior>32)throw std::invalid_argument("Smoke atlas tile size");
  const uint32_t edge=interior+2; // one sample border on all six sides
  const uint64_t pageBytes=uint64_t(edge)*edge*edge*8*3; // density ping-pong + light
  uint32_t axis=0;
  while(axis<2048/edge && uint64_t(axis+1)*(axis+1)*(axis+1)<=budget/pageBytes)++axis;
  if(!axis)throw std::invalid_argument("Smoke atlas budget cannot hold one tile");
  const uint32_t count=axis*axis*axis;
  return {edge,axis,count,edge*axis,uint64_t(count)*pageBytes};
 }
};

class SmokeDetailCache {
public:
 struct Handle { uint32_t slot; uint64_t generation; bool operator==(const Handle&)const=default; };
 explicit SmokeDetailCache(uint32_t virtualPages, uint32_t capacity)
  : mappings_(checked(virtualPages),missing), slots_(checked(capacity)) {
  free_.reserve(capacity);
  for(uint32_t i=capacity;i>0;--i)free_.push_back(i-1);
 }
 std::optional<Handle> find(uint32_t page)const {
  checkPage(page);auto i=mappings_[page];
  return i==missing?std::nullopt:std::optional<Handle>{{i,slots_[i].generation}};
 }
 // A full cache returns no page: caller continues using coarse density.
 // Existing pages never move when unrelated regions enter or leave.
 std::optional<Handle> acquire(uint32_t page) {
  if(auto existing=find(page))return existing;
  if(!free_.empty()){
   const auto i=free_.back();auto& s=slots_[i];
   if(s.generation==std::numeric_limits<uint64_t>::max())
    throw std::overflow_error("Smoke page generation exhausted");
   free_.pop_back();++s.generation;s.state=State::resident;mappings_[page]=i;
   return Handle{i,s.generation};
  }
  return std::nullopt;
 }
 // A retired slot stays unavailable until all old GPU readers have finished.
 // The caller must publish removal and stop submitting new uses before this.
 void release(uint32_t page,uint64_t lastUseFence) {
  checkPage(page);auto i=mappings_[page];if(i==missing)return;
  mappings_[page]=missing;auto& s=slots_[i];s.state=State::retired;s.fence=lastUseFence;
 }
 void collect(uint64_t completedFence) {
  if(completedFence<completed_)throw std::invalid_argument("Smoke fence moved backwards");
  completed_=completedFence;
  for(uint32_t i=0;i<slots_.size();++i){
   auto& s=slots_[i];if(s.state==State::retired&&s.fence<=completed_){
    s.state=State::free;free_.push_back(i);
   }
  }
 }
 bool current(uint32_t page,Handle handle)const {
  return find(page)==std::optional<Handle>{handle};
 }
private:
 static constexpr uint32_t missing=std::numeric_limits<uint32_t>::max();
 enum class State { free,resident,retired };
 struct Slot { State state=State::free;uint64_t generation=0,fence=0; };
 static uint32_t checked(uint32_t size){
  if(!size||size>4*1024*1024)throw std::invalid_argument("Smoke page capacity");return size;
 }
 void checkPage(uint32_t page)const {
  if(page>=mappings_.size())throw std::out_of_range("Smoke virtual page");
 }
 std::vector<uint32_t> mappings_;
 std::vector<Slot> slots_;
 std::vector<uint32_t> free_;
 uint64_t completed_=0;
};
}
