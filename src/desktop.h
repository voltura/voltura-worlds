#pragma once
#include "model.h"
namespace worlds {
std::vector<Monitor> enumerateMonitors();
HWND findWallpaperParent();
bool attachWallpaper(HWND,const Monitor&,HWND,bool visible=true);
bool wallpaperAttached(HWND);
bool monitorCovered(const Monitor&,DWORD ownProcess);
}
