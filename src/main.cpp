#include "renderer.h"
#include "random_cycle.h"
#include "performance.h"
#include "updates.h"
#include "desktop.h"
#include "audio.h"
#include "package.h"
#include "import_ui.h"
#include "presets_ui.h"
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
#include <iomanip>
#include <set>
using namespace worlds;
namespace {
constexpr UINT trayMessage=WM_APP+1,errorMessage=WM_APP+2,openLibraryMessage=WM_APP+3,renderStatusMessage=WM_APP+4;
constexpr int monitorId=100,visualizationId=110,applyAllId=113,reactionId=120,qualityId=121,fpsId=122,paletteId=123,batteryId=124,deviceId=125,pauseId=126,retryId=127,communityId=128,importId=129,videoImportId=130,videoFitId=131,advancedId=132,resetId=133,randomIntervalId=134,bodyStatisticsId=135,performanceStatusId=136,outputId=137,videoSpeedId=138,videoCenterId=139,videoXId=140,videoYId=141,presetsId=142;
struct Visualization { Scene scene; const wchar_t* name; };
constexpr Visualization visualizations[]={{Scene::Aurora,L"Aurora"},{Scene::Orbit,L"Orbit"},{Scene::Mars,L"Mars"},{Scene::Earth,L"Earth"},{Scene::Moon,L"Moon"},{Scene::Jupiter,L"Jupiter"},{Scene::Saturn,L"Saturn"},{Scene::Mercury,L"Mercury"},{Scene::Venus,L"Venus"},{Scene::Neptune,L"Neptune"},{Scene::Uranus,L"Uranus"},{Scene::Pluto,L"Pluto"},{Scene::Sun,L"Sun"},{Scene::Smoke,L"Smoke"},{Scene::Bloom,L"Bloom"},{Scene::Silk,L"Silk"},{Scene::Eclipse,L"Eclipse"},{Scene::Lattice,L"Lattice"},{Scene::Drift,L"Drift"},{Scene::Contour,L"Contour"},{Scene::PlasticMetal,L"Plastic metal"},{Scene::Ripples,L"Ripples"},{Scene::Off,L"Random"},{Scene::Off,L"Random Solar System"},{Scene::Off,L"Off"},{Scene::Video,L"Imported video"}};
constexpr int visualizationMenuBase=1000;
constexpr int randomChoice=int(std::size(visualizations))-4;
constexpr int solarRandomChoice=randomChoice+1;
bool isRandomChoice(int choice){return choice==randomChoice||choice==solarRandomChoice;}
int randomChoiceFor(const Assignment& a){return a.randomPool==RandomPool::SolarSystem?solarRandomChoice:randomChoice;}
struct Surface {HWND hwnd{};Monitor monitor;bool covered=false;RandomCycle randomCycle;bool renderingFailed=false,outputFallback=false;std::wstring performanceStatus;uint64_t renderedFrames=0;double effectiveFps=0;std::optional<double> pendingVideoSpeed;std::wstring videoMessage;};
HWND controller{},library{},statusLabel{};HINSTANCE instance{};HFONT font{},headingFont{};
HICON trayIcon{};
std::vector<Surface> surfaces;std::vector<std::pair<std::wstring,std::wstring>> audioDevices;
std::filesystem::path profile,exeDir;Settings settings;PauseState pauseState;
std::mutex stateMutex;std::condition_variable saveCv;std::jthread renderThread,saveThread;bool savePending=false;
std::wstring lastError;bool previewMode=true,desktopConnectionProblem=false,renderingProblem=false,outputPresentationProblem=false,automated=false;std::atomic<bool> running{true};std::atomic<unsigned> outputRevision{0};
UINT menuDpi=96;HFONT menuFont{};
unsigned renderGeneration=0;std::atomic<bool> renderSetupFailed{false};
UINT taskbarCreated{};AudioCapture audio;HANDLE singleInstance{};
ULONGLONG nextDesktopRetry{},nextUpdateCheck{};
wu::Updates updates;bool automaticUpdates=false,sessionEnding=false,restartForUpdate=false;
struct LayoutItem{HWND hwnd;int x,y,w,h;bool advanced,footer;int sx{},sy{},sw{},sh{};};
std::vector<LayoutItem> layoutItems;bool advancedOpen=false,addingAdvanced=false,addingFooter=false,layingOut=false,sideExpansion=false;
int scrollOffset=0;UINT uiDpi=96;unsigned promotionAttempts=0;
int px(int value){return MulDiv(value,uiDpi,96);}
void layoutLibrary(){
 if(!library||layingOut)return;layingOut=true;RECT client{};GetClientRect(library,&client);
 int contentHeight=advancedOpen?(sideExpansion?722:998):458;
 int total=px(contentHeight);int visible=client.bottom;
 scrollOffset=std::clamp(scrollOffset,0,std::max(0,total-visible));
 SCROLLINFO si{sizeof(si),SIF_RANGE|SIF_PAGE|SIF_POS,0,total-1,UINT(std::max(0,visible)),scrollOffset,0};SetScrollInfo(library,SB_VERT,&si,TRUE);
 HDWP batch=BeginDeferWindowPos(int(layoutItems.size()));
 for(auto& item:layoutItems){bool show=!item.advanced||advancedOpen;bool side=item.advanced&&sideExpansion;int x=side?item.sx:item.x,y=side?item.sy:item.y,width=side?item.sw:item.w,height=side?item.sh:item.h;
  if(GetDlgCtrlID(item.hwnd)==retryId)show=previewMode||desktopConnectionProblem||renderingProblem||outputPresentationProblem;
  if(item.footer&&advancedOpen&&!sideExpansion)y+=540;
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
 RECT wide{0,0,px(1240),px(722)};auto style=DWORD(GetWindowLongPtrW(library,GWL_STYLE));AdjustWindowRectExForDpi(&wide,style,FALSE,WS_EX_APPWINDOW,uiDpi);
 sideExpansion=advancedOpen&&wide.right-wide.left<=work.right-work.left-16;
 RECT r{0,0,px(sideExpansion?1240:760),px(advancedOpen?(sideExpansion?722:998):458)};
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
  if(y==327){item.sx=770;item.sy=24;item.sw=200;}
  else if(id==resetId){item.sx=990;item.sy=20;item.sw=200;}
  else if(id>=200&&id<207){int i=id-200;item.sx=770+(i%2)*220;item.sy=95+(i/2)*56;item.sw=200;item.sh=24;}
  else if(y>=367&&y<=499&&(y-367)%44==0){item.sx=770+(x>=380?220:0);item.sy=65+(y-367)/44*56;item.sw=200;}
  else if(id==videoFitId){item.sx=990;item.sy=263;item.sw=200;}
  else if(y==546){item.sx=770+(x>=380?220:0);item.sy=310;item.sw=200;}
  else if(id==qualityId||id==fpsId){item.sx=id==qualityId?770:990;item.sy=340;item.sw=200;}
  else if(y==591){item.sx=770;item.sy=382;item.sw=420;}
  else if(id==deviceId){item.sx=770;item.sy=412;item.sw=420;}
  else if(id==batteryId){item.sx=770;item.sy=460;item.sw=200;}
  else if(id==outputId){item.sx=1065;item.sy=454;item.sw=125;}
  else if(y==637){item.sx=990;item.sy=460;item.sw=70;}
  else if(y==668){item.sx=770;item.sy=504;item.sw=420;item.sh=50;}
 }
 if(addingAdvanced&&y>=540&&id!=videoXId&&id!=videoYId&&id!=videoSpeedId&&id!=videoCenterId&&id!=240&&id!=241&&id!=242){item.y+=150;item.sy+=150;}
 if(id==240||id==241){item.sx=id==240?770:990;item.sy=310;item.sw=200;}
 if(id==videoXId||id==videoYId){item.sx=id==videoXId?770:990;item.sy=340;item.sw=200;item.sh=24;}
 if(id==242){item.sx=770;item.sy=378;item.sw=200;}
 if(id==videoSpeedId||id==videoCenterId){item.sx=id==videoSpeedId?770:990;item.sy=406;item.sw=200;}
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
 outputPresentationProblem=!surfaces.empty()&&selectedSurface().outputFallback;
 bool showRetry=previewMode||desktopConnectionProblem||renderingProblem||outputPresentationProblem;
 if(bool(GetWindowLongPtrW(retry,GWL_STYLE)&WS_VISIBLE)!=showRetry){
  if(!showRetry&&GetFocus()==retry)SetFocus(GetDlgItem(library,pauseId));
  ShowWindow(retry,showRetry?SW_SHOWNA:SW_HIDE);
 }
 if(surfaces.empty()){setControlText(statusLabel,lastError.empty()?L"No display is available.":lastError);return;}
 auto& a=selected();
 // Planet Worlds reuse saved parameters with scene-specific, meaningful labels.
 const wchar_t* uranusNames[]={L"Brightness",L"Movement",L"Cloud detail",L"Band contrast",L"Cloud drift",L"Atmosphere",L"Audio strength"};
 const wchar_t* regularNames[]={L"Brightness",L"Movement",L"Detail",L"Flow",L"Turbulence",L"Density",L"Audio strength"};
 const wchar_t* sunNames[]={L"Brightness",L"Movement",L"Surface detail",L"Camera drift",L"Solar activity",L"Corona",L"Audio strength"};
 const wchar_t* plutoNames[]={L"Brightness",L"Movement",L"Surface detail",L"Camera drift",L"Sun angle",L"Atmosphere",L"Audio strength"};
 const wchar_t* mercuryNames[]={L"Brightness",L"Movement",L"Terrain relief",L"Camera drift",L"Sun angle",L"Surface contrast",L"Audio strength"};
 const wchar_t* marsNames[]={L"Brightness",L"Movement",L"Terrain relief",L"Camera drift",L"Dust variation",L"Atmosphere",L"Audio strength"};
 const wchar_t* jupiterNames[]={L"Brightness",L"Movement",L"Cloud detail",L"Camera drift",L"Cloud drift",L"Atmosphere",L"Audio strength"};
 const wchar_t* moonNames[]={L"Brightness",L"Movement",L"Terrain relief",L"Camera drift",L"Sun angle",L"Earth brightness",L"Audio strength"};
 const wchar_t* earthNames[]={L"Brightness",L"Movement",L"Cloud detail",L"Camera drift",L"Cloud drift",L"Atmosphere",L"Audio strength"};
 for(int i=0;i<7;i++)setControlText(GetDlgItem(library,210+i),(a.scene==Scene::Sun?sunNames:a.scene==Scene::Pluto?plutoNames:a.scene==Scene::Uranus?uranusNames:a.scene==Scene::Mercury?mercuryNames:(a.scene==Scene::Saturn||a.scene==Scene::Jupiter||a.scene==Scene::Venus||a.scene==Scene::Neptune)?jupiterNames:a.scene==Scene::Moon?moonNames:a.scene==Scene::Earth?earthNames:a.scene==Scene::Mars?marsNames:regularNames)[i]);
 const wchar_t* regularPalettes[]={L"Teal / violet / gold",L"Emerald / blue",L"Ember / gold",L"Silver"};
 const wchar_t* uranusPalettes[]={L"Natural Uranus",L"Pale aqua",L"Cool blue",L"Neutral survey"};
 const wchar_t* neptunePalettes[]={L"Cinematic blue",L"Natural blue-green",L"Deep azure",L"Neutral survey"};
 const wchar_t* venusPalettes[]={L"Natural Venus",L"Warm sulfur",L"Cool ivory",L"Neutral survey"};
 const wchar_t* sunPalettes[]={L"SDO orange (false color)",L"Golden corona (false color)",L"Deep amber (false color)",L"Neutral intensity (false color)"};
 const wchar_t* plutoPalettes[]={L"Natural Pluto",L"Warm ice",L"Cool ice",L"Neutral survey"};
 const wchar_t* mercuryPalettes[]={L"Natural Mercury",L"Warm stone",L"Cool gray",L"Neutral survey"};
 const wchar_t* marsPalettes[]={L"Natural Mars",L"Warm dust",L"Cool dusk",L"Survey neutral"};
 const wchar_t* saturnPalettes[]={L"Natural Saturn",L"Warm gold",L"Cool haze",L"Neutral survey"};
 const wchar_t* jupiterPalettes[]={L"Natural Jupiter",L"Warm belts",L"Cool haze",L"Neutral survey"};
 const wchar_t* moonPalettes[]={L"Natural Moon",L"Warm sunlight",L"Cool silver",L"Neutral survey"};
 const wchar_t* earthPalettes[]={L"Natural Earth",L"Warm daylight",L"Cool daylight",L"Neutral survey"};
 auto palettes=a.scene==Scene::Sun?sunPalettes:a.scene==Scene::Pluto?plutoPalettes:a.scene==Scene::Neptune?neptunePalettes:a.scene==Scene::Uranus?uranusPalettes:a.scene==Scene::Venus?venusPalettes:a.scene==Scene::Mercury?mercuryPalettes:a.scene==Scene::Saturn?saturnPalettes:a.scene==Scene::Jupiter?jupiterPalettes:a.scene==Scene::Moon?moonPalettes:a.scene==Scene::Earth?earthPalettes:a.scene==Scene::Mars?marsPalettes:regularPalettes;
 wchar_t firstPalette[80]{};SendDlgItemMessageW(library,paletteId,CB_GETLBTEXT,0,reinterpret_cast<LPARAM>(firstPalette));
 if(wcscmp(firstPalette,palettes[0])!=0){
  SendDlgItemMessageW(library,paletteId,CB_RESETCONTENT,0,0);
  for(int i=0;i<4;i++)SendDlgItemMessageW(library,paletteId,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(palettes[i]));
 }
 // Native setters can erase and synchronously repaint even an unchanged value.
 setControlText(GetDlgItem(library,pauseId),pauseState.manual?L"Resume":L"Pause");
 setComboSelection(outputId,int(a.output));setComboSelection(reactionId,int(a.reaction));setComboSelection(qualityId,int(a.performance.quality));setComboSelection(fpsId,int(a.performance.frameLimit));setComboSelection(paletteId,a.params.palette);
 int check=settings.pauseBattery?BST_CHECKED:BST_UNCHECKED;if(SendDlgItemMessageW(library,batteryId,BM_GETCHECK,0,0)!=check)SendDlgItemMessageW(library,batteryId,BM_SETCHECK,check,0);
 float values[]={a.params.brightness,a.params.movement,a.params.detail,a.params.flow,a.params.turbulence,a.params.density,a.params.strength};for(int i=0;i<7;i++)if(SendDlgItemMessageW(library,200+i,TBM_GETPOS,0,0)!=int(values[i]*100))SendDlgItemMessageW(library,200+i,TBM_SETPOS,TRUE,int(values[i]*100));
 int dev=0;for(size_t i=0;i<audioDevices.size();i++)if(audioDevices[i].first==settings.audioDevice)dev=int(i)+1;setComboSelection(deviceId,dev);
 auto speed=selectedSurface().pendingVideoSpeed.value_or(a.videoSettings.speed);auto speedIt=std::find_if(videoSpeeds.begin(),videoSpeeds.end(),[&](double r){return std::abs(r-speed)<1e-6;});setComboSelection(videoSpeedId,int(speedIt-videoSpeeds.begin()));
 for(auto pair:std::array<std::pair<int,double>,2>{{{videoXId,a.videoSettings.positionX},{videoYId,a.videoSettings.positionY}}})if(SendDlgItemMessageW(library,pair.first,TBM_GETPOS,0,0)!=int(std::lround(pair.second*100)))SendDlgItemMessageW(library,pair.first,TBM_SETPOS,TRUE,int(std::lround(pair.second*100)));
 for(int id:{videoXId,videoYId,videoCenterId})EnableWindow(GetDlgItem(library,id),a.scene==Scene::Video&&!a.fit);
 EnableWindow(GetDlgItem(library,videoSpeedId),a.scene==Scene::Video);
 setComboSelection(videoFitId,a.fit?1:0);EnableWindow(GetDlgItem(library,videoFitId),a.scene==Scene::Video);
 EnableWindow(GetDlgItem(library,reactionId),a.scene!=Scene::Video&&a.scene!=Scene::Off);EnableWindow(GetDlgItem(library,paletteId),a.scene!=Scene::Video&&a.scene!=Scene::Off);for(int id=200;id<207;id++)EnableWindow(GetDlgItem(library,id),a.scene!=Scene::Video&&a.scene!=Scene::Off);
 auto scene=wide(sceneName(a.scene));if(a.random)scene=(a.randomPool==RandomPool::SolarSystem?L"Random Solar System \u00b7 ":L"Random \u00b7 ")+scene;std::wstring text=previewMode?L"Preview mode · ":L"Desktop · ";text+=scene;text+=L"   |   Audio ";text+=a.reaction==Reaction::Off?L"off":a.reaction==Reaction::Gentle?L"gentle":L"expressive";if(pauseState.manual)text+=L"   |   Paused";if(!lastError.empty())text+=L"   |   "+lastError;
 if(!selectedSurface().videoMessage.empty())text+=L" | "+selectedSurface().videoMessage;
 setControlText(statusLabel,text);
 setControlText(GetDlgItem(library,performanceStatusId),selectedSurface().performanceStatus.empty()?L"Performance will appear during playback.":selectedSurface().performanceStatus);
 int videoChoice=int(std::size(visualizations))-1;
 auto count=SendDlgItemMessageW(library,visualizationId,CB_GETCOUNT,0,0);
 if(!a.video.empty()&&count==videoChoice)SendDlgItemMessageW(library,visualizationId,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(visualizations[videoChoice].name));
 else if(a.video.empty()&&count>videoChoice)SendDlgItemMessageW(library,visualizationId,CB_DELETESTRING,videoChoice,0);
 int choice=-1;for(size_t i=0;i<std::size(visualizations);i++)if(!isRandomChoice(int(i))&&visualizations[i].scene==a.scene)choice=int(i);
 if(a.random)choice=randomChoiceFor(a);
 auto interval=std::find(randomIntervals.begin(),randomIntervals.end(),a.randomInterval);
 setComboSelection(randomIntervalId,int(interval-randomIntervals.begin()));EnableWindow(GetDlgItem(library,randomIntervalId),a.random);
 setComboSelection(visualizationId,choice);
 SendDlgItemMessageW(library,bodyStatisticsId,BM_SETCHECK,a.showBodyStatistics?BST_CHECKED:BST_UNCHECKED,0);
 EnableWindow(GetDlgItem(library,bodyStatisticsId),a.random||bodyFacts(a.scene)!=nullptr);
} 
// Both entry points share the catalog and keep imported video available to return to.
bool selectVisualization(int choice){
 {std::lock_guard lock(stateMutex);
  if(surfaces.empty()||choice<0||choice>=int(std::size(visualizations)))return false;
  auto& a=selected();auto scene=visualizations[choice].scene;
  if(scene==Scene::Video&&a.video.empty())return false;
  a.random=isRandomChoice(choice);
  if(a.random)a.randomPool=choice==solarRandomChoice?RandomPool::SolarSystem:RandomPool::All;
  if(!a.random)a.scene=scene;
  selectedSurface().pendingVideoSpeed.reset();selectedSurface().videoMessage.clear();
  auto& cycle=selectedSurface().randomCycle;cycle.reset();if(a.random)cycle.advance(a,0,true);
  requestSave();
 }
 syncControls();return true;
}
HMENU visualizationMenu(){
 HMENU menu=CreatePopupMenu();std::lock_guard lock(stateMutex);
 HMENU solar=CreatePopupMenu();
 auto appendChoice=[&](HMENU target,size_t i,const wchar_t* name){
  bool available=!surfaces.empty();
  auto scene=visualizations[i].scene;
  if(available&&scene==Scene::Video&&selected().video.empty())return;
  UINT flags=MF_OWNERDRAW|(available?0:MF_GRAYED);
  if(available&&(selected().random?int(i)==randomChoiceFor(selected()):(!isRandomChoice(int(i))&&selected().scene==scene)))flags|=MF_CHECKED;
  AppendMenuW(target,flags,visualizationMenuBase+i,name);
 };
 appendChoice(solar,solarRandomChoice,L"Random");
 AppendMenuW(solar,MF_SEPARATOR,0,nullptr);
 constexpr Scene solarOrder[]={Scene::Sun,Scene::Mercury,Scene::Venus,Scene::Earth,Scene::Moon,Scene::Mars,Scene::Jupiter,Scene::Saturn,Scene::Uranus,Scene::Neptune,Scene::Pluto};
 for(auto scene:solarOrder)for(size_t i=0;i<std::size(visualizations);i++)if(visualizations[i].scene==scene)appendChoice(solar,i,visualizations[i].name);
 for(size_t i=0;i<std::size(visualizations);i++){
  if(bodyFacts(visualizations[i].scene)||int(i)==solarRandomChoice)continue;
  appendChoice(menu,i,visualizations[i].name);
  if(visualizations[i].scene==Scene::Orbit){
   UINT flags=MF_POPUP|MF_OWNERDRAW;
   if(surfaces.empty())flags|=MF_GRAYED;
   else if(selected().random?selected().randomPool==RandomPool::SolarSystem:bodyFacts(selected().scene)!=nullptr)flags|=MF_CHECKED;
   AppendMenuW(menu,flags,reinterpret_cast<UINT_PTR>(solar),L"Solar System");
  }
 }
 MENUINFO info{sizeof(info),MIM_BACKGROUND|MIM_APPLYTOSUBMENUS};info.hbrBack=ui::surfaceBrush;SetMenuInfo(menu,&info);return menu;

}
void advanceRandomVisualizations(uint64_t elapsed){
 bool changed=false;
 {std::lock_guard lock(stateMutex);
  for(auto& surface:surfaces){
   auto entry=std::find_if(settings.displays.begin(),settings.displays.end(),[&](const auto& item){return item.first==surface.monitor.id;});
   if(entry==settings.displays.end())continue;
   auto pause=pauseState;pause.covered=surface.covered;
   changed|=surface.randomCycle.advance(entry->second,elapsed,IsWindow(surface.hwnd)&&!surface.renderingFailed&&!shouldPause(pause,settings.pauseBattery));
  }
 }
 if(changed)syncControls();
}
// Respect each monitor's work area, including a taskbar that is auto-hidden.
std::array<unsigned,4> desktopOverlayInsets(HMONITOR monitor){
 MONITORINFO info{sizeof(info)};if(!GetMonitorInfoW(monitor,&info))return {};
 std::array<unsigned,4> insets{unsigned(std::max(0L,info.rcWork.left-info.rcMonitor.left)),unsigned(std::max(0L,info.rcWork.top-info.rcMonitor.top)),unsigned(std::max(0L,info.rcMonitor.right-info.rcWork.right)),unsigned(std::max(0L,info.rcMonitor.bottom-info.rcWork.bottom))};
 const UINT edges[]={ABE_LEFT,ABE_TOP,ABE_RIGHT,ABE_BOTTOM};
 for(unsigned i=0;i<4;i++){
  APPBARDATA bar{sizeof(bar)};bar.uEdge=edges[i];bar.rc=info.rcMonitor;
  auto taskbar=reinterpret_cast<HWND>(SHAppBarMessage(ABM_GETAUTOHIDEBAREX,&bar));RECT bounds{};
  if(taskbar&&GetWindowRect(taskbar,&bounds)){
   unsigned thickness=unsigned(std::max(0L,(i==0||i==2)?bounds.right-bounds.left:bounds.bottom-bounds.top));
   if(thickness<=3)thickness=MulDiv(48,GetDpiForWindow(taskbar),96);
   insets[i]=std::max(insets[i],thickness);
  }
 }
 return insets;
}
void startRendering(){
 renderingProblem=false;renderSetupFailed=false;auto generation=++renderGeneration;
 renderThread=std::jthread([generation](std::stop_token stop){
  CoInitializeEx(nullptr,COINIT_MULTITHREADED);
  try{
   struct Group{std::shared_ptr<GpuContext> context;std::vector<size_t> indices;};std::vector<Group> groups;
   for(size_t i=0;i<surfaces.size();i++){
    auto context=GpuContext::acquire(surfaces[i].monitor.adapter);
    auto group=std::find_if(groups.begin(),groups.end(),[&](const auto& g){return g.context==context;});
    if(group==groups.end())groups.push_back({context,{i}});else group->indices.push_back(i);
   }
   std::vector<std::jthread> workers;
   for(auto& group:groups)workers.emplace_back([generation,indices=group.indices,context=group.context](std::stop_token token)mutable{
    CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    HANDLE wakeTimer=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS);
    {
     std::vector<std::unique_ptr<Renderer>> renderers(indices.size());std::vector<PerformanceState> performance(indices.size());
     std::vector<double> clocks(indices.size(),0),previous(indices.size(),0),retryAt(indices.size(),0),insetAt(indices.size(),0),outputAt(indices.size(),0);
     std::vector<OutputConfig> outputConfigs(indices.size());std::vector<OutputPreference> outputPreferences(indices.size(),OutputPreference::Auto);std::vector<bool> outputVideo(indices.size(),false);std::vector<unsigned> outputSeen(indices.size(),0);
     double statusAt=0;
     std::vector<Scene> scenes(indices.size(),Scene::Off),failedScenes(indices.size(),Scene::Off);std::vector<std::wstring> failedVideos(indices.size());
     const auto ownedAdapter=context->luid;
     AdapterPerformance adapter;bool reported=false;
     auto seconds=[](){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();};
     adapter.lastEvaluation=seconds();
     auto publish=[&](size_t index,bool failed,const std::wstring& errorText){
      bool any;{std::lock_guard lock(stateMutex);surfaces[index].renderingFailed=failed;any=std::any_of(surfaces.begin(),surfaces.end(),[](const auto& s){return s.renderingFailed;});if(failed)lastError=errorText;else if(!any)lastError.clear();}
      if(any!=reported){reported=any;PostMessageW(controller,renderStatusMessage,any,generation);}
     };
     while(!token.stop_requested()){
      for(size_t n=0;n<indices.size();n++){
       const size_t i=indices[n];auto now=seconds();Assignment a;PauseState pause;Monitor monitor;HWND window;bool preview,hadFailure;
       {std::lock_guard lock(stateMutex);a=assignmentFor(settings,surfaces[i].monitor.id);if(surfaces[i].pendingVideoSpeed)a.videoSettings.speed=*surfaces[i].pendingVideoSpeed;pause=pauseState;pause.covered=surfaces[i].covered;pause.battery=settings.pauseBattery&&pause.battery;monitor=surfaces[i].monitor;window=surfaces[i].hwnd;preview=previewMode;hadFailure=surfaces[i].renderingFailed;}
       auto& perf=performance[n];bool changed=perf.configure(a.performance,monitor.refreshHz,a.scene,now);
       if(retryAt[n]&&(a.scene!=failedScenes[n]||a.video!=failedVideos[n]))retryAt[n]=0;
       bool active=a.scene!=Scene::Off&&IsWindow(window)&&!shouldPause(pause,true)&&retryAt[n]<=now;bool wasActive=perf.active;perf.setActive(active,now);
       if(scenes[n]!=a.scene){clocks[n]=0;scenes[n]=a.scene;previous[n]=now;}if(active&&!wasActive)previous[n]=now;
       if(a.scene==Scene::Off){renderers[n].reset();if(hadFailure)publish(i,false,{});}
       if(!active){if(renderers[n]){try{renderers[n]->setPaused(true);}catch(...){renderers[n].reset();}}previous[n]=now;continue;}
       if(changed)previous[n]=now;
       if(!perf.deadline.due(now))continue;
       try{
        if(!context){context=GpuContext::acquire(ownedAdapter,false,false);adapter={now};for(auto& p:performance)p.restart(now);}
        RECT rect{};GetClientRect(window,&rect);
        if(!renderers[n]){renderers[n]=std::make_unique<Renderer>(window,std::max(1L,rect.right),std::max(1L,rect.bottom),exeDir/L"shaders",false,false,context);renderers[n]->setNonblocking(true);perf.restart(seconds());previous[n]=seconds();}
        auto& renderer=*renderers[n];
        auto revision=outputRevision.load();bool outputEvent=revision!=outputSeen[n];
        if(outputEvent||now>=outputAt[n]||outputPreferences[n]!=a.output||outputVideo[n]!=(a.scene==Scene::Video)){
         outputConfigs[n]=discoverOutput(monitor,a.output,a.scene==Scene::Video);outputAt[n]=now+1;outputPreferences[n]=a.output;outputVideo[n]=a.scene==Scene::Video;outputSeen[n]=revision;
        }
        if(renderer.configureOutput(outputConfigs[n],outputEvent)){perf.restart(seconds());previous[n]=seconds();}
        if(!renderer.ready())continue;
        if(!preview&&now>=insetAt[n]){renderer.setOverlayInsets(desktopOverlayInsets(monitor.handle));insetAt[n]=now+1;}
        renderer.setPaused(a.scene!=Scene::Video);renderer.resize(std::max(1L,rect.right),std::max(1L,rect.bottom));
        if(a.scene==Scene::Smoke)renderer.setGrid(perf.detail==0?64:perf.detail==1?80:96);
        double elapsed=previous[n]>0?std::max(0.,now-previous[n]):0;previous[n]=now;clocks[n]+=elapsed;
        auto features=audio.features();FrameData data;data.time=float(clocks[n]);data.dt=boundedDelta(float(elapsed));data.brightness=a.params.brightness;data.movement=a.params.movement;data.detail=a.params.detail;data.flow=a.params.flow;data.turbulence=a.params.turbulence;data.density=a.params.density;data.palette=float(a.params.palette);data.reaction=a.reaction==Reaction::Off?0:(a.reaction==Reaction::Gentle?.35f:1.f)*a.params.strength;data.bass=features.bass;data.mid=features.mid;data.treble=features.treble;data.energy=features.energy;data.impulse=features.transient;if(isProceduralWorld(a.scene)||a.scene==Scene::Sun)data.spare[0]=float(perf.detail);
        bool missed=perf.deadline.next>0&&now-perf.deadline.next>.25/perf.fps();
        renderer.render(a,data);auto after=seconds();perf.sample(after,renderer.gpuMs(),missed);perf.deadline.submitted(after,perf.fps());
        if(after-now>.25){perf.restart(after);previous[n]=after;} // Ignore uploads and expensive resource changes.
        bool recoveredFailure;{std::lock_guard lock(stateMutex);++surfaces[i].renderedFrames;
         if(a.scene==Scene::Video&&renderer.videoRateKnown()){
          for(auto& [id,live]:settings.displays)if(id==monitor.id&&live.scene==Scene::Video&&live.video==a.video){
           auto& pending=surfaces[i].pendingVideoSpeed;bool matches=pending?std::abs(*pending-a.videoSettings.speed)<1e-6:std::abs(live.videoSettings.speed-a.videoSettings.speed)<1e-6;
           if(matches){bool speedChanged=live.videoSettings.speed!=renderer.videoPlaybackRate();live.videoSettings.speed=renderer.videoPlaybackRate();pending.reset();surfaces[i].videoMessage=renderer.videoRateMessage();if(speedChanged)requestSave();}
          }
         }
         recoveredFailure=surfaces[i].renderingFailed;}
        if(recoveredFailure)publish(i,false,{});retryAt[n]=0;
       }catch(const std::exception& error){
        bool removed=context&&context->removed();renderers[n].reset();retryAt[n]=now+5;failedScenes[n]=a.scene;failedVideos[n]=a.video;perf.restart(now);publish(i,true,wide(error.what()));
        if(removed){for(size_t k=0;k<renderers.size();k++){renderers[k].reset();retryAt[k]=now+5;failedScenes[k]=performance[k].scene;performance[k].restart(now);publish(indices[k],true,L"Graphics device unavailable. Retrying automatically.");}context.reset();}
       }
      }
      auto now=seconds();std::vector<PerformanceState*> inputs;for(auto& p:performance)inputs.push_back(&p);adapter.evaluate(now,inputs);
      if(now>=statusAt){statusAt=now+1;std::lock_guard lock(stateMutex);for(size_t n=0;n<indices.size();n++){
       auto& p=performance[n];std::wostringstream summary;summary<<std::fixed<<std::setprecision(2)<<p.fps()<<L" FPS · "<<(p.detail==2?L"High":p.detail==1?L"Balanced":L"Eco")<<L" · Native resolution";
       if(renderers[n]){auto& out=renderers[n]->output();summary<<(out.hdr?L" · HDR":L" · SDR");if(!out.reason.empty())summary<<L" ("<<out.reason<<L")";if(out.hdr&&(out.whiteFallback||out.peakFallback))summary<<L" (estimated luminance)";}
       if(!p.active)summary<<L" · Paused";
       surfaces[indices[n]].outputFallback=renderers[n]&&renderers[n]->output().reason==L"HDR presentation unavailable";
       surfaces[indices[n]].performanceStatus=summary.str();surfaces[indices[n]].effectiveFps=p.fps();
      }}
      // High-resolution waits keep fractional deadlines accurate without a
      // process-wide timer-resolution change. Paused adapters sleep longer.
      double delay=.05;for(auto& p:performance)if(p.active)delay=std::min(delay,std::max(.001,p.deadline.next-seconds()));
      LARGE_INTEGER due{};due.QuadPart=-LONGLONG(delay*10000000);
      if(wakeTimer&&SetWaitableTimer(wakeTimer,&due,0,nullptr,nullptr,FALSE))WaitForSingleObject(wakeTimer,60);else std::this_thread::sleep_for(std::chrono::milliseconds(1));
     }
    }
    if(wakeTimer)CloseHandle(wakeTimer);CoUninitialize();
   });
   groups.clear(); // Only the adapter worker may retain its current device.
   while(!stop.stop_requested()){
    bool listen=false;std::wstring device;
    {std::lock_guard lock(stateMutex);device=settings.audioDevice;for(const auto& surface:surfaces){auto a=assignmentFor(settings,surface.monitor.id);auto pause=pauseState;pause.covered=surface.covered;listen|=a.scene!=Scene::Off&&a.scene!=Scene::Video&&a.reaction!=Reaction::Off&&!shouldPause(pause,settings.pauseBattery);}}
    audio.configure(listen,device);std::this_thread::sleep_for(std::chrono::milliseconds(20));
   }
   for(auto& worker:workers)worker.request_stop();for(auto& worker:workers)worker.join();
  }catch(const std::exception& e){{std::lock_guard lock(stateMutex);renderSetupFailed=true;lastError=wide(e.what());}PostMessageW(controller,renderStatusMessage,TRUE,generation);}
  audio.configure(false,{});CoUninitialize();
 });
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
void revealDesktopForSettings(){
 // Minimize app windows explicitly instead of toggling Show Desktop, which can
 // restore previously minimized apps and also hide the Worlds interface.
 EnumWindows([](HWND window,LPARAM)->BOOL{
  if(window==library||window==controller||window==GetShellWindow()||window==GetDesktopWindow()||!IsWindowVisible(window)||IsIconic(window))return TRUE;
  for(const auto& surface:surfaces)if(window==surface.hwnd)return TRUE;
  wchar_t name[64]{};GetClassNameW(window,name,64);
  if(!wcscmp(name,L"Progman")||!wcscmp(name,L"WorkerW")||!wcscmp(name,L"Shell_TrayWnd")||!wcscmp(name,L"Shell_SecondaryTrayWnd"))return TRUE;
  DWORD cloaked=0;DwmGetWindowAttribute(window,DWMWA_CLOAKED,&cloaked,sizeof(cloaked));if(cloaked)return TRUE;
  // Do not turn desktop helpers, menus or notification windows into apps.
  auto style=GetWindowLongPtrW(window,GWL_EXSTYLE);
  if(style&(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE))return TRUE;
  ShowWindowAsync(window,SW_SHOWMINNOACTIVE);return TRUE;
 },0);
 ShowWindow(library,SW_SHOW);SetForegroundWindow(library);
}
void importWorld(const std::filesystem::path&,bool);
PresetUiContext presetContext(){
 PresetUiContext context;context.profile=profile;
 context.importFile=[](const std::filesystem::path& file){importWorld(file,false);};
 context.monitors=[] {std::lock_guard lock(stateMutex);std::vector<Monitor> monitors;for(auto& surface:surfaces)monitors.push_back(surface.monitor);return monitors;};
 context.snapshot=[] {std::lock_guard lock(stateMutex);std::vector<Monitor> monitors;for(auto& surface:surfaces)monitors.push_back(surface.monitor);return snapshotPreset(settings,monitors);};
 context.apply=[](const DesktopPreset& preset,const std::vector<int>& mapping,const std::vector<Monitor>& monitors){
  auto devices=AudioCapture::devices();
  {std::lock_guard lock(stateMutex);auto next=settings;next.pauseBattery=preset.pauseBattery;next.audioDevice=preset.audioDevice;
   if(!next.audioDevice.empty()&&std::none_of(devices.begin(),devices.end(),[&](auto& d){return d.first==next.audioDevice;}))next.audioDevice.clear();
   if(mapping.size()!=preset.displays.size())throw std::runtime_error("Invalid preset display mapping.");std::set<int> used;
   for(size_t i=0;i<mapping.size();i++){int target=mapping[i];if(target<0)continue;if(size_t(target)>=monitors.size()||!used.insert(target).second)throw std::runtime_error("Invalid preset destination.");auto& monitor=monitors[target];if(std::none_of(surfaces.begin(),surfaces.end(),[&](auto& surface){return surface.monitor.id==monitor.id&&surface.monitor.adapter.HighPart==monitor.adapter.HighPart&&surface.monitor.adapter.LowPart==monitor.adapter.LowPart;}))throw std::runtime_error("Displays changed. Review the preset mapping again.");
    auto value=preset.displays[i].assignment;bool found=false;for(auto& entry:next.displays)if(entry.first==monitor.id){entry.second=value;found=true;break;}if(!found)next.displays.emplace_back(monitor.id,value);
   }
   if(used.empty())throw std::runtime_error("Choose at least one destination.");settings=std::move(next);
   for(auto target:used)for(auto& surface:surfaces)if(surface.monitor.id==monitors[target].id){auto value=assignmentFor(settings,surface.monitor.id);surface.randomCycle.reset(true,value.randomPool);surface.pendingVideoSpeed.reset();surface.videoMessage.clear();}
   requestSave();
  }syncControls();
 };return context;
}
void importWorld(const std::filesystem::path& file,bool personalVideo=false){
 if(!personalVideo&&isDesktopPackage(file)){showLibrary();showSavedPresets(library,presetContext(),file);return;}
 showLibrary();auto displays=enumerateMonitors();if(displays.empty())throw std::runtime_error("Connect a display before applying a World.");
 int index=int(SendDlgItemMessageW(library,monitorId,CB_GETCURSEL,0,0));
 Monitor target=(!surfaces.empty()&&index>=0&&index<int(surfaces.size()))?surfaces[index].monitor:displays.front();
 showWorldImport(controller,file,profile/L"library",target.name,[id=target.id](const Assignment& imported){
  {std::lock_guard lock(stateMutex);bool found=false;for(auto& [key,value]:settings.displays)if(key==id){value=importForDisplay(imported,value);for(auto& surface:surfaces)if(surface.monitor.id==id){surface.pendingVideoSpeed.reset();surface.videoMessage.clear();}found=true;break;}
   if(!found)settings.displays.emplace_back(id,imported);requestSave();}
  syncControls();
 },personalVideo,target.adapter);
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
 if(msg==WM_NOTIFY){auto custom=reinterpret_cast<NMCUSTOMDRAW*>(l);if((custom->hdr.idFrom==batteryId||custom->hdr.idFrom==bodyStatisticsId)&&custom->hdr.code==NM_CUSTOMDRAW&&custom->dwDrawStage==CDDS_PREPAINT){
  auto dc=custom->hdc;RECT r=custom->rc;FillRect(dc,&r,ui::backgroundBrush);RECT check{r.left+px(2),r.top+px(5),r.left+px(20),r.top+px(23)};
  auto pen=CreatePen(PS_SOLID,px(1),ui::accent);auto oldPen=SelectObject(dc,pen);auto oldBrush=SelectObject(dc,ui::surfaceBrush);Rectangle(dc,check.left,check.top,check.right,check.bottom);
  if(SendMessageW(custom->hdr.hwndFrom,BM_GETCHECK,0,0)==BST_CHECKED){MoveToEx(dc,check.left+px(3),check.top+px(9),nullptr);LineTo(dc,check.left+px(7),check.top+px(13));LineTo(dc,check.left+px(15),check.top+px(4));}
  SelectObject(dc,oldPen);SelectObject(dc,oldBrush);DeleteObject(pen);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,ui::foreground);auto oldFont=SelectObject(dc,font);r.left+=px(30);wchar_t caption[96]{};GetWindowTextW(custom->hdr.hwndFrom,caption,96);DrawTextW(dc,caption,-1,&r,DT_SINGLELINE|DT_VCENTER);SelectObject(dc,oldFont);
  if((custom->uItemState&CDIS_FOCUS)&&!(custom->uItemState&CDIS_DISABLED)&&!(SendMessageW(custom->hdr.hwndFrom,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS)){InflateRect(&check,px(3),px(3));ui::frame(dc,check,ui::accent);}return CDRF_SKIPDEFAULT;
 }}
 if(msg==WM_MEASUREITEM){auto d=reinterpret_cast<MEASUREITEMSTRUCT*>(l);d->itemHeight=px(26);return TRUE;}
 if(msg==WM_DRAWITEM){ui::draw(*reinterpret_cast<DRAWITEMSTRUCT*>(l),font,false,uiDpi);return TRUE;}

 if(msg==WM_COMMAND){int id=LOWORD(w),event=HIWORD(w);
  // Focus/open/close notifications must not reset a combo while its mouse
  // selection is in progress. Only committed selection changes update state.
  bool dropdown=id==videoSpeedId||id==outputId||id==randomIntervalId||id==visualizationId||id==monitorId||id==reactionId||id==qualityId||id==fpsId||id==paletteId||id==deviceId||id==videoFitId;
  if(dropdown&&event!=CBN_SELCHANGE)return 0;
  if(!dropdown&&event!=BN_CLICKED)return 0;
  if(id==advancedId){
   if(!advancedOpen)revealDesktopForSettings();
   SendMessageW(hwnd,WM_SETREDRAW,FALSE,0);advancedOpen=!advancedOpen;
   // Reveal the live World while tuning, and restore the ordinary opaque library on collapse.
   auto style=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);
   SetWindowLongPtrW(hwnd,GWL_EXSTYLE,advancedOpen?style|WS_EX_LAYERED:style&~LONG_PTR(WS_EX_LAYERED));
   if(advancedOpen)SetLayeredWindowAttributes(hwnd,0,179,LWA_ALPHA);
   SetWindowTextW(GetDlgItem(hwnd,advancedId),advancedOpen?L"−  Adjustments":L"+  Adjustments");scrollOffset=0;SendMessageW(hwnd,WM_SETREDRAW,TRUE,0);resizeLibrary();return 0;
  }
  if(id==resetId){
   {std::lock_guard lock(stateMutex);if(!surfaces.empty()){auto& a=selected();a.params=Parameters{};a.reaction=Reaction::Off;a.fit=false;a.videoSettings={};selectedSurface().pendingVideoSpeed.reset();selectedSurface().videoMessage.clear();a.showBodyStatistics=false;a.performance={};a.output=OutputPreference::Auto;}Settings defaults;settings.pauseBattery=defaults.pauseBattery;settings.audioDevice.clear();requestSave();}
   syncControls();return 0;
  }
  if(id==presetsId){showSavedPresets(hwnd,presetContext());return 0;}
  if(id==videoSpeedId){std::lock_guard lock(stateMutex);int value=int(SendDlgItemMessageW(hwnd,id,CB_GETCURSEL,0,0));if(value>=0&&value<int(videoSpeeds.size())){selectedSurface().pendingVideoSpeed=videoSpeeds[value];selectedSurface().videoMessage.clear();}return 0;}
  if(id==videoCenterId){{std::lock_guard lock(stateMutex);auto& a=selected();a.videoSettings.positionX=a.videoSettings.positionY=.5;requestSave();}syncControls();return 0;}
  if(id==videoImportId){
   wchar_t file[32768]{};OPENFILENAMEW dialog{sizeof(dialog)};dialog.hwndOwner=hwnd;dialog.lpstrFilter=L"Video files\0*.mp4;*.m4v;*.mov;*.mkv;*.webm;*.wmv;*.avi\0All files\0*.*\0";dialog.lpstrFile=file;dialog.nMaxFile=32768;dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;dialog.lpstrTitle=L"Import video";
   if(GetOpenFileNameW(&dialog))importWorld(file,true);return 0;
  }
  if(id==visualizationId){selectVisualization(int(SendDlgItemMessageW(hwnd,id,CB_GETCURSEL,0,0)));syncControls();return 0;}
  if(id==monitorId&&event==CBN_SELCHANGE){syncControls();return 0;}
  if(id==communityId){DestroyWindow(controller);return 0;}
  if(id==retryId){{std::lock_guard lock(stateMutex);previewMode=false;}createSurfaces();syncControls();return 0;}
  if(id==pauseId){{std::lock_guard lock(stateMutex);pauseState.manual=!pauseState.manual;}syncControls();return 0;}
  {std::lock_guard lock(stateMutex);if(surfaces.empty())return 0;auto& a=selected();
   if(id==applyAllId){auto copy=a;for(auto& s:surfaces){bool found=false;for(auto& [key,entry]:settings.displays)if(key==s.monitor.id){entry=copyForDisplay(copy,entry,s.monitor.refreshHz);found=true;break;}if(!found)settings.displays.emplace_back(s.monitor.id,copyForDisplay(copy,Assignment{},s.monitor.refreshHz));s.randomCycle.reset(true,copy.randomPool);}}
   else if(event==CBN_SELCHANGE){int value=int(SendDlgItemMessageW(hwnd,id,CB_GETCURSEL,0,0));if(id==randomIntervalId&&value>=0&&value<int(randomIntervals.size())){a.randomInterval=randomIntervals[value];selectedSurface().randomCycle.reset(true,a.randomPool);}if(id==reactionId)a.reaction=Reaction(value);if(id==outputId)a.output=OutputPreference(value);if(id==qualityId)a.performance.quality=Quality(value);if(id==fpsId)a.performance.frameLimit=FrameLimit(value);if(id==paletteId)a.params.palette=value;if(id==videoFitId)a.fit=value==1;if(id==deviceId)settings.audioDevice=value>0?audioDevices.at(value-1).first:L"";}
   else if(id==bodyStatisticsId)a.showBodyStatistics=SendDlgItemMessageW(hwnd,id,BM_GETCHECK,0,0)==BST_CHECKED;
   else if(id==batteryId)settings.pauseBattery=SendDlgItemMessageW(hwnd,id,BM_GETCHECK,0,0)==BST_CHECKED;
   requestSave();}
  syncControls();return 0;
 }
 if(msg==WM_HSCROLL&&(GetDlgCtrlID((HWND)l)==videoXId||GetDlgCtrlID((HWND)l)==videoYId)){std::lock_guard lock(stateMutex);auto& a=selected();double value=double(SendMessageW((HWND)l,TBM_GETPOS,0,0))/100;if(GetDlgCtrlID((HWND)l)==videoXId)a.videoSettings.positionX=value;else a.videoSettings.positionY=value;requestSave();return 0;}
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
 control(L"BUTTON",L"Show body statistics",BS_AUTOCHECKBOX|WS_TABSTOP,155,190,345,28,bodyStatisticsId);
 label(L"Audio reaction",30,232,120);combo(reactionId,155,226,170,{L"Off",L"Gentle",L"Expressive"});label(L"Palette",355,232,80);combo(paletteId,435,226,285,{L"Teal / violet / gold",L"Emerald / blue",L"Ember / gold",L"Silver"});
 control(L"BUTTON",L"Import video…",WS_TABSTOP,30,273,165,34,videoImportId);control(L"BUTTON",L"Presets…",WS_TABSTOP,205,273,165,34,presetsId);
 control(L"BUTTON",L"+  Adjustments",WS_TABSTOP,385,273,335,34,advancedId);
 addingAdvanced=true;
 label(L"Fine-tune this World",30,327,440);
 control(L"BUTTON",L"Restore defaults",WS_TABSTOP,500,321,220,34,resetId);
 const wchar_t* names[]={L"Brightness",L"Movement",L"Detail",L"Flow",L"Turbulence",L"Density",L"Audio strength"};for(int i=0;i<7;i++){int column=i%2,row=i/2;control(L"STATIC",names[i],0,30+column*350,367+row*44,115,24,210+i);HWND slider=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS|WS_TABSTOP,145+column*350,360+row*44,220,32,200+i);SendMessageW(slider,TBM_SETRANGE,TRUE,MAKELPARAM(0,i==6?100:200));}
 label(L"Video position",380,499,115);combo(videoFitId,495,493,220,{L"Fill",L"Fit"});
 control(L"STATIC",L"Horizontal position",0,30,546,320,24,240);control(L"STATIC",L"Vertical position",0,380,546,320,24,241);
 for(int id:{videoXId,videoYId}){auto slider=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS|WS_TABSTOP,id==videoXId?30:380,574,335,28,id);SendMessageW(slider,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));}
 control(L"STATIC",L"Video speed",0,30,621,110,24,242);combo(videoSpeedId,145,615,220,{L"0.25x",L"0.5x",L"0.75x",L"1x",L"1.25x",L"1.5x",L"2x"});control(L"BUTTON",L"Center",WS_TABSTOP,495,615,220,32,videoCenterId);
 label(L"Quality",30,546,110);combo(qualityId,145,540,220,{L"Eco",L"Auto",L"High"});label(L"Frame limit",380,546,115);combo(fpsId,495,540,220,{L"Auto",L"30 FPS",L"60 FPS"});
 label(L"Audio output",30,591,115);combo(deviceId,145,585,570,{L"Follow system output"});audioDevices=AudioCapture::devices();for(auto& d:audioDevices)SendDlgItemMessageW(library,deviceId,CB_ADDSTRING,0,(LPARAM)d.second.c_str());
 label(L"Output",380,637,110);combo(outputId,495,631,220,{L"Auto",L"SDR"});
 control(L"BUTTON",L"Pause on battery",BS_AUTOCHECKBOX|WS_TABSTOP,30,631,230,28,batteryId);
 control(L"STATIC",L"Performance will appear during playback.",SS_NOPREFIX,30,668,690,48,performanceStatusId);
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
 case renderStatusMessage:if(unsigned(l)==renderGeneration){{std::lock_guard lock(stateMutex);renderingProblem=renderSetupFailed||std::any_of(surfaces.begin(),surfaces.end(),[](const auto& s){return s.renderingFailed;});}syncControls();}return 0;
 case WM_MEASUREITEM:{auto d=reinterpret_cast<MEASUREITEMSTRUCT*>(l);d->itemWidth=MulDiv(230,menuDpi,96);d->itemHeight=MulDiv(36,menuDpi,96);return TRUE;}
 case WM_DRAWITEM:ui::draw(*reinterpret_cast<DRAWITEMSTRUCT*>(l),menuFont?menuFont:font,false,menuDpi);return TRUE;
 case WM_SETTINGCHANGE:ui::refresh(library);return 0;
 case WM_TIMER:{
  if(w==2){bool changed=false;bool promoted=ui::promoteTray(executable().wstring(),changed);if(promoted||++promotionAttempts>=20)KillTimer(hwnd,2);if(changed){NOTIFYICONDATAW n{sizeof(n)};n.hWnd=hwnd;n.uID=1;Shell_NotifyIconW(NIM_DELETE,&n);addTray(false);}return 0;}

  if(automaticUpdates&&!sessionEnding&&GetTickCount64()>=nextUpdateCheck){nextUpdateCheck=GetTickCount64()+3600000;updates.check();}
  bool reconnect=renderSetupFailed;
  if(!previewMode)reconnect|=surfaces.empty()||std::any_of(surfaces.begin(),surfaces.end(),[](const Surface& s){return !wallpaperAttached(s.hwnd);});
  if(GetTickCount64()>=nextDesktopRetry&&reconnect)createSurfaces();
  static ULONGLONG previousRandomTick=GetTickCount64();auto now=GetTickCount64();auto elapsed=now-previousRandomTick;previousRandomTick=now;
  SYSTEM_POWER_STATUS power{};GetSystemPowerStatus(&power);std::vector<bool> covered;for(auto& s:surfaces)covered.push_back(!previewMode&&monitorCovered(s.monitor,GetCurrentProcessId()));{std::lock_guard lock(stateMutex);pauseState.battery=power.ACLineStatus==0;pauseState.remote=GetSystemMetrics(SM_REMOTESESSION)!=0;for(size_t i=0;i<surfaces.size();i++)surfaces[i].covered=covered[i];}advanceRandomVisualizations(elapsed);syncControls();}return 0;
 case WM_WTSSESSION_CHANGE:{std::lock_guard lock(stateMutex);if(w==WTS_SESSION_LOCK)pauseState.locked=true;if(w==WTS_SESSION_UNLOCK)pauseState.locked=false;}return 0;
 case WM_POWERBROADCAST:{std::lock_guard lock(stateMutex);if(w==PBT_APMSUSPEND)pauseState.suspended=true;if(w==PBT_APMRESUMEAUTOMATIC||w==PBT_APMRESUMESUSPEND){pauseState.suspended=false;++outputRevision;}if(w==PBT_POWERSETTINGCHANGE){auto p=reinterpret_cast<POWERBROADCAST_SETTING*>(l);if(p->PowerSetting==GUID_CONSOLE_DISPLAY_STATE&&p->DataLength==sizeof(DWORD)){DWORD state;memcpy(&state,p->Data,sizeof(state));pauseState.asleep=state==0;}}}return TRUE;
 case WM_DISPLAYCHANGE:{
  auto monitors=enumerateMonitors();bool same;
  {std::lock_guard lock(stateMutex);same=monitors.size()==surfaces.size();
   for(const auto& surface:surfaces)if(std::none_of(monitors.begin(),monitors.end(),[&](const Monitor& m){return sameOutputSurface(surface.monitor,m);}))same=false;
   if(same)for(auto& surface:surfaces)surface.monitor=*std::find_if(monitors.begin(),monitors.end(),[&](const Monitor& m){return m.id==surface.monitor.id;});
  }
  ++outputRevision;if(!same)createSurfaces();return 0;
 }
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
 {Renderer renderer(hwnd,width,height,exeDir/L"shaders",std::find(args.begin(),args.end(),L"--debug-gpu")!=args.end(),std::find(args.begin(),args.end(),L"--single-sample")!=args.end());renderer.setGrid(std::stoul(option(L"--grid",L"64")));Assignment a;a.scene=scene;a.showBodyStatistics=std::find(args.begin(),args.end(),L"--show-body-statistics")!=args.end();if(scene==Scene::Video){a.video=option(L"--video-file",L"");if(a.video.empty())throw std::runtime_error("Video capture requires --video-file");a.fit=std::find(args.begin(),args.end(),L"--fit")!=args.end();a.videoSettings={std::stod(option(L"--video-x",L"0.5")),std::stod(option(L"--video-y",L"0.5")),std::stod(option(L"--video-speed",L"1"))};if(!validVideoSettings(a.videoSettings))throw std::runtime_error("Invalid diagnostic video settings");}bool diagnosticHdr=std::find(args.begin(),args.end(),L"--hdr-output")!=args.end();
 if(diagnosticHdr){auto nits=[&](const wchar_t* flag,const wchar_t* fallback){auto value=option(flag,fallback);size_t consumed=0;float n=std::stof(value,&consumed);if(consumed!=value.size())throw std::runtime_error("Invalid HDR diagnostic luminance");return n;};float w=nits(L"--hdr-white",L"203"),p=nits(L"--hdr-peak",L"400");if(!usableLuminance(w)||!usableLuminance(p)||w>p||scene==Scene::Video||record)throw std::runtime_error("Invalid HDR diagnostic configuration");auto cfg=resolveOutput(OutputPreference::Auto,false,true,w,p);renderer.configureOutput(cfg);if(!renderer.output().hdr)throw std::runtime_error("Diagnostic FP16 unavailable");}
 FrameData data;data.dt=1.f/60;std::vector<double> timings;auto started=std::chrono::steady_clock::now();
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
   const Scene lifecycleScenes[]={Scene::Aurora,Scene::Orbit,Scene::Mars,Scene::Earth,Scene::Moon,Scene::Jupiter,Scene::Saturn,Scene::Mercury,Scene::Venus,Scene::Neptune,Scene::Uranus,Scene::Pluto,Scene::Sun,Scene::Off,Scene::Orbit,Scene::Earth,Scene::Mars,Scene::Smoke,Scene::Bloom,Scene::Silk,Scene::Eclipse,Scene::Lattice,Scene::Drift,Scene::Contour,Scene::PlasticMetal,Scene::Ripples};
   for(size_t cycle=0;cycle<std::size(lifecycleScenes)*2;cycle++){
    a.scene=lifecycleScenes[cycle%std::size(lifecycleScenes)];renderer.resize(cycle%2?width:640,cycle%2?height:360);
    for(int frame=0;frame<8;frame++){data.time+=data.dt;renderer.render(a,data,false);}
    renderer.validateDebugMessages();
   }
   renderer.resize(width,height);a.scene=scene;
   if(scene==Scene::Mars||scene==Scene::Earth||scene==Scene::Moon||scene==Scene::Jupiter||(scene==Scene::Saturn||scene==Scene::Mercury||scene==Scene::Venus||scene==Scene::Neptune||(scene==Scene::Uranus||scene==Scene::Pluto||scene==Scene::Sun))){
    renderer.render(a,data,false);renderer.idle();auto marsBytes=renderer.allocations();
    for(int cycle=0;cycle<3;cycle++){
     a.scene=Scene::Orbit;renderer.render(a,data,false);a.scene=scene==Scene::Earth?Scene::Mars:Scene::Earth;renderer.render(a,data,false);a.scene=scene;renderer.render(a,data,false);
     renderer.resize(720,1280);renderer.render(a,data,false);renderer.resize(width,height);renderer.render(a,data,false);renderer.idle();
     if(renderer.allocations()!=marsBytes)throw std::runtime_error("Planet resources changed after space-scene/resize roundtrip");
     if(a.showBodyStatistics){
      a.showBodyStatistics=false;renderer.render(a,data,false);renderer.idle();
      if(renderer.allocations()>=marsBytes)throw std::runtime_error("Hidden body statistics retained GPU texture");
      a.showBodyStatistics=true;data.palette=float(cycle%4);renderer.render(a,data,false);renderer.render(a,data,false);renderer.idle();
      if(renderer.allocations()!=marsBytes)throw std::runtime_error("Body statistics resources changed after visibility/palette roundtrip");
     }
     renderer.validateDebugMessages();
    }
   }
  }
  auto startTimeOption=option(L"--start-time",L"0");size_t startConsumed{};float startTime=std::stof(startTimeOption,&startConsumed);
  if(startConsumed!=startTimeOption.size()||!std::isfinite(startTime)||startTime<0||startTime>86400||(startTime!=0&&scene!=Scene::Orbit&&scene!=Scene::Mars&&scene!=Scene::Earth&&scene!=Scene::Moon&&scene!=Scene::Jupiter&&(scene!=Scene::Saturn&&scene!=Scene::Mercury&&scene!=Scene::Venus&&scene!=Scene::Neptune&&(scene!=Scene::Uranus&&scene!=Scene::Pluto&&scene!=Scene::Sun))&&scene!=Scene::Aurora&&scene!=Scene::Bloom&&!isProceduralWorld(scene)))throw std::runtime_error("Capture start time requires a procedural World and a finite value from 0 to 86400 seconds");
  // Exercise the actual shader controls in native capture regressions.
  auto captureParameter=[&](const wchar_t* key){auto value=option(key,L"1");size_t consumed{};float number=std::stof(value,&consumed);if(consumed!=value.size()||!std::isfinite(number)||number<0||number>2)throw std::runtime_error("Capture parameter must be finite and between 0 and 2");return number;};
  data.density=captureParameter(L"--density");data.movement=captureParameter(L"--movement");data.brightness=captureParameter(L"--brightness");data.detail=captureParameter(L"--detail");data.flow=captureParameter(L"--flow");data.turbulence=captureParameter(L"--turbulence");
  auto paletteOption=option(L"--palette",L"0");size_t paletteConsumed{};int palette=std::stoi(paletteOption,&paletteConsumed);if(paletteConsumed!=paletteOption.size()||palette<0||palette>3)throw std::runtime_error("Capture palette must be 0 to 3");data.palette=float(palette);
  auto qualityOption=option(L"--quality",L"auto");int quality=qualityOption==L"eco"?0:qualityOption==L"auto"?1:qualityOption==L"high"?2:-1;if(quality<0)throw std::runtime_error("Capture quality must be eco, auto or high");if(isProceduralWorld(scene)||scene==Scene::Sun)data.spare[0]=float(quality);
  auto rippleProbe=option(L"--ripples-probe",L"0");size_t rippleConsumed{};int rippleMode=std::stoi(rippleProbe,&rippleConsumed);
  if(rippleConsumed!=rippleProbe.size()||rippleMode<0||rippleMode>6||(rippleMode&&scene!=Scene::Ripples))throw std::runtime_error("Ripples probe requires the ripples scene and a mode from 0 to 6");
  if(scene==Scene::Ripples)data.spare[1]=float(rippleMode);
  auto saturnProbe=option(L"--saturn-probe",L"0");size_t saturnConsumed{};int saturnMode=std::stoi(saturnProbe,&saturnConsumed);
  if(saturnConsumed!=saturnProbe.size()||saturnMode<0||saturnMode>6||(saturnMode&&scene!=Scene::Saturn))throw std::runtime_error("Saturn probe requires Saturn and a mode from 0 to 6");
  auto saturnElevation=option(L"--saturn-elevation",L"25");size_t elevationConsumed{};float elevation=std::stof(saturnElevation,&elevationConsumed);
  if(elevationConsumed!=saturnElevation.size()||!std::isfinite(elevation)||elevation<0||elevation>90)throw std::runtime_error("Saturn elevation must be from 0 to 90 degrees");
  if(scene==Scene::Saturn){data.spare[4]=float(saturnMode);data.spare[5]=elevation;data.spare[6]=1;}
  auto uranusProbe=option(L"--uranus-probe",L"0");size_t uranusConsumed{};int uranusMode=std::stoi(uranusProbe,&uranusConsumed);
  if(uranusConsumed!=uranusProbe.size()||uranusMode<0||uranusMode>3||(uranusMode&&scene!=Scene::Uranus))throw std::runtime_error("Uranus probe requires Uranus and a mode from 0 to 3");
  if(scene==Scene::Uranus)data.spare[4]=float(uranusMode);
  auto jupiterProbe=option(L"--jupiter-probe",L"0");size_t jupiterConsumed{};int jupiterMode=std::stoi(jupiterProbe,&jupiterConsumed);
  if(jupiterConsumed!=jupiterProbe.size()||jupiterMode<0||jupiterMode>4||(jupiterMode&&scene!=Scene::Jupiter))throw std::runtime_error("Jupiter probe requires Jupiter and a mode from 0 to 4");
  if(scene==Scene::Jupiter)data.spare[4]=float(jupiterMode);
  bool jupiterFixed=std::find(args.begin(),args.end(),L"--jupiter-fixed-globe")!=args.end();
  if(jupiterFixed&&scene!=Scene::Jupiter)throw std::runtime_error("Fixed Jupiter globe requires Jupiter");
  if(jupiterFixed)data.spare[6]=1;

  auto plutoProbe=option(L"--pluto-probe",L"0");size_t plutoConsumed{};int plutoMode=std::stoi(plutoProbe,&plutoConsumed);
  if(plutoConsumed!=plutoProbe.size()||plutoMode<0||plutoMode>3||(plutoMode&&scene!=Scene::Pluto))throw std::runtime_error("Pluto probe requires Pluto and a mode from 0 to 3");
  if(scene==Scene::Pluto)data.spare[4]=float(plutoMode);
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
   if(scene!=Scene::Smoke&&scene!=Scene::Jupiter)throw std::runtime_error("Paused-frame diagnostic requires Smoke or Jupiter");
   FrameData frozen=data;frozen.dt=0;
   for(int i=0;i<3;i++)renderer.render(a,frozen,false);
   renderer.capture(out/(wide(sceneName(scene))+L"-paused.png"));
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
  renderer.validateDebugMessages();PROCESS_MEMORY_COUNTERS_EX memory{};GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory));std::sort(timings.begin(),timings.end());double p95=timings.empty()?0:timings[size_t((timings.size()-1)*.95)];nlohmann::json report={{"scene",sceneName(scene)},{"width",width},{"height",height},{"warmupFrames",frames},{"adapter",utf8(renderer.adapterName())},{"gpuP95Ms",p95},{"privateBytes",memory.PrivateUsage},{"estimatedGpuBytes",renderer.allocations()},{"outputMode",renderer.output().hdr?"diagnostic FP16 scRGB":"SDR"},{"referenceWhiteNits",renderer.output().white},{"peakNits",renderer.output().peak},{"grid",renderer.gridSize()},{"densityGrid",renderer.densityGridSize()},{"densityDimensions",renderer.densityDimensions()},{"sceneSamples",renderer.sceneSamples()},{"pressureMultigrid",multigrid},{"elapsedSeconds",std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()},{"evidence","Native D3D12 offscreen readback; not physical desktop or presentation performance"}};
  report["gpuMedianMs"]=timings.empty()?0:timings[timings.size()/2];
  report["textureGpuBytes"]=renderer.textureAllocations();
  report["startTimeOffsetSeconds"]=startTime;
  report["timingWarmupFrames"]=timingWarmup;
  report["timingSampleCount"]=timings.size();
  if(isProceduralWorld(scene)||scene==Scene::Mars||scene==Scene::Earth||scene==Scene::Moon||scene==Scene::Jupiter||(scene==Scene::Saturn||scene==Scene::Mercury||scene==Scene::Venus||scene==Scene::Neptune||(scene==Scene::Uranus||scene==Scene::Pluto||scene==Scene::Sun)))report["quality"]=utf8(qualityOption);
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
  MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){HWND root=GetAncestor(msg.hwnd,GA_ROOT);wchar_t name[64]{};GetClassNameW(root,name,64);if((!wcscmp(name,L"VolturaWorldImport")||!wcscmp(name,L"VolturaSavedPresets"))&&IsDialogMessageW(root,&msg))continue;if(library&&IsDialogMessageW(library,&msg))continue;TranslateMessage(&msg);DispatchMessageW(&msg);}if(powerNotify)UnregisterPowerSettingNotification(powerNotify);
 }catch(const std::exception& e){error(e);result=1;}
 renderThread.request_stop();if(renderThread.joinable())renderThread.join();running=false;saveCv.notify_all();if(saveThread.joinable())saveThread.join();audio.configure(false,{});for(auto& s:surfaces)if(IsWindow(s.hwnd))DestroyWindow(s.hwnd);if(library&&IsWindow(library))DestroyWindow(library);if(font)DeleteObject(font);if(headingFont)DeleteObject(headingFont);if(singleInstance)CloseHandle(singleInstance);CoUninitialize();return result;
}

