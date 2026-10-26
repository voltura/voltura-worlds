#include "desktop.h"
#include <dwmapi.h>
namespace worlds {
std::vector<Monitor> enumerateMonitors(){
 struct Enumeration {std::vector<Monitor> monitors;std::exception_ptr failure;};Enumeration state;
 auto enumerated=EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR h,HDC,LPRECT,LPARAM p)->BOOL{
  auto& state=*reinterpret_cast<Enumeration*>(p);
  try{
  MONITORINFOEXW info{};info.cbSize=sizeof(info);if(!GetMonitorInfoW(h,&info))throw std::runtime_error("Cannot read display information");
  DISPLAY_DEVICEW device{};device.cb=sizeof(device);std::wstring id=info.szDevice,name=info.szDevice;
  if(EnumDisplayDevicesW(info.szDevice,0,&device,EDD_GET_DEVICE_INTERFACE_NAME)){if(device.DeviceID[0])id=device.DeviceID;name=device.DeviceString;}
  Monitor monitor{h,info.rcMonitor,id,name};
  for(const auto& path:activeDisplayPaths()){
   DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};source.header={DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME,sizeof(source),path.sourceInfo.adapterId,path.sourceInfo.id};
   if(DisplayConfigGetDeviceInfo(&source.header)==ERROR_SUCCESS&&!wcscmp(source.viewGdiDeviceName,info.szDevice)){
    monitor.adapter=path.sourceInfo.adapterId;auto rate=path.targetInfo.refreshRate;
    if(rate.Numerator&&rate.Denominator)monitor.refreshHz=double(rate.Numerator)/rate.Denominator;
    break;
   }
  }
  state.monitors.push_back(std::move(monitor));return TRUE;
  }catch(...){state.failure=std::current_exception();return FALSE;}
 },reinterpret_cast<LPARAM>(&state));
 // Do not unwind a C++ exception through a Windows enumeration callback or
 // publish a partial monitor list after a failed enumeration.
 if(state.failure)std::rethrow_exception(state.failure);
 if(!enumerated)throw std::runtime_error("Cannot enumerate displays");
 return std::move(state.monitors);
}
HWND findWallpaperParent(){
 HWND progman=FindWindowW(L"Progman",nullptr);if(!progman)return nullptr;
 DWORD_PTR result{};SendMessageTimeoutW(progman,0x052c,0xD,0,SMTO_ABORTIFHUNG,1000,&result);SendMessageTimeoutW(progman,0x052c,0xD,1,SMTO_ABORTIFHUNG,1000,&result);
 struct Search {HWND worker{};};Search found;
 EnumWindows([](HWND top,LPARAM param)->BOOL{
  if(FindWindowExW(top,nullptr,L"SHELLDLL_DefView",nullptr)){
   HWND worker=FindWindowExW(nullptr,top,L"WorkerW",nullptr);
   if(worker&&!FindWindowExW(worker,nullptr,L"SHELLDLL_DefView",nullptr)){reinterpret_cast<Search*>(param)->worker=worker;return FALSE;}
  }return TRUE;
 },reinterpret_cast<LPARAM>(&found));
 // Newer Explorer builds may keep the background WorkerW below Progman.
 if(!found.worker){HWND child=nullptr;while((child=FindWindowExW(progman,child,L"WorkerW",nullptr))){if(!FindWindowExW(child,nullptr,L"SHELLDLL_DefView",nullptr)){found.worker=child;break;}}}
 return found.worker;
}
bool attachWallpaper(HWND hwnd,const Monitor& m,HWND parent,bool visible){
 wchar_t parentClass[32]{};GetClassNameW(parent,parentClass,32);
 if(!IsWindow(parent)||wcscmp(parentClass,L"WorkerW")||FindWindowExW(parent,nullptr,L"SHELLDLL_DefView",nullptr))return false;
 SetLastError(0);auto old=SetParent(hwnd,parent);if(!old&&GetLastError())return false;
 auto setStyle=[&](int index,LONG_PTR value){SetLastError(0);return SetWindowLongPtrW(hwnd,index,value)!=0||GetLastError()==0;};
 if(!setStyle(GWL_STYLE,WS_CHILD|(visible?WS_VISIBLE:0)|WS_CLIPSIBLINGS|WS_CLIPCHILDREN)||!setStyle(GWL_EXSTYLE,WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE))return false;
 POINT p{m.rect.left,m.rect.top};SetLastError(0);if(!MapWindowPoints(HWND_DESKTOP,parent,&p,1)&&GetLastError())return false;
 return SetWindowPos(hwnd,HWND_BOTTOM,p.x,p.y,m.rect.right-m.rect.left,m.rect.bottom-m.rect.top,SWP_NOACTIVATE|SWP_FRAMECHANGED|(visible?SWP_SHOWWINDOW:SWP_HIDEWINDOW))!=FALSE;
}
bool wallpaperAttached(HWND hwnd){
 if(!IsWindow(hwnd))return false;
 HWND parent=GetParent(hwnd);wchar_t name[32]{};
 return IsWindow(parent)&&GetClassNameW(parent,name,32)&&!wcscmp(name,L"WorkerW")&&!FindWindowExW(parent,nullptr,L"SHELLDLL_DefView",nullptr);
}
bool monitorCovered(const Monitor& m,DWORD own){
 struct Coverage{HRGN remaining;DWORD own;};Coverage ctx{CreateRectRgn(m.rect.left,m.rect.top,m.rect.right,m.rect.bottom),own};
 if(!ctx.remaining)return false;
 EnumWindows([](HWND hwnd,LPARAM param)->BOOL{
  auto& c=*reinterpret_cast<Coverage*>(param);DWORD pid{};GetWindowThreadProcessId(hwnd,&pid);
  if(pid==c.own||!IsWindowVisible(hwnd)||IsIconic(hwnd))return TRUE;
  auto style=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);if(style&(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOREDIRECTIONBITMAP))return TRUE;
  DWORD cloaked{};if(SUCCEEDED(DwmGetWindowAttribute(hwnd,DWMWA_CLOAKED,&cloaked,sizeof(cloaked)))&&cloaked)return TRUE;
  wchar_t name[128]{};GetClassNameW(hwnd,name,128);if(!wcscmp(name,L"Progman")||!wcscmp(name,L"WorkerW")||!wcscmp(name,L"Shell_TrayWnd")||!wcscmp(name,L"Shell_SecondaryTrayWnd"))return TRUE;
  RECT r{};if(FAILED(DwmGetWindowAttribute(hwnd,DWMWA_EXTENDED_FRAME_BOUNDS,&r,sizeof(r))))GetWindowRect(hwnd,&r);
  HRGN covered=CreateRectRgn(r.left,r.top,r.right,r.bottom);if(!covered)return TRUE;
  // An explicitly shaped window can have holes that leave the wallpaper visible.
  // Its region uses window coordinates, while coverage uses screen coordinates.
  HRGN shape=CreateRectRgn(0,0,0,0);if(!shape){DeleteObject(covered);return TRUE;}
  {RECT bounds{};if(GetWindowRgn(hwnd,shape)!=ERROR&&GetWindowRect(hwnd,&bounds)){
   if(OffsetRgn(shape,bounds.left,bounds.top)==ERROR||CombineRgn(covered,covered,shape,RGN_AND)==ERROR){DeleteObject(shape);DeleteObject(covered);return TRUE;}
  }DeleteObject(shape);}
  int result=CombineRgn(c.remaining,c.remaining,covered,RGN_DIFF);DeleteObject(covered);return result!=NULLREGION;
 },reinterpret_cast<LPARAM>(&ctx));
 RECT remaining{};bool covered=GetRgnBox(ctx.remaining,&remaining)==NULLREGION;DeleteObject(ctx.remaining);return covered;
}
}
