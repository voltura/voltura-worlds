#pragma once
#include <array>
#include <cmath>
#include <algorithm>
namespace worlds {
inline constexpr std::array<double,7> videoSpeeds{.25,.5,.75,1,1.25,1.5,2};
struct VideoSettings {
 double positionX=.5,positionY=.5,speed=1;
 bool operator==(const VideoSettings&)const=default;
};
inline bool validVideoSettings(const VideoSettings& v){return std::isfinite(v.positionX)&&std::isfinite(v.positionY)&&v.positionX>=0&&v.positionX<=1&&v.positionY>=0&&v.positionY<=1&&std::isfinite(v.speed)&&std::any_of(videoSpeeds.begin(),videoSpeeds.end(),[&](double r){return std::abs(r-v.speed)<1e-6;});}
inline double canonicalVideoSpeed(double speed){for(auto r:videoSpeeds)if(std::abs(r-speed)<1e-6)return r;return 1;}
struct VideoUV {double x,y;bool visible;};
inline VideoUV videoSourceUV(double u,double v,double displayAspect,double sourceAspect,bool fit,const VideoSettings& settings){
 auto sx=displayAspect/sourceAspect,sy=sourceAspect/displayAspect;
 if(fit){sx=std::max(1.,sx);sy=std::max(1.,sy);u=(u-.5)*sx+.5;v=(v-.5)*sy+.5;return {u,v,u>=0&&u<=1&&v>=0&&v<=1};}
 sx=std::min(1.,sx);sy=std::min(1.,sy);
 return {std::clamp((1-sx)*settings.positionX+u*sx,0.,1.),std::clamp((1-sy)*settings.positionY+v*sy,0.,1.),true};
}
}
