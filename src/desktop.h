#pragma once
#include "model.h"
namespace worlds {
inline std::vector<DISPLAYCONFIG_PATH_INFO> activeDisplayPaths(){
 // A hot-plug can grow the topology after Windows reports buffer sizes.
 // Re-query those sizes rather than interpreting a short buffer as no output.
 for(unsigned attempt=0;attempt<4;attempt++){
  UINT pathCount=0,modeCount=0;
  if(GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS,&pathCount,&modeCount)!=ERROR_SUCCESS)return {};
  std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
  auto result=QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,&pathCount,paths.data(),&modeCount,modes.data(),nullptr);
  if(result==ERROR_INSUFFICIENT_BUFFER)continue;
  if(result!=ERROR_SUCCESS)return {};
  paths.resize(pathCount);return paths;
 }
 return {};
}
std::vector<Monitor> enumerateMonitors();
HWND findWallpaperParent();
bool attachWallpaper(HWND,const Monitor&,HWND,bool visible=true);
bool wallpaperAttached(HWND);
bool monitorCovered(const Monitor&,DWORD ownProcess);
}
