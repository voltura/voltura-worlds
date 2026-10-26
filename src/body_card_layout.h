#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace worlds {
struct BodyCardPlacement { float x,y,width,height,scale; };
// Insets are physical pixels reserved by desktop app bars: left, top, right,
// bottom. Preview windows and native captures use their complete client area.
inline BodyCardPlacement bodyCardPlacement(unsigned width,unsigned height,float logicalHeight,const std::array<unsigned,4>& insets={}){
 const float left=float(std::min(insets[0],width-1)),top=float(std::min(insets[1],height-1));
 const float right=std::min(float(insets[2]),width-left-1),bottom=std::min(float(insets[3]),height-top-1);
 const float usableWidth=width-left-right,usableHeight=height-top-bottom;
 float scale=std::clamp(height/1080.f,.75f,4.f);
 const float margin=std::min(24.f*scale,std::min(usableWidth,usableHeight)*.04f);
 scale=std::min({scale,(usableWidth-2*margin)/360.f,(usableHeight-2*margin)/logicalHeight});
 const float w=std::min(std::ceil(360*scale),usableWidth),h=std::min(std::ceil(logicalHeight*scale),usableHeight);
 return {std::max(left,width-right-margin-w),std::max(top,height-bottom-margin-h),w,h,scale};
}
}
