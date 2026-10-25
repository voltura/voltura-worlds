#include "renderer.h"
#include "random_cycle.h"
#include "updates.h"
#include "desktop.h"
#include "audio.h"
#include "package.h"
#include "import_ui.h"
#include "ui_theme.h"
#include "json.hpp"
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <wtsapi32.h>
#include <dwmapi.h>
#include <psapi.h>
#include <gdiplus.h>
#include <thread>
#include <condition_variable>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
using namespace worlds;
namespace {
constexpr UINT trayMessage=WM_APP+1,errorMessage=WM_APP+2,openLibraryMessage=WM_APP+3,renderStatusMessage=WM_APP+4;
constexpr int monitorId=100,visualizationId=110,applyAllId=113,reactionId=120,qualityId=121,fpsId=122,paletteId=123,batteryId=124,deviceId=125,pauseId=126,retryId=127,communityId=128,importId=129,videoImportId=130,videoFitId=131,advancedId=132,resetId=133,randomIntervalId=134;
struct Visualization { Scene scene; const wchar_t* name; };
constexpr Visualization visualizations[]={{Scene::Aurora,L"Aurora"},{Scene::Orbit,L"Orbit"},{Scene::Mars,L"Mars"},{Scene::Earth,L"Earth"},{Scene::Moon,L"Moon"},{Scene::Jupiter,L"Jupiter"},{Scene::Smoke,L"Smoke"},{Scene::Bloom,L"Bloom"},{Scene::Silk,L"Silk"},{Scene::Eclipse,L"Eclipse"},{Scene::Lattice,L"Lattice"},{Scene::Drift,L"Drift"},{Scene::Contour,L"Contour"},{Scene::Mercury,L"Mercury"},{Scene::Ripples,L"Ripples"},{Scene::Off,L"Random"},{Scene::Off,L"Off"},{Scene::Video,L"Imported video"}};
constexpr int visualizationMenuBase=1000;
constexpr int randomChoice=int(std::size(visualizations))-3;
struct Surface {HWND hwnd{};Monitor monitor;bool covered=false;RandomCycle randomCycle;};
HWND controller{},library{},statusLabel{};HINSTANCE instance{};HFONT font{},headingFont{};
HICON trayIcon{};
std::vector<Surface> surfaces;std::vector<std::pair<std::wstring,std::wstring>> audioDevices;
std::filesystem::path profile,exeDir;Settings settings;PauseState pauseState;
std::mutex stateMutex;std::condition_variable saveCv;std::jthread renderThread,saveThread;bool savePending=false;
std::wstring lastError;bool previewMode=true,desktopConnectionProblem=false,renderingProblem=false,automated=false;std::atomic<bool> running{true};
UINT menuDpi=96;HFONT menuFont{};
unsigned renderGeneration=0;
UINT taskbarCreated{};AudioCapture audio;HANDLE singleInstance{};
ULONGLONG nextDesktopRetry{},nextUpdateCheck{};
wu::Updates updates;bool automaticUpdates=false,sessionEnding=false,restartForUpdate=false;
struct LayoutItem{HWND hwnd;int x,y,w,h;bool advanced,footer;int sx{},sy{},sw{},sh{};};
std::vector<LayoutItem> layoutItems;bool advancedOpen=false,addingAdvanced=false,addingFooter=false,layingOut=false,sideExpansion=false;
int scrollOffset=0;UINT uiDpi=96;unsigned promotionAttempts=0;
int px(int value){return MulDiv(value,uiDpi,96);}
void layoutLibrary(){
 if(!library||layingOut)return;layingOut=true;RECT client{};GetClientRect(library,&client);
 int contentHeight=advancedOpen?(sideExpansion?572:824):458;
 int total=px(contentHeight);int visible=client.bottom;
 scrollOffset=std::clamp(scrollOffset,0,std::max(0,total-visible));
 SCROLLINFO si{sizeof(si),SIF_RANGE|SIF_PAGE|SIF_POS,0,total-1,UINT(std::max(0,visible)),scrollOffset,0};SetScrollInfo(library,SB_VERT,&si,TRUE);
 HDWP batch=BeginDeferWindowPos(int(layoutItems.size()));
 for(auto& item:layoutItems){bool show=!item.advanced||advancedOpen;bool side=item.advanced&&sideExpansion;int x=side?item.sx:item.x,y=side?item.sy:item.y,width=side?item.sw:item.w,height=side?item.sh:item.h;
  if(GetDlgCtrlID(item.hwnd)==retryId)show=previewMode||desktopConnectionProblem||renderingProblem;
  if(item.footer&&advancedOpen&&!sideExpansion)y+=366;
  if(item.hwnd==statusLabel)y=contentHeight-72;
  UINT flags=SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW|(show?SWP_SHOWWINDOW:SWP_HIDEWINDOW);
  if(batch)batch=DeferWindowPos(batch,item.hwnd,nullptr,px(x),px(y)-scrollOffset,px(width),px(height),flags);
 }
 if(batch)EndDeferWindowPos(batch);
 // Moving controls without redraw also exposes parent background that must be erased.
 layingOut=false;RedrawWindow(library,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);

}
void updateUiFonts(){
 HFONT old=font,oldHeading=headingFont;
 font=CreateFontW(-px(16),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 headingFont=CreateFontW(-px(30),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 for(size_t i=0;i<layoutItems.size();i++){auto child=layoutItems[i].hwnd;SendMessageW(child,WM_SETFONT,(WPARAM)(i==0?headingFont:font),TRUE);wchar_t name[32]{};GetClassNameW(child,name,32);if(!wcscmp(name,L"ComboBox")){SendMessageW(child,CB_SETITEMHEIGHT,WPARAM(-1),px(26));SendMessageW(child,CB_SETITEMHEIGHT,0,px(26));}}
 if(old)DeleteObject(old);if(oldHeading)DeleteObject(oldHeading);
}
void resizeLibrary(){
 MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(library,MONITOR_DEFAULTTONEAREST),&monitor);RECT work=monitor.rcWork,current{};GetWindowRect(library,&current);
 RECT wide{0,0,px(1240),px(572)};auto style=DWORD(GetWindowLongPtrW(library,GWL_STYLE));AdjustWindowRectExForDpi(&wide,style,FALSE,WS_EX_APPWINDOW,uiDpi);
 sideExpansion=advancedOpen&&wide.right-wide.left<=work.right-work.left-16;
 RECT r{0,0,px(sideExpansion?1240:760),px(advancedOpen?(sideExpansion?572:824):458)};
 AdjustWindowRectExForDpi(&r,style,FALSE,WS_EX_APPWINDOW,uiDpi);
 int width=std::min(int(r.right-r.left),int(work.right-work.left)),height=std::min(int(r.bottom-r.top),int(work.bottom-work.top));
 int x=std::clamp(int(current.left),int(work.left),int(work.right)-width),y=std::clamp(int(current.top),int(work.top),int(work.bottom)-height);
 bool visible=IsWindowVisible(library);if(visible)SendMessageW(library,WM_SETREDRAW,FALSE,0);
 layingOut=true;SetWindowPos(library,nullptr,x,y,width,height,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW);layingOut=false;layoutLibrary();
 if(visible)SendMessageW(library,WM_SETREDRAW,TRUE,0);
 RedrawWindow(library,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_FRAME|RDW_UPDATENOW);
}

void setComboSelection(int id,int value){HWND child=GetDlgItem(library,id);if(!SendMessageW(child,CB_GETDROPPEDSTATE,0,0)&&SendMessageW(child,CB_GETCURSEL,0,0)!=value)SendMessageW(child,CB_SETCURSEL,value,0);}
void setControlText(HWND child,const std::wstring& text){
 std::wstring current(size_t(GetWindowTextLengthW(child))+1,L'\0');GetWindowTextW(child,current.data(),int(current.size()));
 if(text!=current.c_str())SetWindowTextW(child,text.c_str());
}

std::filesystem::path executable(){wchar_t path[32768];DWORD n=GetModuleFileNameW(nullptr,path,32768);return std::filesystem::path(std::wstring(path,n));}
void error(const std::exception& e){if(automated){std::ofstream out(exeDir/L"capture-error.txt");out<<e.what();return;}MessageBoxW(library?library:controller,wide(e.what()).c_str(),L"Voltura Worlds",MB_OK|MB_ICONWARNING);}
void requestSave(){savePending=true;saveCv.notify_one();}
Surface& selectedSurface(){int i=int(SendDlgItemMessageW(library,monitorId,CB_GETCURSEL,0,0));if(i<0||i>=int(surfaces.size()))i=0;return surfaces.at(i);}
Assignment& selected(){const auto& id=selectedSurface().monitor.id;for(auto& [key,a]:settings.displays)if(key==id)return a;settings.displays.emplace_back(id,Assignment{});return settings.displays.back().second;}
HWND control(const wchar_t* type,const wchar_t* text,DWORD style,int x,int y,int w,int h,int id){
 if(!wcscmp(type,L"BUTTON")&&(style&BS_TYPEMASK)!=BS_AUTOCHECKBOX)style=(style&~DWORD(BS_TYPEMASK))|BS_OWNERDRAW;
 HWND hwnd=CreateWindowExW(0,type,text,WS_CHILD|WS_VISIBLE|style,px(x),px(y),px(w),px(h),library,(HMENU)(INT_PTR)id,instance,nullptr);SendMessageW(hwnd,WM_SETFONT,(WPARAM)font,TRUE);
 SetWindowTheme(hwnd,ui::dark?L"DarkMode_Explorer":L"Explorer",nullptr);if(!wcscmp(type,TRACKBAR_CLASSW))SetWindowSubclass(hwnd,ui::sliderPaint,1,0);
 LayoutItem item{hwnd,x,y,w,h,addingAdvanced,addingFooter,x,y,w,h};
 if(addingAdvanced){
  if(y==327){item.sx=770;item.sy=24;item.sw=420;}
  else if(id>=200&&id<207){int i=id-200;item.sx=770+(i%2)*220;item.sy=95+(i/2)*56;item.sw=200;item.sh=24;}
  else if(y>=367&&y<=499&&(y-367)%44==0){item.sx=770+(x>=380?220:0);item.sy=65+(y-367)/44*56;item.sw=200;}
  else if(id==videoFitId){item.sx=990;item.sy=263;item.sw=200;}
  else if(y==546){item.sx=770+(x>=380?220:0);item.sy=310;item.sw=200;}
  else if(id==qualityId||id==fpsId){item.sx=id==qualityId?770:990;item.sy=340;item.sw=200;}
  else if(y==591){item.sx=770;item.sy=382;item.sw=420;}
  else if(id==deviceId){item.sx=770;item.sy=412;item.sw=420;}
  else if(id==batteryId){item.sx=770;item.sy=460;item.sw=420;}
  else if(y==668){item.sx=770;item.sy=504;item.sw=420;item.sh=50;}
 }
 layoutItems.push_back(item);return hwnd;
}
void label(const wchar_t* text,int x,int y,int w=200){control(L"STATIC",text,0,x,y,w,24,0);}
void combo(int id,int x,int y,int w,std::initializer_list<const wchar_t*> values){HWND c=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_TABSTOP|WS_VSCROLL,x,y,w,240,id);SendMessageW(c,CB_SETITEMHEIGHT,WPARAM(-1),px(26));SendMessageW(c,CB_SETITEMHEIGHT,0,px(26));for(auto v:values)SendMessageW(c,CB_ADDSTRING,0,(LPARAM)v);SetWindowSubclass(c,ui::comboBorder,1,0);ui::themeCombo(c);}
// Called on the UI thread with stateMutex held. Hidden surfaces reveal Explorer's
// existing wallpaper; no Windows wallpaper settings are changed.
void syncSurfaceVisibility(){
 for(auto& s:surfaces){bool visible=assignmentFor(settings,s.monitor.id).scene!=Scene::Off;
  if(IsWindow(s.hwnd)&&bool(IsWindowVisible(s.hwnd))!=visible)ShowWindow(s.hwnd,visible?SW_SHOWNOACTIVATE:SW_HIDE);
 }
}
void syncControls(){
 std::lock_guard lock(stateMutex);syncSurfaceVisibility();if(!library)return;
 auto retry=GetDlgItem(library,retryId);
 setControlText(retry,previewMode?L"Use on desktop":L"Retry now");
 bool showRetry=previewMode||desktopConnectionProblem||renderingProblem;
 if(bool(GetWindowLongPtrW(retry,GWL_STYLE)&WS_VISIBLE)!=showRetry){
  if(!showRetry&&GetFocus()==retry)SetFocus(GetDlgItem(library,pauseId));
  ShowWindow(retry,showRetry?SW_SHOWNA:SW_HIDE);
 }
 if(surfaces.empty()){setControlText(statusLabel,lastError.empty()?L"No display is available.":lastError);return;}
 auto& a=selected();
 // Planet Worlds reuse saved parameters with scene-specific, meaningful labels.
 const wchar_t* regularNames[]={L"Brightness",L"Movement",L"Detail",L"Flow",L"Turbulence",L"Density",L"Audio strength"};
 const wchar_t* marsNames[]={L"Brightness",L"Movement",L"Terrain relief",L"Camera drift",L"Dust variation",L"Atmosphere",L"Audio strength"};
 const wchar_t* jupiterNames[]={L"Brightness",L"Movement",L"Cloud detail",L"Camera drift",L"Cloud drift",L"Atmosphere",L"Audio strength"};
 const wchar_t* moonNames[]={L"Brightness",L"Movement",L"Terrain relief",L"Camera drift",L"Sun angle",L"Earth brightness",L"Audio strength"};
 const wchar_t* earthNames[]={L"Brightness",L"Movement",L"Cloud detail",L"Camera drift",L"Cloud drift",L"Atmosphere",L"Audio strength"};
 for(int i=0;i<7;i++)setControlText(GetDlgItem(library,210+i),(a.scene==Scene::Jupiter?jupiterNames:a.scene==Scene::Moon?moonNames:a.scene==Scene::Earth?earthNames:a.scene==Scene::Mars?marsNames:regularNames)[i]);
 const wchar_t* regularPalettes[]={L"Teal / violet / gold",L"Emerald / blue",L"Ember / gold",L"Silver"};
 const wchar_t* marsPalettes[]={L"Natural Mars",L"Warm dust",L"Cool dusk",L"Survey neutral"};
 const wchar_t* jupiterPalettes[]={L"Natural Jupiter",L"Warm belts",L"Cool haze",L"Neutral survey"};
 const wchar_t* moonPalettes[]={L"Natural Moon",L"Warm sunlight",L"Cool silver",L"Neutral survey"};
 const wchar_t* earthPalettes[]={L"Natural Earth",L"Warm daylight",L"Cool daylight",L"Neutral survey"};
 auto palettes=a.scene==Scene::Jupiter?jupiterPalettes:a.scene==Scene::Moon?moonPalettes:a.scene==Scene::Earth?earthPalettes:a.scene==Scene::Mars?marsPalettes:regularPalettes;
 wchar_t firstPalette[80]{};SendDlgItemMessageW(library,paletteId,CB_GETLBTEXT,0,reinterpret_cast<LPARAM>(firstPalette));
 if(wcscmp(firstPalette,palettes[0])!=0){
  SendDlgItemMessageW(library,paletteId,CB_RESETCONTENT,0,0);
  for(int i=0;i<4;i++)SendDlgItemMessageW(library,paletteId,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(palettes[i]));
 }
 // Native setters can erase and synchronously repaint even an unchanged value.
 setControlText(GetDlgItem(library,pauseId),pauseState.manual?L"Resume":L"Pause");
 setComboSelection(reactionId,int(a.reaction));setComboSelection(qualityId,int(settings.quality));setComboSelection(fpsId,settings.fps==30?0:1);setComboSelection(paletteId,a.params.palette);
 int check=settings.pauseBattery?BST_CHECKED:BST_UNCHECKED;if(SendDlgItemMessageW(library,batteryId,BM_GETCHECK,0,0)!=check)SendDlgItemMessageW(library,batteryId,BM_SETCHECK,check,0);
 float values[]={a.params.brightness,a.params.movement,a.params.detail,a.params.flow,a.params.turbulence,a.params.density,a.params.strength};for(int i=0;i<7;i++)if(SendDlgItemMessageW(library,200+i,TBM_GETPOS,0,0)!=int(values[i]*100))SendDlgItemMessageW(library,200+i,TBM_SETPOS,TRUE,int(values[i]*100));
 int dev=0;for(size_t i=0;i<audioDevices.size();i++)if(audioDevices[i].first==settings.audioDevice)dev=int(i)+1;setComboSelection(deviceId,dev);
 setComboSelection(videoFitId,a.fit?1:0);EnableWindow(GetDlgItem(library,videoFitId),a.scene==Scene::Video);
 EnableWindow(GetDlgItem(library,reactionId),a.scene!=Scene::Video&&a.scene!=Scene::Off);EnableWindow(GetDlgItem(library,paletteId),a.scene!=Scene::Video&&a.scene!=Scene::Off);for(int id=200;id<207;id++)EnableWindow(GetDlgItem(library,id),a.scene!=Scene::Video&&a.scene!=Scene::Off);
 auto scene=wide(sceneName(a.scene));if(a.random)scene=L"Random · "+scene;std::wstring text=previewMode?L"Preview mode · ":L"Desktop · ";text+=scene;text+=L"   |   Audio ";text+=a.reaction==Reaction::Off?L"off":a.reaction==Reaction::Gentle?L"gentle":L"expressive";if(pauseState.manual)text+=L"   |   Paused";if(!lastError.empty())text+=L"   |   "+lastError;
 setControlText(statusLabel,text);
 int videoChoice=int(std::size(visualizations))-1;
 auto count=SendDlgItemMessageW(library,visualizationId,CB_GETCOUNT,0,0);
 if(!a.video.empty()&&count==videoChoice)SendDlgItemMessageW(library,visualizationId,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(visualizations[videoChoice].name));
 else if(a.video.empty()&&count>videoChoice)SendDlgItemMessageW(library,visualizationId,CB_DELETESTRING,videoChoice,0);
 int choice=-1;for(size_t i=0;i<std::size(visualizations);i++)if(int(i)!=randomChoice&&visualizations[i].scene==a.scene)choice=int(i);
 if(a.random)choice=randomChoice;
 auto interval=std::find(randomIntervals.begin(),randomIntervals.end(),a.randomInterval);
 setComboSelection(randomIntervalId,int(interval-randomIntervals.begin()));EnableWindow(GetDlgItem(library,randomIntervalId),a.random);
 setComboSelection(visualizationId,choice);
} 
// Both entry points share the catalog and keep imported video available to return to.
bool selectVisualization(int choice){
 {std::lock_guard lock(stateMutex);
  if(surfaces.empty()||choice<0||choice>=int(std::size(visualizations)))return false;
  auto& a=selected();auto scene=visualizations[choice].scene;
  if(scene==Scene::Video&&a.video.empty())return false;
  a.random=choice==randomChoice;
  if(!a.random)a.scene=scene;
  auto& cycle=selectedSurface().randomCycle;cycle.reset();if(a.random)cycle.advance(a,0,true);
  requestSave();
 }
 syncControls();return true;
}
HMENU visualizationMenu(){
 HMENU menu=CreatePopupMenu();std::lock_guard lock(stateMutex);
 for(size_t i=0;i<std::size(visualizations);i++){
  bool available=!surfaces.empty();
  auto scene=visualizations[i].scene;
  if(available&&scene==Scene::Video&&selected().video.empty())continue;
  UINT flags=MF_OWNERDRAW|(available?0:MF_GRAYED);
  if(available&&(selected().random?int(i)==randomChoice:(int(i)!=randomChoice&&selected().scene==scene)))flags|=MF_CHECKED;
  AppendMenuW(menu,flags,visualizationMenuBase+i,visualizations[i].name);
 }
 MENUINFO info{sizeof(info),MIM_BACKGROUND};info.hbrBack=ui::surfaceBrush;SetMenuInfo(menu,&info);return menu;

}
void advanceRandomVisualizations(uint64_t elapsed){
 bool changed=false;
 {std::lock_guard lock(stateMutex);
  for(auto& surface:surfaces){
   auto entry=std::find_if(settings.displays.begin(),settings.displays.end(),[&](const auto& item){return item.first==surface.monitor.id;});
   if(entry==settings.displays.end())continue;
   auto pause=pauseState;pause.covered=surface.covered;
   changed|=surface.randomCycle.advance(entry->second,elapsed,IsWindow(surface.hwnd)&&!shouldPause(pause,settings.pauseBattery));
  }
 }
 if(changed)syncControls();
}
void startRendering(){
 renderingProblem=false;auto generation=++renderGeneration;
 renderThread=std::jthread([generation](std::stop_token stop){CoInitializeEx(nullptr,COINIT_MULTITHREADED);try{
  std::vector<std::unique_ptr<Renderer>> renderers(surfaces.size());std::vector<float> clocks(surfaces.size(),0);std::vector<unsigned> overBudget(surfaces.size(),0);std::vector<bool> paused(surfaces.size(),true);
  // Start Moon on its near side and Jupiter on its Great Red Spot, regardless
  // of how long a preceding World ran on this display.
  std::vector<float> tourClocks(surfaces.size(),0);std::vector<Scene> tourScenes(surfaces.size(),Scene::Off);
  std::vector<ULONGLONG> retryAt(surfaces.size(),0);std::vector<Scene> failedScene(surfaces.size());std::vector<std::wstring> failedVideo(surfaces.size());bool reportedFailure=false;
  auto previous=std::chrono::steady_clock::now();
  while(!stop.stop_requested()){
   auto tick=std::chrono::steady_clock::now();Settings config;PauseState pause;std::vector<bool> covered;
   {std::lock_guard lock(stateMutex);config=settings;pause=pauseState;for(auto& s:surfaces)covered.push_back(s.covered);}
   bool active=false,listen=false;for(size_t i=0;i<surfaces.size();i++){pause.covered=covered[i];auto a=assignmentFor(config,surfaces[i].monitor.id);bool nowPaused=a.scene==Scene::Off||!IsWindow(surfaces[i].hwnd)||shouldPause(pause,config.pauseBattery);paused[i]=nowPaused;active|=!nowPaused;listen|=!nowPaused&&a.scene!=Scene::Video&&a.reaction!=Reaction::Off;}
   audio.configure(listen,config.audioDevice);auto features=audio.features();float delta=boundedDelta(std::chrono::duration<float>(tick-previous).count());previous=tick;
   for(size_t i=0;i<surfaces.size();i++){
    auto a=assignmentFor(config,surfaces[i].monitor.id);
    if(a.scene!=tourScenes[i])tourClocks[i]=0;
    tourScenes[i]=a.scene;
    if(retryAt[i]&&(a.scene!=failedScene[i]||a.video!=failedVideo[i]))retryAt[i]=0;
    if(retryAt[i]&&GetTickCount64()<retryAt[i])continue;
    try{
    if(paused[i]){if(renderers[i])renderers[i]->setPaused(true);continue;}
    RECT rect{};GetClientRect(surfaces[i].hwnd,&rect);
    if(!renderers[i])renderers[i]=std::make_unique<Renderer>(surfaces[i].hwnd,std::max(1L,rect.right),std::max(1L,rect.bottom),exeDir/L"shaders");
    auto& r=*renderers[i];r.setPaused(a.scene!=Scene::Video);r.resize(std::max(1L,rect.right),std::max(1L,rect.bottom));
    unsigned desired=config.quality==Quality::Eco?64:config.quality==Quality::High?96:80;
    if(config.quality==Quality::Auto&&a.scene==Scene::Smoke){
     if(overBudget[i]<=120){if(r.gpuMs()>8)++overBudget[i];else overBudget[i]=0;}
     if(overBudget[i]>120)desired=64;
    }else overBudget[i]=0;
    r.setGrid(desired);
    FrameData data;clocks[i]+=delta;bool localClock=a.scene==Scene::Moon||a.scene==Scene::Jupiter;if(localClock)tourClocks[i]+=delta;data.time=localClock?tourClocks[i]:clocks[i];data.dt=delta;data.brightness=a.params.brightness;data.movement=a.params.movement;data.detail=a.params.detail;data.flow=a.params.flow;data.turbulence=a.params.turbulence;data.density=a.params.density;data.palette=float(a.params.palette);data.reaction=a.reaction==Reaction::Off?0:(a.reaction==Reaction::Gentle?.35f:1.f)*a.params.strength;data.bass=features.bass;data.mid=features.mid;data.treble=features.treble;data.energy=features.energy;data.impulse=features.transient;if(isProceduralWorld(a.scene))data.spare[0]=float(config.quality);r.render(a,data);
    retryAt[i]=0;
    }catch(const std::exception& e){
     renderers[i].reset();retryAt[i]=GetTickCount64()+5000;failedScene[i]=a.scene;failedVideo[i]=a.video;
     {std::lock_guard lock(stateMutex);lastError=wide(e.what());}
    }
   }
   bool failed=std::any_of(retryAt.begin(),retryAt.end(),[](ULONGLONG value){return value!=0;});
   if(failed!=reportedFailure){reportedFailure=failed;if(!failed){std::lock_guard lock(stateMutex);lastError.clear();}PostMessageW(controller,renderStatusMessage,failed,generation);}
   std::this_thread::sleep_until(tick+(active?std::chrono::milliseconds(1000/config.fps):std::chrono::milliseconds(200)));
  }
  audio.configure(false,{});
 }catch(const std::exception& e){{std::lock_guard lock(stateMutex);lastError=wide(e.what());}audio.configure(false,{});PostMessageW(controller,renderStatusMessage,TRUE,generation);}CoUninitialize();});
}
LRESULT CALLBACK wallpaperProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;if(msg==WM_NCHITTEST&&!previewMode)return HTTRANSPARENT;if(msg==WM_CLOSE){ShowWindow(hwnd,SW_HIDE);return 0;}return DefWindowProcW(hwnd,msg,w,l);}
void createSurfaces(){
 nextDesktopRetry=GetTickCount64()+5000;
 renderThread.request_stop();if(renderThread.joinable())renderThread.join();for(auto& s:surfaces)DestroyWindow(s.hwnd);surfaces.clear();auto monitors=enumerateMonitors();HWND parent=previewMode?nullptr:findWallpaperParent();
 if(library)SendDlgItemMessageW(library,monitorId,CB_RESETCONTENT,0,0);
 if(monitors.empty()||(!previewMode&&!parent)){
  desktopConnectionProblem=true;
  {std::lock_guard lock(stateMutex);lastError=L"Desktop unavailable. Retrying automatically; your ordinary wallpaper is unchanged.";}
  syncControls();return;
 }
 try{
 for(auto& m:monitors){auto hwnd=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,L"VolturaWorldSurface",L"Voltura Worlds — native preview",previewMode?WS_OVERLAPPEDWINDOW:WS_POPUP,previewMode?CW_USEDEFAULT:m.rect.left,previewMode?CW_USEDEFAULT:m.rect.top,previewMode?960:m.rect.right-m.rect.left,previewMode?540:m.rect.bottom-m.rect.top,nullptr,nullptr,instance,nullptr);if(!hwnd)throw std::runtime_error("Cannot create wallpaper surface");
  bool visible=assignmentFor(settings,m.id).scene!=Scene::Off;
  if(!previewMode&&!attachWallpaper(hwnd,m,parent,visible)){DestroyWindow(hwnd);throw std::runtime_error("Cannot attach desktop surface");}surfaces.push_back({hwnd,m,false});if(visible)ShowWindow(hwnd,previewMode?SW_SHOWNOACTIVATE:SW_SHOWNA);if(previewMode)break;
 }
 }catch(const std::exception& e){
  for(auto& s:surfaces)DestroyWindow(s.hwnd);surfaces.clear();
  desktopConnectionProblem=true;
  {std::lock_guard lock(stateMutex);lastError=wide(e.what())+L". Retrying automatically.";}
  syncControls();return;
 }
 desktopConnectionProblem=false;
 {std::lock_guard lock(stateMutex);lastError.clear();}
 startRendering();if(library){SendDlgItemMessageW(library,monitorId,CB_RESETCONTENT,0,0);for(auto& s:surfaces)SendDlgItemMessageW(library,monitorId,CB_ADDSTRING,0,(LPARAM)s.monitor.name.c_str());SendDlgItemMessageW(library,monitorId,CB_SETCURSEL,0,0);syncControls();}
}
void showLibrary();
void importWorld(const std::filesystem::path& file,bool personalVideo=false){
 showLibrary();auto displays=enumerateMonitors();if(displays.empty())throw std::runtime_error("Connect a display before applying a World.");
 int index=int(SendDlgItemMessageW(library,monitorId,CB_GETCURSEL,0,0));
 Monitor target=(!surfaces.empty()&&index>=0&&index<int(surfaces.size()))?surfaces[index].monitor:displays.front();
 showWorldImport(controller,file,profile/L"library",target.name,[id=target.id](const Assignment& imported){
  {std::lock_guard lock(stateMutex);bool found=false;for(auto& [key,value]:settings.displays)if(key==id){value=imported;found=true;break;}
   if(!found)settings.displays.emplace_back(id,imported);requestSave();}
  syncControls();
 },personalVideo);
}
LRESULT CALLBACK libraryProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
 try{
 if(msg==WM_CLOSE){ShowWindow(hwnd,SW_HIDE);return 0;}
 if(msg==WM_SIZE){layoutLibrary();return 0;}
 if(msg==WM_VSCROLL||msg==WM_MOUSEWHEEL){
  SCROLLINFO si{sizeof(si),SIF_ALL};GetScrollInfo(hwnd,SB_VERT,&si);
  if(msg==WM_MOUSEWHEEL)scrollOffset-=GET_WHEEL_DELTA_WPARAM(w)*px(48)/WHEEL_DELTA;
  else switch(LOWORD(w)){case SB_LINEUP:scrollOffset-=px(32);break;case SB_LINEDOWN:scrollOffset+=px(32);break;case SB_PAGEUP:scrollOffset-=si.nPage;break;case SB_PAGEDOWN:scrollOffset+=si.nPage;break;case SB_THUMBTRACK:scrollOffset=si.nTrackPos;break;}
  layoutLibrary();return 0;
 }
 if(msg==WM_DPICHANGED){uiDpi=HIWORD(w);updateUiFonts();auto r=reinterpret_cast<RECT*>(l);SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);layoutLibrary();return 0;}
 if(msg==WM_SETTINGCHANGE||msg==WM_THEMECHANGED){ui::refresh(hwnd);for(auto& item:layoutItems){wchar_t name[32]{};GetClassNameW(item.hwnd,name,32);if(!wcscmp(name,L"ComboBox"))ui::themeCombo(item.hwnd);else SetWindowTheme(item.hwnd,ui::dark?L"DarkMode_Explorer":L"Explorer",nullptr);}return 0;}
 if(msg==WM_ERASEBKGND){RECT r;GetClientRect(hwnd,&r);FillRect((HDC)w,&r,ui::backgroundBrush);return 1;}
 if(msg==WM_CTLCOLORSTATIC||msg==WM_CTLCOLORBTN||msg==WM_CTLCOLORLISTBOX){SetTextColor((HDC)w,ui::foreground);SetBkColor((HDC)w,msg==WM_CTLCOLORLISTBOX?ui::surface:ui::background);return (LRESULT)(msg==WM_CTLCOLORLISTBOX?ui::surfaceBrush:ui::backgroundBrush);}
 if(msg==WM_NOTIFY){auto custom=reinterpret_cast<NMCUSTOMDRAW*>(l);if(custom->hdr.idFrom==batteryId&&custom->hdr.code==NM_CUSTOMDRAW&&custom->dwDrawStage==CDDS_PREPAINT){
  auto dc=custom->hdc;RECT r=custom->rc;FillRect(dc,&r,ui::backgroundBrush);RECT check{r.left+px(2),r.top+px(5),r.left+px(20),r.top+px(23)};
  auto pen=CreatePen(PS_SOLID,px(1),ui::accent);auto oldPen=SelectObject(dc,pen);auto oldBrush=SelectObject(dc,ui::surfaceBrush);Rectangle(dc,check.left,check.top,check.right,check.bottom);
  if(SendMessageW(custom->hdr.hwndFrom,BM_GETCHECK,0,0)==BST_CHECKED){MoveToEx(dc,check.left+px(3),check.top+px(9),nullptr);LineTo(dc,check.left+px(7),check.top+px(13));LineTo(dc,check.left+px(15),check.top+px(4));}
  SelectObject(dc,oldPen);SelectObject(dc,oldBrush);DeleteObject(pen);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,ui::foreground);auto oldFont=SelectObject(dc,font);r.left+=px(30);DrawTextW(dc,L"Pause on battery",-1,&r,DT_SINGLELINE|DT_VCENTER);SelectObject(dc,oldFont);
  if((custom->uItemState&CDIS_FOCUS)&&!(custom->uItemState&CDIS_DISABLED)&&!(SendMessageW(custom->hdr.hwndFrom,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS)){InflateRect(&check,px(3),px(3));ui::frame(dc,check,ui::accent);}return CDRF_SKIPDEFAULT;
 }}
 if(msg==WM_MEASUREITEM){auto d=reinterpret_cast<MEASUREITEMSTRUCT*>(l);d->itemHeight=px(26);return TRUE;}
 if(msg==WM_DRAWITEM){ui::draw(*reinterpret_cast<DRAWITEMSTRUCT*>(l),font,false,uiDpi);return TRUE;}

 if(msg==WM_COMMAND){int id=LOWORD(w),event=HIWORD(w);
  // Focus/open/close notifications must not reset a combo while its mouse
  // selection is in progress. Only committed selection changes update state.
  bool dropdown=id==randomIntervalId||id==visualizationId||id==monitorId||id==reactionId||id==qualityId||id==fpsId||id==paletteId||id==deviceId||id==videoFitId;
  if(dropdown&&event!=CBN_SELCHANGE)return 0;
  if(!dropdown&&event!=BN_CLICKED)return 0;
  if(id==advancedId){
   SendMessageW(hwnd,WM_SETREDRAW,FALSE,0);advancedOpen=!advancedOpen;
   // Reveal the live World while tuning, and restore the ordinary opaque library on collapse.
   auto style=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);
   SetWindowLongPtrW(hwnd,GWL_EXSTYLE,advancedOpen?style|WS_EX_LAYERED:style&~LONG_PTR(WS_EX_LAYERED));
   if(advancedOpen)SetLayeredWindowAttributes(hwnd,0,179,LWA_ALPHA);
   SetWindowTextW(GetDlgItem(hwnd,advancedId),advancedOpen?L"−  Advanced settings":L"+  Advanced settings");scrollOffset=0;SendMessageW(hwnd,WM_SETREDRAW,TRUE,0);resizeLibrary();return 0;
  }
  if(id==resetId){
   {std::lock_guard lock(stateMutex);if(!surfaces.empty()){auto& a=selected();a.params=Parameters{};a.reaction=Reaction::Off;a.fit=false;}Settings defaults;settings.quality=defaults.quality;settings.fps=defaults.fps;settings.pauseBattery=defaults.pauseBattery;settings.audioDevice.clear();requestSave();}
   syncControls();return 0;
  }
  if(id==importId||id==videoImportId){
   wchar_t file[32768]{};OPENFILENAMEW dialog{sizeof(dialog)};dialog.hwndOwner=hwnd;dialog.lpstrFilter=id==videoImportId?L"H.264 MP4 video (*.mp4)\0*.mp4\0":L"Voltura World (*.world)\0*.world\0";dialog.lpstrFile=file;dialog.nMaxFile=32768;dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;dialog.lpstrTitle=L"Import World";
   if(GetOpenFileNameW(&dialog))importWorld(file,id==videoImportId);return 0;
  }
  if(id==visualizationId){selectVisualization(int(SendDlgItemMessageW(hwnd,id,CB_GETCURSEL,0,0)));syncControls();return 0;}
  if(id==monitorId&&event==CBN_SELCHANGE){syncControls();return 0;}
  if(id==communityId){DestroyWindow(controller);return 0;}
  if(id==retryId){previewMode=false;createSurfaces();syncControls();return 0;}
  if(id==pauseId){{std::lock_guard lock(stateMutex);pauseState.manual=!pauseState.manual;}syncControls();return 0;}
  {std::lock_guard lock(stateMutex);if(surfaces.empty())return 0;auto& a=selected();
   if(id==applyAllId){auto copy=a;for(auto& s:surfaces){bool found=false;for(auto& [key,entry]:settings.displays)if(key==s.monitor.id){entry=copy;found=true;break;}if(!found)settings.displays.emplace_back(s.monitor.id,copy);s.randomCycle.reset(true);}}
   else if(event==CBN_SELCHANGE){int value=int(SendDlgItemMessageW(hwnd,id,CB_GETCURSEL,0,0));if(id==randomIntervalId&&value>=0&&value<int(randomIntervals.size())){a.randomInterval=randomIntervals[value];selectedSurface().randomCycle.reset(true);}if(id==reactionId)a.reaction=Reaction(value);if(id==qualityId)settings.quality=Quality(value);if(id==fpsId)settings.fps=value==0?30:60;if(id==paletteId)a.params.palette=value;if(id==videoFitId)a.fit=value==1;if(id==deviceId)settings.audioDevice=value>0?audioDevices.at(value-1).first:L"";}
   else if(id==batteryId)settings.pauseBattery=SendDlgItemMessageW(hwnd,id,BM_GETCHECK,0,0)==BST_CHECKED;
   requestSave();}
  syncControls();return 0;
 }
 if(msg==WM_HSCROLL){int id=GetDlgCtrlID((HWND)l);if(id>=200&&id<207){std::lock_guard lock(stateMutex);if(surfaces.empty())return 0;auto& p=selected().params;float* values[]={&p.brightness,&p.movement,&p.detail,&p.flow,&p.turbulence,&p.density,&p.strength};*values[id-200]=float(SendMessageW((HWND)l,TBM_GETPOS,0,0))/100;requestSave();}return 0;}
 }catch(const std::exception& e){error(e);}return DefWindowProcW(hwnd,msg,w,l);
}
void showLibrary(){
 if(library){ShowWindow(library,SW_RESTORE);SetForegroundWindow(library);return;}
 uiDpi=GetDpiForSystem();
 library=CreateWindowExW(WS_EX_APPWINDOW|WS_EX_COMPOSITED,L"VolturaWorldLibrary",L"Voltura Worlds",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_VSCROLL|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,px(780),px(500),nullptr,nullptr,instance,nullptr);
 uiDpi=GetDpiForWindow(library);updateUiFonts();ui::refresh(library);
 HWND title=control(L"STATIC",L"Your desktop, alive.",0,28,20,700,42,0);SendMessageW(title,WM_SETFONT,(WPARAM)headingFont,TRUE);label(L"Choose a World. Make it yours.",30,65,690);
 label(L"Display",30,108,110);combo(monitorId,150,102,570,{});for(auto& s:surfaces)SendDlgItemMessageW(library,monitorId,CB_ADDSTRING,0,(LPARAM)s.monitor.name.c_str());SendDlgItemMessageW(library,monitorId,CB_SETCURSEL,0,0);
 label(L"Visualization",30,158,120);combo(visualizationId,155,152,345,{});for(auto& v:visualizations)if(v.scene!=Scene::Video)SendDlgItemMessageW(library,visualizationId,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(v.name));
 combo(randomIntervalId,515,152,205,{L"Every 30 seconds",L"Every 1 minute",L"Every 2 minutes",L"Every 5 minutes",L"Every 10 minutes",L"Every 15 minutes",L"Every 30 minutes"});
 label(L"Audio reaction",30,232,120);combo(reactionId,155,226,170,{L"Off",L"Gentle",L"Expressive"});label(L"Palette",355,232,80);combo(paletteId,435,226,285,{L"Teal / violet / gold",L"Emerald / blue",L"Ember / gold",L"Silver"});
 control(L"BUTTON",L"Import video…",WS_TABSTOP,30,273,165,34,videoImportId);control(L"BUTTON",L"Import World…",WS_TABSTOP,205,273,165,34,importId);
 control(L"BUTTON",L"+  Advanced settings",WS_TABSTOP,385,273,210,34,advancedId);control(L"BUTTON",L"Reset settings",WS_TABSTOP,605,273,115,34,resetId);
 addingAdvanced=true;
 label(L"Fine-tune this World",30,327,690);
 const wchar_t* names[]={L"Brightness",L"Movement",L"Detail",L"Flow",L"Turbulence",L"Density",L"Audio strength"};for(int i=0;i<7;i++){int column=i%2,row=i/2;control(L"STATIC",names[i],0,30+column*350,367+row*44,115,24,210+i);HWND slider=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS|WS_TABSTOP,145+column*350,360+row*44,220,32,200+i);SendMessageW(slider,TBM_SETRANGE,TRUE,MAKELPARAM(0,i==6?100:200));}
 label(L"Video position",380,499,115);combo(videoFitId,495,493,220,{L"Fill",L"Fit"});
 label(L"Quality",30,546,110);combo(qualityId,145,540,220,{L"Eco",L"Auto",L"High"});label(L"Frame limit",380,546,115);combo(fpsId,495,540,220,{L"30 FPS",L"60 FPS"});
 label(L"Audio output",30,591,115);combo(deviceId,145,585,570,{L"Follow system output"});audioDevices=AudioCapture::devices();for(auto& d:audioDevices)SendDlgItemMessageW(library,deviceId,CB_ADDSTRING,0,(LPARAM)d.second.c_str());
 control(L"BUTTON",L"Pause on battery",BS_AUTOCHECKBOX|WS_TABSTOP,30,631,230,28,batteryId);
 label(L"Reset restores this World’s adjustments and playback preferences.",30,668,690);
 addingAdvanced=false;addingFooter=true;
 control(L"BUTTON",L"Apply to all displays",WS_TABSTOP,30,330,170,36,applyAllId);control(L"BUTTON",L"Pause",WS_TABSTOP,210,330,130,36,pauseId);control(L"BUTTON",L"Retry now",WS_TABSTOP,350,330,190,36,retryId);control(L"BUTTON",L"Exit Worlds",WS_TABSTOP,550,330,170,36,communityId);
 statusLabel=control(L"STATIC",L"",0,30,386,690,55,0);addingFooter=false;syncControls();resizeLibrary();ShowWindow(library,SW_SHOW);SetForegroundWindow(library);
}
HICON createTrayIcon(){
 // The notification area needs a full-size mark without the app icon's padded tile.
 Gdiplus::GdiplusStartupInput startup;ULONG_PTR token{};
 if(Gdiplus::GdiplusStartup(&token,&startup,nullptr)!=Gdiplus::Ok)return nullptr;
 HICON icon{};
 {
  const UINT dpi=GetDpiForSystem();
  const int width=GetSystemMetricsForDpi(SM_CXSMICON,dpi),height=GetSystemMetricsForDpi(SM_CYSMICON,dpi);
  Gdiplus::Bitmap artwork(256,256,PixelFormat32bppARGB);
  {
   Gdiplus::Graphics graphics(&artwork);
   graphics.Clear(Gdiplus::Color(0,0,0,0));
   graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
   Gdiplus::SolidBrush cyan(Gdiplus::Color(255,47,191,208)),blue(Gdiplus::Color(255,85,110,218));
   Gdiplus::Pen orbit(Gdiplus::Color(255,190,132,255),18.0f);
   // Paint the far semicircle behind the opaque planet, then the near semicircle.
   // Keep this geometry in sync with scripts/build-icons.ps1.
   graphics.TranslateTransform(128.0f,128.0f);
   graphics.RotateTransform(-40.0f);
   graphics.DrawEllipse(&orbit,-124.0f,-64.0f,248.0f,128.0f);
   graphics.ResetTransform();
   graphics.FillEllipse(&cyan,30,30,196,196);
   graphics.FillEllipse(&blue,64,40,158,174);
   graphics.TranslateTransform(128.0f,128.0f);
   graphics.RotateTransform(-40.0f);
   graphics.DrawArc(&orbit,-124.0f,-64.0f,248.0f,128.0f,0.0f,180.0f);
  }
  Gdiplus::Bitmap bitmap(width,height,PixelFormat32bppARGB);
  {
   Gdiplus::Graphics scaled(&bitmap);
   scaled.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
   scaled.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
   // Use the tray's remaining padding while preserving the complete ring.
   constexpr float scale=1.09f;
   scaled.DrawImage(&artwork,width*(1.0f-scale)*0.5f,height*(1.0f-scale)*0.5f,width*scale,height*scale);
  }
  bitmap.GetHICON(&icon);
 }
 Gdiplus::GdiplusShutdown(token);
 return icon;
}
void addTray(bool promote=true){if(trayIcon)DestroyIcon(trayIcon);trayIcon=createTrayIcon();NOTIFYICONDATAW n{sizeof(n)};n.hWnd=controller;n.uID=1;n.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;n.uCallbackMessage=trayMessage;n.hIcon=trayIcon?trayIcon:LoadIconW(instance,MAKEINTRESOURCEW(101));wcscpy_s(n.szTip,L"Voltura Worlds");Shell_NotifyIconW(NIM_ADD,&n);n.uVersion=NOTIFYICON_VERSION_4;Shell_NotifyIconW(NIM_SETVERSION,&n);if(promote){promotionAttempts=0;SetTimer(controller,2,250,nullptr);}}
void prepareTrayMenu(HWND owner,POINT point){
 // Windows scales native menus using their owner. Keep the hidden controller
 // on the invocation monitor, including after HDMI reconnects or scale changes.
 // An unrelated window under the pointer may use a different DPI awareness.
 SetWindowPos(owner,nullptr,point.x,point.y,1,1,SWP_NOZORDER|SWP_NOACTIVATE);
 menuDpi=GetDpiForWindow(owner);if(!menuDpi)menuDpi=GetDpiForSystem();
 menuFont=CreateFontW(-MulDiv(16,menuDpi,96),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
}
LRESULT CALLBACK controllerProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
 try{
 if(taskbarCreated&&msg==taskbarCreated){addTray();if(!previewMode)createSurfaces();return 0;}
 switch(msg){
 case openLibraryMessage:showLibrary();return 0;
 case WM_COPYDATA:{
  auto copy=reinterpret_cast<const COPYDATASTRUCT*>(l);
  if(!copy||copy->dwData!=0x574F524C||copy->cbData<sizeof(wchar_t)||copy->cbData>65536||copy->cbData%sizeof(wchar_t))return FALSE;
  auto path=static_cast<const wchar_t*>(copy->lpData);size_t length=copy->cbData/sizeof(wchar_t);
  if(!path||path[length-1]!=0||wcsnlen_s(path,length)!=length-1)return FALSE;
  importWorld(std::filesystem::path(path));return TRUE;
 }
 case trayMessage:if(LOWORD(l)==WM_LBUTTONUP||LOWORD(l)==NIN_SELECT)showLibrary();else if(LOWORD(l)==WM_CONTEXTMENU||LOWORD(l)==WM_RBUTTONUP){POINT point{};GetCursorPos(&point);prepareTrayMenu(hwnd,point);HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_OWNERDRAW,1,L"Open Worlds");AppendMenuW(menu,MF_POPUP|MF_OWNERDRAW,reinterpret_cast<UINT_PTR>(visualizationMenu()),L"Visualizations");AppendMenuW(menu,MF_OWNERDRAW,2,pauseState.manual?L"Resume":L"Pause");AppendMenuW(menu,MF_OWNERDRAW|(automaticUpdates?MF_CHECKED:0),4,L"Automatic updates");if(updates.ready())AppendMenuW(menu,MF_OWNERDRAW,5,L"Restart to update");AppendMenuW(menu,MF_OWNERDRAW,3,L"Exit Worlds");MENUINFO info{sizeof(info),MIM_BACKGROUND};info.hbrBack=ui::surfaceBrush;SetMenuInfo(menu,&info);SetForegroundWindow(hwnd);auto hook=SetWindowsHookExW(WH_CBT,ui::menuCreation,nullptr,GetCurrentThreadId());int cmd=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,point.x,point.y,0,hwnd,nullptr);if(hook)UnhookWindowsHookEx(hook);DestroyMenu(menu);DeleteObject(menuFont);menuFont=nullptr;if(cmd==1)showLibrary();if(cmd==2){std::lock_guard lock(stateMutex);pauseState.manual=!pauseState.manual;}if(cmd==3)DestroyWindow(hwnd);if(cmd==4){bool next=!automaticUpdates;wu::setEnabled(next);automaticUpdates=next;if(!next)updates.cancel();else nextUpdateCheck=GetTickCount64()+5000;}if(cmd==5&&updates.ready()){restartForUpdate=true;DestroyWindow(hwnd);}if(cmd>=visualizationMenuBase&&cmd<visualizationMenuBase+int(std::size(visualizations)))selectVisualization(cmd-visualizationMenuBase);}return 0;
 case errorMessage:syncControls();return 0;
 case renderStatusMessage:if(unsigned(l)==renderGeneration){renderingProblem=w!=0;syncControls();}return 0;
 case WM_MEASUREITEM:{auto d=reinterpret_cast<MEASUREITEMSTRUCT*>(l);d->itemWidth=MulDiv(230,menuDpi,96);d->itemHeight=MulDiv(36,menuDpi,96);return TRUE;}
 case WM_DRAWITEM:ui::draw(*reinterpret_cast<DRAWITEMSTRUCT*>(l),menuFont?menuFont:font,false,menuDpi);return TRUE;
 case WM_SETTINGCHANGE:ui::refresh(library);return 0;
 case WM_TIMER:{
  if(w==2){bool changed=false;bool promoted=ui::promoteTray(executable().wstring(),changed);if(promoted||++promotionAttempts>=20)KillTimer(hwnd,2);if(changed){NOTIFYICONDATAW n{sizeof(n)};n.hWnd=hwnd;n.uID=1;Shell_NotifyIconW(NIM_DELETE,&n);addTray(false);}return 0;}

  if(automaticUpdates&&!sessionEnding&&GetTickCount64()>=nextUpdateCheck){nextUpdateCheck=GetTickCount64()+3600000;updates.check();}
  if(!previewMode&&GetTickCount64()>=nextDesktopRetry&&(surfaces.empty()||std::any_of(surfaces.begin(),surfaces.end(),[](const Surface& s){return !wallpaperAttached(s.hwnd);})))createSurfaces();
  static ULONGLONG previousRandomTick=GetTickCount64();auto now=GetTickCount64();auto elapsed=std::min<ULONGLONG>(now-previousRandomTick,2000);previousRandomTick=now;
  SYSTEM_POWER_STATUS power{};GetSystemPowerStatus(&power);std::vector<bool> covered;for(auto& s:surfaces)covered.push_back(!previewMode&&monitorCovered(s.monitor,GetCurrentProcessId()));{std::lock_guard lock(stateMutex);pauseState.battery=power.ACLineStatus==0;pauseState.remote=GetSystemMetrics(SM_REMOTESESSION)!=0;for(size_t i=0;i<surfaces.size();i++)surfaces[i].covered=covered[i];}advanceRandomVisualizations(elapsed);}return 0;
 case WM_WTSSESSION_CHANGE:{std::lock_guard lock(stateMutex);if(w==WTS_SESSION_LOCK)pauseState.locked=true;if(w==WTS_SESSION_UNLOCK)pauseState.locked=false;}return 0;
 case WM_POWERBROADCAST:{std::lock_guard lock(stateMutex);if(w==PBT_APMSUSPEND)pauseState.suspended=true;if(w==PBT_APMRESUMEAUTOMATIC||w==PBT_APMRESUMESUSPEND)pauseState.suspended=false;if(w==PBT_POWERSETTINGCHANGE){auto p=reinterpret_cast<POWERBROADCAST_SETTING*>(l);if(p->PowerSetting==GUID_CONSOLE_DISPLAY_STATE&&p->DataLength==sizeof(DWORD)){DWORD state;memcpy(&state,p->Data,sizeof(state));pauseState.asleep=state==0;}}}return TRUE;
 case WM_DISPLAYCHANGE:createSurfaces();return 0;
 case WM_QUERYENDSESSION:sessionEnding=true;updates.sessionEnd();return TRUE;
 case WM_ENDSESSION:sessionEnding=true;updates.sessionEnd();if(w)DestroyWindow(hwnd);return 0;
 case WM_CLOSE:DestroyWindow(hwnd);return 0;
 case WM_DESTROY:{updates.close(automaticUpdates&&!sessionEnding,restartForUpdate);NOTIFYICONDATAW n{sizeof(n)};n.hWnd=hwnd;n.uID=1;Shell_NotifyIconW(NIM_DELETE,&n);if(trayIcon){DestroyIcon(trayIcon);trayIcon=nullptr;}WTSUnRegisterSessionNotification(hwnd);running=false;saveCv.notify_all();PostQuitMessage(0);}return 0;
 }
 }catch(const std::exception& e){error(e);}return DefWindowProcW(hwnd,msg,w,l);
}
int captureMode(const std::vector<std::wstring>& args){
 bool presentFrames=std::find(args.begin(),args.end(),L"--present")!=args.end();
 bool validateStars=std::find(args.begin(),args.end(),L"--validate-star-layer")!=args.end();
 auto option=[&](const wchar_t* key,const std::wstring& fallback){auto it=std::find(args.begin(),args.end(),key);return it!=args.end()&&it+1!=args.end()?*(it+1):fallback;};
 auto out=std::filesystem::absolute(option(L"--capture-dir",L"artifacts/native"));std::filesystem::create_directories(out);Scene scene=parseScene(utf8(option(L"--scene",L"aurora")));unsigned width=std::stoul(option(L"--width",L"1280")),height=std::stoul(option(L"--height",L"720"));int frames=std::stoi(option(L"--frames",L"180")),record=std::stoi(option(L"--record",L"0"));if(width<64||height<64||width>7680||height>4320||frames<1||frames>36000||record<0||record>900)throw std::runtime_error("Invalid capture limits");
 HWND hwnd=CreateWindowExW(WS_EX_TOOLWINDOW,L"VolturaWorldSurface",L"Voltura Worlds native validation",WS_POPUP,0,0,width,height,nullptr,nullptr,instance,nullptr);if(!hwnd)throw std::runtime_error("Cannot create validation surface");
 if(presentFrames)ShowWindow(hwnd,SW_SHOWNOACTIVATE);
 {Renderer renderer(hwnd,width,height,exeDir/L"shaders",std::find(args.begin(),args.end(),L"--debug-gpu")!=args.end(),std::find(args.begin(),args.end(),L"--single-sample")!=args.end());renderer.setGrid(std::stoul(option(L"--grid",L"64")));Assignment a;a.scene=scene;if(scene==Scene::Video){a.video=option(L"--video-file",L"");if(a.video.empty())throw std::runtime_error("Video capture requires --video-file");a.fit=std::find(args.begin(),args.end(),L"--fit")!=args.end();}FrameData data;data.dt=1.f/60;std::vector<double> timings;auto started=std::chrono::steady_clock::now();
  if(validateStars){if(scene!=Scene::Aurora)throw std::runtime_error("Star-layer validation requires Aurora");data.spare[0]=1;}
  bool multigrid=std::find(args.begin(),args.end(),L"--smoke-multigrid")!=args.end();
  if(multigrid){
   if(scene!=Scene::Smoke)throw std::runtime_error("Multigrid capture requires Smoke");
   for(auto flag:{L"--detail-coordinates",L"--fine-atlas",L"--flowmap-live",L"--flowmap-reference",L"--transport-test",L"--smoke-reference-volume",L"--wide-density"})if(std::find(args.begin(),args.end(),flag)!=args.end())throw std::runtime_error("Multigrid capture currently requires ordinary live Smoke");
   renderer.setPressureMultigrid(true);
  }
  renderer.setWideDensity(std::find(args.begin(),args.end(),L"--wide-density")!=args.end());
  auto referenceVolume=option(L"--smoke-reference-volume",L"");
  if(!referenceVolume.empty()){
   if(scene!=Scene::Smoke)throw std::runtime_error("Reference volume requires Smoke");
   for(auto incompatible:{L"--detail-coordinates",L"--validate-lifecycle"})if(std::find(args.begin(),args.end(),incompatible)!=args.end())throw std::runtime_error("Reference volume cannot be combined with simulation diagnostics");
   renderer.setReferenceVolume(referenceVolume);
  }
  auto referenceGrid=option(L"--reference-density-grid",L"");
  if(!referenceGrid.empty()){
   if(!option(L"--transport-test",L"").empty())throw std::runtime_error("Reference density override is only for frozen-volume captures");
   size_t consumed{};auto size=std::stoul(referenceGrid,&consumed);
   if(consumed!=referenceGrid.size()||size>384)throw std::runtime_error("Invalid reference density grid");
   renderer.setReferenceDensityGrid(static_cast<unsigned>(size));
  }
  renderer.setDetailCoordinates(std::find(args.begin(),args.end(),L"--detail-coordinates")!=args.end());
  bool fineAtlas=std::find(args.begin(),args.end(),L"--fine-atlas")!=args.end();
  if(fineAtlas){
   if(scene!=Scene::Smoke||!referenceVolume.empty()||!option(L"--transport-test",L"").empty())throw std::runtime_error("Fine atlas capture requires live Smoke");
   for(auto incompatible:{L"--flowmap-live",L"--flowmap-reference",L"--wide-density",L"--detail-coordinates"})if(std::find(args.begin(),args.end(),incompatible)!=args.end())throw std::runtime_error("Fine atlas cannot be combined with other density experiments");
   renderer.setFineAtlas(true);
  }
  bool fineAtlasFront=std::find(args.begin(),args.end(),L"--fine-atlas-front")!=args.end();
  if(fineAtlasFront)renderer.setFineAtlasFront(true);
  bool fineLighting=std::find(args.begin(),args.end(),L"--fine-atlas-lighting")!=args.end();
  if(fineLighting)renderer.setFineLighting(true);
  auto fineCurlOption=option(L"--fine-curl",L"");float fineCurl=0;
  if(!fineCurlOption.empty()){
   size_t consumed{};fineCurl=std::stof(fineCurlOption,&consumed);
   if(consumed!=fineCurlOption.size())throw std::runtime_error("Invalid fine curl strength");
   renderer.setFineCurl(fineCurl);
  }
  bool flowmapLive=std::find(args.begin(),args.end(),L"--flowmap-live")!=args.end();
  bool flowmapReference=std::find(args.begin(),args.end(),L"--flowmap-reference")!=args.end();
  if(flowmapReference){
   if(renderer.gridSize()>96)throw std::runtime_error("Flow map diagnostic supports velocity grids up to 96");
   if(referenceVolume.empty()||option(L"--transport-test",L"")!=L"maccormack")throw std::runtime_error("Flow map reference requires an imported volume and maccormack transport test");
   renderer.setDetailCoordinates(true);data.spare[9]=2;
  }
  if(flowmapLive){
   if(scene!=Scene::Smoke||flowmapReference||!referenceVolume.empty()||!option(L"--transport-test",L"").empty()||renderer.gridSize()>96)throw std::runtime_error("Live flow map requires Smoke, grid <=96, and no reference/transport diagnostic");
   renderer.setFlowmapLive(true);data.spare[9]=3;data.spare[0]=1;renderer.setFlowmapRenewal(.25f);
  }
  bool flowmapAdaptive=std::find(args.begin(),args.end(),L"--flowmap-adaptive")!=args.end();
  if(flowmapAdaptive){
   if(!flowmapLive||std::find(args.begin(),args.end(),L"--wide-density")!=args.end())throw std::runtime_error("Adaptive flow map requires live mode with cubic density layout");
   renderer.setFlowmapAdaptive(true);renderer.setFlowmapRenewal(2.f);
  }
  auto renewalOption=option(L"--flowmap-renew",L"");float renewalSeconds=flowmapAdaptive?2.f:flowmapLive?.25f:0.f;
  bool flowmapCubic=std::find(args.begin(),args.end(),L"--flowmap-cubic")!=args.end();
  if(flowmapCubic){if(!flowmapReference&&!flowmapLive)throw std::runtime_error("Flow map cubic requires flow-map reference mode");data.spare[0]=1;}
  if(!renewalOption.empty()){
   size_t consumed=0;renewalSeconds=std::stof(renewalOption,&consumed);
   if((!flowmapReference&&!flowmapLive)||consumed!=renewalOption.size()||!std::isfinite(renewalSeconds)||renewalSeconds<.25f||renewalSeconds>12.f)throw std::runtime_error("Flow map renewal requires flow-map reference mode and 0.25 to 12 seconds");
   renderer.setFlowmapRenewal(renewalSeconds);
  }
  if(std::find(args.begin(),args.end(),L"--no-empty-skip")!=args.end())data.spare[10]=1;
  bool profileStages=std::find(args.begin(),args.end(),L"--gpu-stages")!=args.end();
  renderer.setStageProfiling(profileStages);std::array<std::vector<double>,14> stageTimings;
  bool smokeFilaments=std::find(args.begin(),args.end(),L"--smoke-filaments")!=args.end();
  if(smokeFilaments){
   if(scene!=Scene::Smoke||fineAtlas||!referenceVolume.empty()||flowmapLive)throw std::runtime_error("Smoke filaments require the ordinary live Smoke capture");
   for(auto incompatible:{L"--detail-coordinates",L"--transport-test",L"--flowmap-reference"})if(std::find(args.begin(),args.end(),incompatible)!=args.end())throw std::runtime_error("Smoke filaments cannot be combined with transport diagnostics");
   renderer.setSmokeFilaments(true);
  }
  auto transportTest=option(L"--transport-test",L"");
  bool transportDeform=std::find(args.begin(),args.end(),L"--transport-deform")!=args.end();
  if(transportDeform){
   if(transportTest.empty()||referenceVolume.empty())throw std::runtime_error("Deformation diagnostic requires an imported volume and transport test");
   data.spare[11]=1;
  }
  if(!transportTest.empty()){
   if(scene!=Scene::Smoke||(transportTest!=L"maccormack"&&transportTest!=L"first-order"&&transportTest!=L"cubic"))throw std::runtime_error("Transport test requires Smoke and maccormack, first-order or cubic");
   data.spare[4]=transportTest==L"maccormack"?1.f:transportTest==L"cubic"?3.f:2.f;
  }
  if(std::find(args.begin(),args.end(),L"--aurora-procedural")!=args.end())data.spare[1]=1;
  if(std::find(args.begin(),args.end(),L"--validate-lifecycle")!=args.end()){
   const Scene lifecycleScenes[]={Scene::Aurora,Scene::Orbit,Scene::Mars,Scene::Earth,Scene::Moon,Scene::Jupiter,Scene::Orbit,Scene::Earth,Scene::Mars,Scene::Smoke,Scene::Bloom,Scene::Silk,Scene::Eclipse,Scene::Lattice,Scene::Drift,Scene::Contour,Scene::Mercury,Scene::Ripples};
   for(size_t cycle=0;cycle<std::size(lifecycleScenes)*2;cycle++){
    a.scene=lifecycleScenes[cycle%std::size(lifecycleScenes)];renderer.resize(cycle%2?width:640,cycle%2?height:360);
    for(int frame=0;frame<8;frame++){data.time+=data.dt;renderer.render(a,data,false);}
    renderer.validateDebugMessages();
   }
   renderer.resize(width,height);a.scene=scene;
   if(scene==Scene::Mars||scene==Scene::Earth||scene==Scene::Moon||scene==Scene::Jupiter){
    renderer.render(a,data,false);renderer.idle();auto marsBytes=renderer.allocations();
    for(int cycle=0;cycle<3;cycle++){
     a.scene=Scene::Orbit;renderer.render(a,data,false);a.scene=scene==Scene::Earth?Scene::Mars:Scene::Earth;renderer.render(a,data,false);a.scene=scene;renderer.render(a,data,false);
     renderer.resize(720,1280);renderer.render(a,data,false);renderer.resize(width,height);renderer.render(a,data,false);renderer.idle();
     if(renderer.allocations()!=marsBytes)throw std::runtime_error("Planet resources changed after space-scene/resize roundtrip");
     renderer.validateDebugMessages();
    }
   }
  }
  auto startTimeOption=option(L"--start-time",L"0");size_t startConsumed{};float startTime=std::stof(startTimeOption,&startConsumed);
  if(startConsumed!=startTimeOption.size()||!std::isfinite(startTime)||startTime<0||startTime>86400||(startTime!=0&&scene!=Scene::Orbit&&scene!=Scene::Mars&&scene!=Scene::Earth&&scene!=Scene::Moon&&scene!=Scene::Jupiter&&scene!=Scene::Aurora&&scene!=Scene::Bloom&&!isProceduralWorld(scene)))throw std::runtime_error("Capture start time requires a procedural World and a finite value from 0 to 86400 seconds");
  // Exercise the actual shader controls in native capture regressions.
  auto captureParameter=[&](const wchar_t* key){auto value=option(key,L"1");size_t consumed{};float number=std::stof(value,&consumed);if(consumed!=value.size()||!std::isfinite(number)||number<0||number>2)throw std::runtime_error("Capture parameter must be finite and between 0 and 2");return number;};
  data.density=captureParameter(L"--density");data.movement=captureParameter(L"--movement");data.brightness=captureParameter(L"--brightness");data.detail=captureParameter(L"--detail");data.flow=captureParameter(L"--flow");data.turbulence=captureParameter(L"--turbulence");
  auto paletteOption=option(L"--palette",L"0");size_t paletteConsumed{};int palette=std::stoi(paletteOption,&paletteConsumed);if(paletteConsumed!=paletteOption.size()||palette<0||palette>3)throw std::runtime_error("Capture palette must be 0 to 3");data.palette=float(palette);
  auto qualityOption=option(L"--quality",L"auto");int quality=qualityOption==L"eco"?0:qualityOption==L"auto"?1:qualityOption==L"high"?2:-1;if(quality<0)throw std::runtime_error("Capture quality must be eco, auto or high");if(isProceduralWorld(scene))data.spare[0]=float(quality);
  auto rippleProbe=option(L"--ripples-probe",L"0");size_t rippleConsumed{};int rippleMode=std::stoi(rippleProbe,&rippleConsumed);
  if(rippleConsumed!=rippleProbe.size()||rippleMode<0||rippleMode>6||(rippleMode&&scene!=Scene::Ripples))throw std::runtime_error("Ripples probe requires the ripples scene and a mode from 0 to 6");
  if(scene==Scene::Ripples)data.spare[1]=float(rippleMode);
  auto jupiterProbe=option(L"--jupiter-probe",L"0");size_t jupiterConsumed{};int jupiterMode=std::stoi(jupiterProbe,&jupiterConsumed);
  if(jupiterConsumed!=jupiterProbe.size()||jupiterMode<0||jupiterMode>4||(jupiterMode&&scene!=Scene::Jupiter))throw std::runtime_error("Jupiter probe requires Jupiter and a mode from 0 to 4");
  if(scene==Scene::Jupiter)data.spare[4]=float(jupiterMode);
  auto moonProbe=option(L"--moon-probe",L"0");size_t moonConsumed{};int moonMode=std::stoi(moonProbe,&moonConsumed);
  if(moonConsumed!=moonProbe.size()||moonMode<0||moonMode>3||(moonMode&&scene!=Scene::Moon))throw std::runtime_error("Moon probe requires Moon and a mode from 0 to 3");
  if(scene==Scene::Moon)data.spare[4]=float(moonMode);
  auto earthProbe=option(L"--earth-probe",L"0");size_t earthConsumed{};int earthMode=std::stoi(earthProbe,&earthConsumed);
  if(earthConsumed!=earthProbe.size()||earthMode<0||earthMode>8||(earthMode&&scene!=Scene::Earth))throw std::runtime_error("Earth probe requires Earth and a mode from 0 to 8");
  if(scene==Scene::Earth)data.spare[4]=float(earthMode);
  auto reactionOption=option(L"--reaction",L"off");a.reaction=reactionOption==L"off"?Reaction::Off:reactionOption==L"gentle"?Reaction::Gentle:reactionOption==L"expressive"?Reaction::Expressive:Reaction(-1);if(int(a.reaction)<0)throw std::runtime_error("Capture reaction must be off, gentle or expressive");
  float strength=std::find(args.begin(),args.end(),L"--strength")==args.end()?.5f:captureParameter(L"--strength");if(strength>1)throw std::runtime_error("Capture strength must be 0 to 1");data.reaction=(a.reaction==Reaction::Off?0.f:a.reaction==Reaction::Gentle?.35f:1.f)*strength;
  auto audioParameter=[&](const wchar_t* key){return std::find(args.begin(),args.end(),key)==args.end()?0.f:captureParameter(key);};data.bass=audioParameter(L"--bass");data.mid=audioParameter(L"--mid");data.treble=audioParameter(L"--treble");data.energy=audioParameter(L"--energy");data.impulse=audioParameter(L"--impulse");
  data.time+=startTime;
  auto timingWarmupOption=option(L"--timing-warmup",L"11");size_t timingConsumed{};int timingWarmup=std::stoi(timingWarmupOption,&timingConsumed);
  if(timingConsumed!=timingWarmupOption.size()||timingWarmup<0||timingWarmup>36000)throw std::runtime_error("Timing warmup must be an integer from 0 to 36000 frames");
  for(int i=0;i<frames;i++){data.time+=data.dt;renderer.render(a,data,presentFrames);if(presentFrames||scene==Scene::Video)std::this_thread::sleep_for(std::chrono::milliseconds(16));if(i>=timingWarmup){timings.push_back(renderer.gpuMs());if(profileStages)for(unsigned stage=0;stage<14;stage++)stageTimings[stage].push_back(renderer.gpuStageMs()[stage]);}}
  renderer.capture(out/(wide(sceneName(scene))+L".png"));
  if(std::find(args.begin(),args.end(),L"--validate-paused-frame")!=args.end()){
   if(scene!=Scene::Smoke)throw std::runtime_error("Paused-frame diagnostic requires Smoke");
   FrameData frozen=data;frozen.dt=0;
   for(int i=0;i<3;i++)renderer.render(a,frozen,false);
   renderer.capture(out/L"smoke-paused.png");
  }
  if(std::find(args.begin(),args.end(),L"--fluid-diagnostics")!=args.end())renderer.captureFluidFields(out);
  if(std::find(args.begin(),args.end(),L"--smoke-render-diagnostics")!=args.end()){
   if(scene!=Scene::Smoke)throw std::runtime_error("Smoke rendering diagnostics require Smoke");
   FrameData frozen=data;frozen.dt=0;
   const wchar_t* names[]={L"smoke-unlit.png",L"smoke-density-slice.png",L"smoke-opacity.png",L"smoke-white-lit.png"};
   for(int mode=1;mode<=4;mode++){
    frozen.spare[3]=float(mode);renderer.render(a,frozen,false);renderer.capture(out/names[mode-1]);
   }
  }
  for(int i=0;i<record;i++){data.time+=1.f/30;data.dt=1.f/30;
   if(validateStars){data.time+=7;data.movement=float(i+1);data.energy=float(i%2);data.reaction=1;data.palette=float(i%4);}
   renderer.render(a,data,false);wchar_t name[64];swprintf_s(name,L"frame-%05d.png",i);renderer.capture(out/name);}
  renderer.validateDebugMessages();PROCESS_MEMORY_COUNTERS_EX memory{};GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory));std::sort(timings.begin(),timings.end());double p95=timings.empty()?0:timings[size_t((timings.size()-1)*.95)];nlohmann::json report={{"scene",sceneName(scene)},{"width",width},{"height",height},{"warmupFrames",frames},{"adapter",utf8(renderer.adapterName())},{"gpuP95Ms",p95},{"privateBytes",memory.PrivateUsage},{"estimatedGpuBytes",renderer.allocations()},{"grid",renderer.gridSize()},{"densityGrid",renderer.densityGridSize()},{"densityDimensions",renderer.densityDimensions()},{"sceneSamples",renderer.sceneSamples()},{"pressureMultigrid",multigrid},{"elapsedSeconds",std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()},{"evidence","Native D3D12 offscreen readback; not physical desktop or presentation performance"}};
  report["startTimeOffsetSeconds"]=startTime;
  report["timingWarmupFrames"]=timingWarmup;
  report["timingSampleCount"]=timings.size();
  if(isProceduralWorld(scene)||scene==Scene::Mars||scene==Scene::Earth||scene==Scene::Moon||scene==Scene::Jupiter)report["quality"]=utf8(qualityOption);
  if(flowmapReference||flowmapLive){report["flowmapLive"]=flowmapLive;report["flowmapAdaptive"]=flowmapAdaptive;report["flowmapAdaptiveRenewals"]=renderer.flowmapAdaptiveRenewals();report["flowmapReconstruction"]=flowmapCubic||flowmapLive?"Bounded Catmull-Rom":"Trilinear";report["flowmapRenewalSeconds"]=renewalSeconds;report["flowmapRenewals"]=renderer.flowmapRenewals();}
  if(!transportTest.empty())report["transportField"]=transportDeform?"Reversible nonlinear deformation":"Rigid rotation";
  if(!referenceVolume.empty()){report["densitySource"]=transportTest.empty()?"Imported frozen RGBA16F reference volume":"Imported RGBA16F analytic-transport seed";report["simulationSteps"]=transportTest.empty()?0:frames;report["flowmapReference"]=flowmapReference;report["evidence"]="Native D3D12 imported-volume diagnostic; simulationSteps identifies analytic transport; not live smoke or physical display";}
  if(profileStages){
   double totalGpuMs=0;for(double value:timings)totalGpuMs+=value;report["gpuMeanMs"]=timings.empty()?0:totalGpuMs/timings.size();
   const char* names[]={"clear","velocityAdvectionForces","curl","confinement","divergence","pressureIterations","projection","densityCorrection","lighting","densityForward","detailCoordinatesAndField","sceneDraw","densityCopy","postProcessing"};
   for(unsigned stage=0;stage<14;stage++){auto values=stageTimings[stage];double total=0;for(double value:values)total+=value;std::sort(values.begin(),values.end());report["gpuStages"][names[stage]]={{"samples",values.size()},{"meanMs",values.empty()?0:total/values.size()},{"p95Ms",values.empty()?0:values[size_t((values.size()-1)*.95)]}};}
   report["gpuStageEvidence"]="Non-overlapping GPU timestamp scopes on completed fences; pressure aggregates all iterations; scopes omit some command gaps; profiling adds overhead.";
  }
  if(smokeFilaments)report["smokeFilaments"]="Persistent fluid-advected gold tracers; experimental overlay";
  if(fineAtlas){report["fineLighting"]=fineLighting;report["fineCurlStrength"]=fineCurl;report["fineAtlas"]=true;report["fineAtlasGrid"]=256;report["fineAtlasResidentTiles"]=fineAtlasFront?3840:512;report["fineAtlasRegion"]=fineAtlasFront?"x [0.25,0.75], y [0.125,0.875], z [0.25,0.5625]":"[0.375,0.625] cubed";}
  std::ofstream f(out/L"metrics.json");f<<report.dump(2);
 }DestroyWindow(hwnd);return 0;
}
}
int WINAPI wWinMain(HINSTANCE h,HINSTANCE,PWSTR,int){
 instance=h;CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);int result=0;
 try{
  exeDir=executable().parent_path();int argc{};LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);int helperResult=wu::updateHelper(argc,argv);if(helperResult>=0){LocalFree(argv);CoUninitialize();return helperResult;}std::vector<std::wstring> args(argv+1,argv+argc);LocalFree(argv);
  // A single token avoids pixtool's multi-argument command-line parser limitations.
  if(args.size()==1&&args[0].starts_with(L"profile-")){
   auto scene=args[0].substr(8);parseScene(utf8(scene));
   args={L"--capture-dir",(exeDir/L"profile-output"/scene).wstring(),L"--scene",scene,L"--width",L"1920",L"--height",L"1080",L"--frames",L"600",L"--present"};
  }
  WNDCLASSW surface{};surface.hInstance=h;surface.lpfnWndProc=wallpaperProc;surface.lpszClassName=L"VolturaWorldSurface";surface.hIcon=LoadIconW(h,MAKEINTRESOURCEW(101));surface.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&surface);
  if(std::find(args.begin(),args.end(),L"--capture-dir")!=args.end()){automated=true;result=captureMode(args);CoUninitialize();return result;}
  std::filesystem::path importFile;
  auto importArgument=std::find(args.begin(),args.end(),L"--import");
  if(importArgument!=args.end()){
   if(importArgument+1==args.end())throw std::runtime_error("Choose a .world file to import.");
   importFile=std::filesystem::absolute(*(importArgument+1));
  }
  if(!wu::updateReaderMayStart()){CoUninitialize();return 0;}
  singleInstance=CreateMutexW(nullptr,TRUE,L"Local\\Voltura.Worlds");
  if(!singleInstance)throw std::runtime_error("Cannot coordinate Worlds startup.");
  bool anotherInstance=GetLastError()==ERROR_ALREADY_EXISTS;
  HWND existing{};
  if(anotherInstance){
   // The first process owns its mutex before settings and its controller are
   // ready. Preserve this launch request while that process is starting.
   auto deadline=GetTickCount64()+10000;
   while(!(existing=FindWindowW(L"VolturaWorldController",nullptr))){
    DWORD wait=WaitForSingleObject(singleInstance,25);
    if(wait==WAIT_OBJECT_0||wait==WAIT_ABANDONED){anotherInstance=false;break;}
    if(wait==WAIT_FAILED)throw std::runtime_error("Cannot wait for Worlds startup.");
    if(GetTickCount64()>=deadline)throw std::runtime_error("Worlds is still starting or is not responding. Please try again.");
   }
  }
  if(anotherInstance){
   if(std::find(args.begin(),args.end(),L"--wallpaper")==args.end()){
    if(existing){DWORD owner{};GetWindowThreadProcessId(existing,&owner);AllowSetForegroundWindow(owner);
     if(!importFile.empty()){
      auto path=importFile.wstring();COPYDATASTRUCT copy{0x574F524C,DWORD((path.size()+1)*sizeof(wchar_t)),path.data()};DWORD_PTR accepted{};
      if(!SendMessageTimeoutW(existing,WM_COPYDATA,0,reinterpret_cast<LPARAM>(&copy),SMTO_ABORTIFHUNG,5000,&accepted)||!accepted)throw std::runtime_error("World import could not be handed to the running application.");
     }else if(!PostMessageW(existing,openLibraryMessage,0,0))throw std::runtime_error("Worlds could not open its library. Please try again.");
    }
   }
   if(singleInstance)CloseHandle(singleInstance);CoUninitialize();return 0;
  }
  automaticUpdates=wu::enabled();nextUpdateCheck=GetTickCount64()+5000;
  profile=profilePath();try{settings=loadSettings(profile);}catch(const std::exception& e){error(e);settings={};}
  INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_STANDARD_CLASSES|ICC_BAR_CLASSES};InitCommonControlsEx(&controls);uiDpi=GetDpiForSystem();updateUiFonts();
  ui::refresh();WNDCLASSW cls{};cls.hIcon=LoadIconW(h,MAKEINTRESOURCEW(101));cls.hInstance=h;cls.lpfnWndProc=controllerProc;cls.lpszClassName=L"VolturaWorldController";RegisterClassW(&cls);cls.lpfnWndProc=libraryProc;cls.lpszClassName=L"VolturaWorldLibrary";cls.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&cls);
  controller=CreateWindowW(L"VolturaWorldController",L"Voltura Worlds",WS_OVERLAPPED,0,0,0,0,nullptr,nullptr,h,nullptr);taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");addTray();WTSRegisterSessionNotification(controller,NOTIFY_FOR_THIS_SESSION);HPOWERNOTIFY powerNotify=RegisterPowerSettingNotification(controller,&GUID_CONSOLE_DISPLAY_STATE,DEVICE_NOTIFY_WINDOW_HANDLE);SetTimer(controller,1,1000,nullptr);
  // Claim foreground activation while this user launch still owns it, before
  // wallpaper-host discovery and renderer setup can hand focus back to Explorer.
  // Windows startup uses --wallpaper and must never open the library by itself.
  bool backgroundLaunch=std::find(args.begin(),args.end(),L"--wallpaper")!=args.end();
  if(!backgroundLaunch||!importFile.empty())showLibrary();
  previewMode=std::find(args.begin(),args.end(),L"--preview")!=args.end();createSurfaces();
  saveThread=std::jthread([](std::stop_token stop){while(!stop.stop_requested()){Settings snapshot;{std::unique_lock lock(stateMutex);saveCv.wait_for(lock,std::chrono::milliseconds(300),[&]{return savePending||!running;});if(!savePending){if(!running)break;continue;}snapshot=settings;savePending=false;}try{saveSettings(profile,snapshot);}catch(const std::exception& e){{std::lock_guard lock(stateMutex);lastError=wide(e.what());}PostMessageW(controller,errorMessage,0,0);}if(!running)break;}});
  if(!importFile.empty())importWorld(importFile);
  MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){HWND root=GetAncestor(msg.hwnd,GA_ROOT);wchar_t name[64]{};GetClassNameW(root,name,64);if(!wcscmp(name,L"VolturaWorldImport")&&IsDialogMessageW(root,&msg))continue;if(library&&IsDialogMessageW(library,&msg))continue;TranslateMessage(&msg);DispatchMessageW(&msg);}if(powerNotify)UnregisterPowerSettingNotification(powerNotify);
 }catch(const std::exception& e){error(e);result=1;}
 renderThread.request_stop();if(renderThread.joinable())renderThread.join();running=false;saveCv.notify_all();if(saveThread.joinable())saveThread.join();audio.configure(false,{});for(auto& s:surfaces)if(IsWindow(s.hwnd))DestroyWindow(s.hwnd);if(library&&IsWindow(library))DestroyWindow(library);if(font)DeleteObject(font);if(headingFont)DeleteObject(headingFont);if(singleInstance)CloseHandle(singleInstance);CoUninitialize();return result;
}

