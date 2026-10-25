#include "smoke_detail_cache.h"
#include <iostream>
#include <stdexcept>
using namespace worlds;
static void require(bool b,const char* what){if(!b)throw std::runtime_error(what);}
int main(){try{
 constexpr uint64_t budget=96ull*1024*1024;
 auto layout=SmokeAtlasLayout::make(8,budget);
 require(layout.fieldBytes<=budget&&layout.textureEdge<=2048,"Atlas exceeds limits");
 require(layout.capacity==4096&&layout.textureEdge==160,"Border-inclusive atlas packing");
 SmokeDetailCache cache(128,2);
 auto first=cache.acquire(3).value(),second=cache.acquire(8).value();
 require(first.slot!=second.slot,"Aliased residents");
 require(cache.acquire(3)==first,"Existing page moved");
 require(!cache.acquire(12),"Full atlas must fall back");
 cache.release(3,10);require(!cache.find(3),"Retired page still published");
 cache.collect(9);require(!cache.acquire(12),"Reused an in-flight GPU page");
 cache.collect(10);auto replacement=cache.acquire(12).value();
 require(replacement.slot==first.slot&&replacement.generation!=first.generation,"Stale handle not invalidated");
 require(!cache.current(12,first)&&cache.current(8,second),"Stale lookup or unrelated page movement");
 // Repeated churn must preserve the pinned page, bound capacity and wait on
 // fences, including when the same virtual page returns before retirement.
 for(uint64_t frame=11;frame<10011;++frame){
  cache.release(12,frame);require(!cache.acquire(12),"Premature same-page reuse");
  cache.collect(frame);require(cache.acquire(12).has_value(),"Retirement leaked capacity");
  require(cache.current(8,second),"Pinned detail moved during churn");
 }
 bool rejected=false;try{cache.collect(1);}catch(const std::invalid_argument&){rejected=true;}
 require(rejected,"Backward fence accepted");
 rejected=false;try{SmokeAtlasLayout::make(8,23999);}catch(const std::invalid_argument&){rejected=true;}
 require(rejected,"Insufficient bordered-tile budget accepted");
 SmokeDetailCache full(8192,layout.capacity);
 std::vector<bool> seen(layout.capacity,false);
 for(uint32_t p=0;p<layout.capacity;++p){auto h=full.acquire(p).value();
  require(h.slot<seen.size()&&!seen[h.slot],"Full atlas assigned duplicate slots");seen[h.slot]=true;}
 require(!full.acquire(8000),"Full-budget atlas exceeded capacity");
 for(uint32_t p=0;p<layout.capacity;++p)full.release(p,5);
 full.collect(5);full.collect(5); // repeated polling must not duplicate free slots
 std::fill(seen.begin(),seen.end(),false);
 for(uint32_t p=0;p<layout.capacity;++p){auto h=full.acquire(p+4096).value();
  require(!seen[h.slot],"Repeated collection duplicated a free slot");seen[h.slot]=true;}
 require(!full.acquire(0),"Recycled atlas grew past capacity");
 std::cout<<"PASS: bounded atlas layout, stable residency, coarse fallback, fence-safe recycling, stale handles, 10000 churn cycles\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
