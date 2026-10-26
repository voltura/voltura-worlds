#pragma once
#include "model.h"
#include <dxgi1_6.h>
#include <algorithm>
#include <cmath>
namespace worlds {
inline bool sameOutputSurface(const Monitor& a,const Monitor& b){return a.handle==b.handle&&a.id==b.id&&a.adapter.HighPart==b.adapter.HighPart&&a.adapter.LowPart==b.adapter.LowPart&&EqualRect(&a.rect,&b.rect);}
struct OutputConfig {
 bool hdr=false;
 float white=203,peak=400;
 bool whiteFallback=false,peakFallback=false;
 std::wstring reason=L"Windows HDR is off";
 DXGI_FORMAT format()const{return hdr?DXGI_FORMAT_R16G16B16A16_FLOAT:DXGI_FORMAT_R8G8B8A8_UNORM;}
 DXGI_COLOR_SPACE_TYPE colorSpace()const{return hdr?DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;}
 bool operator==(const OutputConfig&)const=default;
};
inline bool usableLuminance(float n){return std::isfinite(n)&&n>=1&&n<=10000;}
inline OutputConfig resolveOutput(OutputPreference preference,bool video,bool active,float white,float peak){
 OutputConfig c;c.hdr=preference==OutputPreference::Auto&&!video&&active;
 c.reason=video?L"Video uses SDR":preference==OutputPreference::Sdr?L"SDR selected":active?L"":L"Windows HDR is off";
 c.whiteFallback=!usableLuminance(white);c.peakFallback=!usableLuminance(peak);
 c.peak=c.peakFallback?400:peak;c.white=std::min(c.whiteFallback?203:white,c.peak);return c;
}
inline bool activeHdrSignal(DXGI_COLOR_SPACE_TYPE signal){return signal==DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;}
inline float sdrWhiteNits(unsigned level){float n=float(level)*.08f;return usableLuminance(n)?n:0;}
struct OutputTarget {bool activeHdr=false;float white=0,peak=0;};
inline OutputConfig resolveTargets(OutputPreference preference,bool video,const std::vector<OutputTarget>& targets){
 bool active=!targets.empty(),whiteFallback=false,peakFallback=false;float white=10000,peak=10000;
 for(auto& t:targets){active&=t.activeHdr;whiteFallback|=!usableLuminance(t.white);peakFallback|=!usableLuminance(t.peak);white=std::min(white,usableLuminance(t.white)?t.white:203.f);peak=std::min(peak,usableLuminance(t.peak)?t.peak:400.f);}
 auto c=resolveOutput(preference,video,active,targets.empty()?0:white,targets.empty()?0:peak);c.whiteFallback|=whiteFallback;c.peakFallback|=peakFallback;if(targets.empty()&&preference==OutputPreference::Auto&&!video)c.reason=L"HDR display information unavailable";return c;
}
template<class Apply,class Removed> OutputConfig applyOutputWithFallback(OutputConfig next,Apply&& apply,Removed&& removed){
 try{apply(next);}catch(...){if(!next.hdr||removed())throw;next.hdr=false;next.reason=L"HDR presentation unavailable";apply(next);}return next;
}
OutputConfig discoverOutput(const Monitor&,OutputPreference,bool video);
}
