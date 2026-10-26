// Exercise the real library window, renderer recovery and notification handlers
// without tray registration, installer, user's settings, or changes to the real desktop.
#define wWinMain unusedApplicationEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>
#include <dlgs.h>
#include "../src/ui_scrollbar.h"
void requireUi(bool value,const char* text){if(!value)throw std::runtime_error(text);}
void pump(){for(int i=0;i<10;i++){MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}Sleep(5);}}
void requireRenderRecovery(){
 WNDCLASSW cls{};cls.hInstance=instance;cls.lpszClassName=L"VolturaRenderRecoveryTest";
 cls.lpfnWndProc=[](HWND hwnd,UINT msg,WPARAM w,LPARAM l)->LRESULT{return msg==renderStatusMessage?controllerProc(hwnd,msg,w,l):DefWindowProcW(hwnd,msg,w,l);};
 RegisterClassW(&cls);controller=CreateWindowW(cls.lpszClassName,L"",WS_POPUP,0,0,1,1,nullptr,nullptr,instance,nullptr);
 for(auto& surface:surfaces)surface.hwnd=CreateWindowW(L"STATIC",L"",WS_POPUP|WS_VISIBLE,0,0,160,90,nullptr,nullptr,instance,nullptr);
 surfaces[0].monitor.refreshHz=24;surfaces[1].monitor.refreshHz=60;
 exeDir=executable().parent_path();pauseState={};settings.audioDevice.clear();
 {std::lock_guard lock(stateMutex);
  Assignment bad;bad.scene=Scene::Video;bad.video=(exeDir/L"missing-recovery-test.mp4").wstring();
  Assignment good;good.scene=Scene::Silk;good.performance={Quality::High,FrameLimit::Fps60};bad.performance=good.performance;settings.displays={{surfaces[0].monitor.id,bad},{surfaces[1].monitor.id,good}};}
 auto await=[&](auto condition){auto deadline=GetTickCount64()+15000;while(GetTickCount64()<deadline){pump();if(condition())return true;}return false;};
 startRendering();
 bool failed=await([]{return renderingProblem;});
 {std::lock_guard lock(stateMutex);settings.displays[0].second.scene=Scene::Silk;}
 bool recovered=await([]{return !renderingProblem;});
 uint64_t slowBefore,fastBefore;{std::lock_guard lock(stateMutex);slowBefore=surfaces[0].renderedFrames;fastBefore=surfaces[1].renderedFrames;}
 wchar_t durationText[16]{};GetEnvironmentVariableW(L"WORLDS_TEST_ENDURANCE_SECONDS",durationText,16);
 auto duration=std::clamp(_wtoi(durationText)*1000,2500,120000);
 auto end=GetTickCount64()+duration;while(GetTickCount64()<end)pump();
 uint64_t slowFrames,fastFrames;{std::lock_guard lock(stateMutex);slowFrames=surfaces[0].renderedFrames-slowBefore;fastFrames=surfaces[1].renderedFrames-fastBefore;}
 std::cout<<"Mixed native cadence over "<<duration/1000.0<<" seconds: "<<slowFrames<<" frames at 24 FPS, "<<fastFrames<<" at 60 FPS\n";
 requireUi(slowFrames>=20&&fastFrames>=50&&fastFrames>slowFrames*1.5,"Native mixed 24/60 deadlines progress independently");
 syncControls();wchar_t performanceText[256]{};GetWindowTextW(GetDlgItem(library,performanceStatusId),performanceText,256);
 requireUi(wcsstr(performanceText,L"FPS")&&wcsstr(performanceText,L"Native resolution"),"Advanced performance status reports active cadence and detail");
 {std::lock_guard lock(stateMutex);auto& a=settings.displays[0].second;a.scene=Scene::Video;a.video=(exeDir.parent_path().parent_path()/L"tests"/L"fixtures"/L"h264.mp4").wstring();a.videoSettings={};surfaces[0].pendingVideoSpeed=.5;}
 requireUi(await([]{std::lock_guard lock(stateMutex);return !surfaces[0].pendingVideoSpeed&&settings.displays[0].second.videoSettings.speed==.5;}),"Native renderer acknowledges and persists requested video speed");
 {std::lock_guard lock(stateMutex);pauseState.manual=true;surfaces[0].pendingVideoSpeed=2;}
 auto pausedUntil=GetTickCount64()+150;while(GetTickCount64()<pausedUntil)pump();
 {std::lock_guard lock(stateMutex);requireUi(settings.displays[0].second.videoSettings.speed==.5&&surfaces[0].pendingVideoSpeed==2,"Paused speed request is not persisted prematurely");pauseState.manual=false;}
 requireUi(await([]{std::lock_guard lock(stateMutex);return !surfaces[0].pendingVideoSpeed&&settings.displays[0].second.videoSettings.speed==2;}),"Resumed video applies pending speed");
 auto oldContext=GpuContext::acquire();Microsoft::WRL::ComPtr<ID3D12Device5> removable;requireUi(SUCCEEDED(oldContext->device.As(&removable)),"Device removal interface");removable->RemoveDevice();removable.Reset();oldContext.reset();
 bool removed=await([]{return renderingProblem;});
 {std::lock_guard lock(stateMutex);surfaces[0].pendingVideoSpeed=1.25;}
 bool adapterRecovered=await([]{std::lock_guard lock(stateMutex);return !renderingProblem&&!surfaces[0].pendingVideoSpeed&&settings.displays[0].second.videoSettings.speed==1.25;});
 requireUi(removed&&adapterRecovered,"Actual adapter removal recovers both displays");
 renderThread.request_stop();if(renderThread.joinable())renderThread.join();
 // Notifications queued by an older renderer must not corrupt the current UI.
 SendMessageW(controller,renderStatusMessage,TRUE,renderGeneration-1);
 bool staleIgnored=!renderingProblem;
 for(auto& surface:surfaces){DestroyWindow(surface.hwnd);surface.hwnd=nullptr;}
 DestroyWindow(controller);controller=nullptr;
 requireUi(failed&&recovered,"Missing video is reported and switching scenes recovers rendering on both displays");
 requireUi(staleIgnored,"Old renderer failure notifications are ignored");
}
int syncPaints=0;
int unrelatedPresetMoves=0;
LRESULT CALLBACK countPresetMoves(HWND hwnd,UINT msg,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR){if(msg==WM_WINDOWPOSCHANGING)++unrelatedPresetMoves;return DefSubclassProc(hwnd,msg,w,l);}
struct NativeScrollProbe {int calls=0;bool clipped=true;};
struct FooterProbe {int moves=0,paints=0;};
LRESULT CALLBACK probeFooter(HWND hwnd,UINT msg,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data){auto probe=reinterpret_cast<FooterProbe*>(data);if(msg==WM_WINDOWPOSCHANGING)++probe->moves;if(msg==WM_PAINT)++probe->paints;return DefSubclassProc(hwnd,msg,w,l);}
LRESULT CALLBACK probeNativeScrollbar(HWND hwnd,UINT msg,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data){auto probe=reinterpret_cast<NativeScrollProbe*>(data);auto inspect=[&]{RECT bounds{};GetWindowRect(hwnd,&bounds);auto region=CreateRectRgn(0,0,0,0);int x=bounds.right-bounds.left-1-GetSystemMetricsForDpi(SM_CXVSCROLL,GetDpiForWindow(hwnd))/2,y=(bounds.bottom-bounds.top)/2;probe->clipped&=GetWindowRgn(hwnd,region)!=ERROR&&!PtInRegion(region,x,y);DeleteObject(region);};bool scroll=msg==WM_VSCROLL||msg==WM_MOUSEWHEEL||msg==EM_LINESCROLL||msg==LB_SETTOPINDEX;if(scroll){++probe->calls;inspect();}auto result=DefSubclassProc(hwnd,msg,w,l);if(scroll)inspect();return result;}
LRESULT CALLBACK countSyncPaint(HWND hwnd,UINT msg,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR){
 if(msg==WM_PAINT||msg==WM_ERASEBKGND)++syncPaints;
 return DefSubclassProc(hwnd,msg,w,l);
}
void requireQuietSync(){
 pump();SetWindowSubclass(library,countSyncPaint,77,0);
 for(auto& item:layoutItems)SetWindowSubclass(item.hwnd,countSyncPaint,77,0);
 syncPaints=0;syncControls();pump();
 requireUi(syncPaints==0,"Synchronizing unchanged values must not erase or repaint the form or its controls");
 // A real quality change must leave every unrelated control untouched.
 auto quality=GetDlgItem(library,qualityId);RemoveWindowSubclass(quality,countSyncPaint,77);
 SendMessageW(quality,CB_SETCURSEL,0,0);pump();syncPaints=0;
 SendMessageW(library,WM_COMMAND,MAKEWPARAM(qualityId,CBN_SELCHANGE),reinterpret_cast<LPARAM>(quality));pump();
 requireUi(syncPaints==0,"Dropdown selection must not repaint the parent or unrelated controls");
 RemoveWindowSubclass(library,countSyncPaint,77);
 for(auto& item:layoutItems)RemoveWindowSubclass(item.hwnd,countSyncPaint,77);
}
void requireDesktopActionStates(){
 auto retry=GetDlgItem(library,retryId);
 auto requireAction=[&](bool visible,const wchar_t* text){
  syncControls();layoutLibrary();pump();wchar_t actual[64]{};GetWindowTextW(retry,actual,64);
  requireUi(bool(IsWindowVisible(retry))==visible,"Desktop action visibility survives layout");
  requireUi(std::wstring(actual)==text,"Desktop action has the state-specific label");
 };
 requireAction(true,L"Use on desktop");
 previewMode=false;requireAction(false,L"Retry now");
 lastError=L"Audio unavailable";requireAction(false,L"Retry now");lastError.clear();
 auto savedSurfaces=surfaces;surfaces.clear();desktopConnectionProblem=true;
 lastError=L"Desktop unavailable. Retrying automatically.";requireAction(true,L"Retry now");
 surfaces=savedSurfaces;desktopConnectionProblem=false;lastError.clear();
 SetFocus(retry);requireAction(false,L"Retry now");
 requireUi(GetFocus()!=retry,"Recovery does not leave keyboard focus on a hidden button");
 // Keep desktop mode for the remaining layout, theme and repaint checks.
}
void requireFlatComboHighlight(){
 auto combo=GetDlgItem(library,qualityId);HDC screen=GetDC(combo),dc=CreateCompatibleDC(screen);
 BITMAPINFO bmi{};bmi.bmiHeader={sizeof(BITMAPINFOHEADER),240,-40,1,32,BI_RGB};void* pixels{};
 auto bitmap=CreateDIBSection(screen,&bmi,DIB_RGB_COLORS,&pixels,nullptr,0);auto old=SelectObject(dc,bitmap);
 DRAWITEMSTRUCT item{};item.CtlType=ODT_COMBOBOX;item.itemID=0;item.hwndItem=combo;item.hDC=dc;item.rcItem={0,0,240,40};item.itemState=ODS_SELECTED;
 ui::draw(item,font);std::vector<unsigned char> highlighted(240*40*4);memcpy(highlighted.data(),pixels,highlighted.size());
 item.itemState|=ODS_FOCUS;ui::draw(item,font);
 bool equal=memcmp(highlighted.data(),pixels,highlighted.size())==0;
 SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(combo,screen);
 requireUi(equal,"Focused dropdown row must retain a flat highlight without an inner outline");
}
void requireBackground(){
 // Read the painted client, before PrintWindow can trigger a fresh background erase.
 RECT client{};GetClientRect(library,&client);auto dc=GetDC(library);
 auto bottom=GetPixel(dc,px(10),client.bottom-px(10));
 auto right=GetPixel(dc,client.right-px(10),px(10));ReleaseDC(library,dc);
 requireUi(bottom==ui::background&&right==ui::background,"Expanded and collapsed background must match the active theme");
}
void requireAdvancedLayout(){
 for(int id=200;id<=207;id++){
  int controlId=id==207?videoFitId:id;
  auto field=std::find_if(layoutItems.begin(),layoutItems.end(),[&](const auto& item){return GetDlgCtrlID(item.hwnd)==controlId;});
  int row=id==207?3:(id-200)/2,column=id==207?1:(id-200)%2;
  auto header=std::find_if(layoutItems.begin(),layoutItems.end(),[&](const auto& item){return item.advanced&&item.x==30+column*350&&item.y==367+row*44;});
  requireUi(field!=layoutItems.end()&&header!=layoutItems.end(),"Advanced field and header exist");
  RECT fieldRect{},headerRect{};GetWindowRect(field->hwnd,&fieldRect);GetWindowRect(header->hwnd,&headerRect);
  if(sideExpansion)requireUi(fieldRect.top-headerRect.bottom>=px(6),"Advanced header leaves space above the complete control border");
 }
 RECT status{},client{};GetWindowRect(statusLabel,&status);MapWindowPoints(nullptr,library,reinterpret_cast<POINT*>(&status),2);GetClientRect(library,&client);
 int contentHeight=sideExpansion?722:998;
 requireUi(status.left==px(30)&&status.bottom==px(contentHeight-17)-scrollOffset,"Visualization info stays at the bottom left of expanded content");
}
void mouseSelect(int id,int option){
 HWND comboWindow=GetDlgItem(library,id);COMBOBOXINFO info{sizeof(info)};GetComboBoxInfo(comboWindow,&info);
 PostMessageW(comboWindow,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(info.rcButton.left+3,info.rcButton.top+3));
 PostMessageW(comboWindow,WM_LBUTTONUP,0,MAKELPARAM(info.rcButton.left+3,info.rcButton.top+3));pump();
 requireUi(SendMessageW(comboWindow,CB_GETDROPPEDSTATE,0,0)!=0,"Mouse must open dropdown");
 GetComboBoxInfo(comboWindow,&info);SendMessageW(info.hwndList,LB_SETTOPINDEX,std::max(0,option-3),0);pump();RECT item{};SendMessageW(info.hwndList,LB_GETITEMRECT,option,reinterpret_cast<LPARAM>(&item));
 auto point=MAKELPARAM(item.left+10,(item.top+item.bottom)/2);
 PostMessageW(info.hwndList,WM_MOUSEMOVE,0,point);PostMessageW(info.hwndList,WM_LBUTTONDOWN,MK_LBUTTON,point);PostMessageW(info.hwndList,WM_LBUTTONUP,0,point);pump();
 if(SendMessageW(comboWindow,CB_GETCURSEL,0,0)!=option){std::cerr<<"Dropdown ID "<<id<<"\n";throw std::runtime_error("Mouse selection did not persist");}
 requireUi(!SendMessageW(comboWindow,CB_GETDROPPEDSTATE,0,0),"Mouse selection must close dropdown");
}
void screenshot(const std::filesystem::path& path,HWND target=library){
 RECT r{};GetWindowRect(target,&r);int width=r.right-r.left,height=r.bottom-r.top;HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);
 BITMAPINFO bmi{};bmi.bmiHeader={sizeof(BITMAPINFOHEADER),width,-height,1,32,BI_RGB};void* pixels{};auto bitmap=CreateDIBSection(screen,&bmi,DIB_RGB_COLORS,&pixels,nullptr,0);auto old=SelectObject(dc,bitmap);
 PrintWindow(target,dc,2);BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+width*height*4;
 std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<char*>(&file),sizeof(file));out.write(reinterpret_cast<char*>(&bmi.bmiHeader),sizeof(bmi.bmiHeader));out.write(reinterpret_cast<char*>(pixels),width*height*4);
 SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(nullptr,screen);
}
std::filesystem::path menuShot;
HMENU measuredMenu{};bool nativeMenuScaleCorrect=false;
void requireMenuTextRecovery(){
 // Reproduce the reported mismatch without changing the user's displays:
 // Windows supplies a scaled row, but the retained owner font is still 96 DPI.
 HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);
 auto stale=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 for(UINT dpi:{96u,120u,144u,192u,216u,240u,288u}){
  int width=MulDiv(230,dpi,96),height=MulDiv(36,dpi,96);
  BITMAPINFO bmi{};bmi.bmiHeader={sizeof(BITMAPINFOHEADER),width,-height,1,32,BI_RGB};void* bits{};
  auto bitmap=CreateDIBSection(screen,&bmi,DIB_RGB_COLORS,&bits,nullptr,0);auto previous=SelectObject(dc,bitmap);
  DRAWITEMSTRUCT item{};item.CtlType=ODT_MENU;item.hDC=dc;item.rcItem={0,0,width,height};item.itemData=reinterpret_cast<ULONG_PTR>(L"Open Worlds");
  auto originalFont=GetCurrentObject(dc,OBJ_FONT);
  ui::draw(item,stale,false,dpi);GdiFlush();
  requireUi(GetCurrentObject(dc,OBJ_FONT)==originalFont,"Menu painting restores the borrowed DC font");
  int top=height,bottom=-1;
  for(int y=0;y<height;y++)for(int x=MulDiv(40,dpi,96);x<MulDiv(190,dpi,96);x++){
   DWORD pixel=static_cast<DWORD*>(bits)[y*width+x];
   if(std::abs(int((pixel>>16)&255)-int(GetRValue(ui::surface)))>100){top=std::min(top,y);bottom=std::max(bottom,y);}
  }
  bool correct=bottom-top+1>=MulDiv(9,dpi,96)&&bottom-top+1<=MulDiv(18,dpi,96);
  // Drawing must restore the borrowed DC's font and release temporary fonts.
  SelectObject(dc,previous);DeleteObject(bitmap);
  requireUi(correct,"Scaled menu rows must recover full-size text from a stale 96-DPI font");
 }
 DeleteObject(stale);DeleteDC(dc);ReleaseDC(nullptr,screen);
}
void CALLBACK captureMenu(HWND,UINT,UINT_PTR timer,DWORD){
 HWND popup=FindWindowW(L"#32768",nullptr);if(popup){
  screenshot(menuShot,popup);
  RECT row{};GetMenuItemRect(nullptr,measuredMenu,0,&row);
  // Inspect actual rendered ink as well as row height: a native menu can have
  // correctly sized rows while Windows rescales owner-drawn text incorrectly.
  RECT window{};GetWindowRect(popup,&window);int width=window.right-window.left,height=window.bottom-window.top;
  HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);BITMAPINFO bmi{};
  bmi.bmiHeader={sizeof(BITMAPINFOHEADER),width,-height,1,32,BI_RGB};void* bits{};
  auto bitmap=CreateDIBSection(screen,&bmi,DIB_RGB_COLORS,&bits,nullptr,0);auto old=SelectObject(dc,bitmap);PrintWindow(popup,dc,2);
  int top=height,bottom=-1;
  // ClearType's edge pixels need not equal the foreground exactly, especially
  // at 96 DPI. Measure at least half text coverage against the menu background
  // while retaining the same row-height and rendered-glyph-size requirements.
  int fr=GetRValue(ui::foreground)-GetRValue(ui::surface),fg=GetGValue(ui::foreground)-GetGValue(ui::surface),fb=GetBValue(ui::foreground)-GetBValue(ui::surface);
  int contrast=fr*fr+fg*fg+fb*fb;
  for(int y=std::max(0,int(row.top-window.top));y<std::min(height,int(row.bottom-window.top));y++)
   for(int x=MulDiv(40,menuDpi,96);x<std::min(width,MulDiv(190,menuDpi,96));x++){
    auto pixel=static_cast<DWORD*>(bits)[y*width+x];
    int coverage=(int((pixel>>16)&255)-GetRValue(ui::surface))*fr+(int((pixel>>8)&255)-GetGValue(ui::surface))*fg+(int(pixel&255)-GetBValue(ui::surface))*fb;
    if(coverage*2>=contrast){top=std::min(top,y);bottom=std::max(bottom,y);}
   }
  nativeMenuScaleCorrect=std::abs((row.bottom-row.top)-MulDiv(36,menuDpi,96))<=2&&bottom-top+1>=MulDiv(9,menuDpi,96)&&bottom-top+1<=MulDiv(18,menuDpi,96);
  if(!nativeMenuScaleCorrect)std::cerr<<"Menu DPI="<<menuDpi<<" row="<<(row.bottom-row.top)<<" ink="<<(bottom-top+1)<<" window="<<width<<"x"<<height<<" rowY="<<(row.top-window.top)<<"\n";
  SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(nullptr,screen);
 }
 EndMenu();KillTimer(nullptr,timer);
}
void requireSolarMenu(){
 const wchar_t* names[]={L"Random",L"",L"Sun",L"Mercury",L"Venus",L"Earth",L"Moon",L"Mars",L"Jupiter",L"Saturn",L"Uranus",L"Neptune",L"Pluto"};
 for(int choice=0;choice<=solarRandomChoice;choice++){
  requireUi(selectVisualization(choice),"Menu choices remain selectable");
  HMENU root=visualizationMenu(),solar=GetSubMenu(root,2);
  requireUi(solar&&GetMenuItemCount(solar)==int(std::size(names)),"Solar System follows Orbit and contains rotation plus all bodies");
  MENUITEMINFOW heading{sizeof(heading)};heading.fMask=MIIM_DATA|MIIM_STATE;
  requireUi(GetMenuItemInfoW(root,2,TRUE,&heading)&&!wcscmp(reinterpret_cast<const wchar_t*>(heading.dwItemData),L"Solar System"),"Solar submenu heading");
  bool solarActive=choice==solarRandomChoice||(!isRandomChoice(choice)&&bodyFacts(selected().scene));
  requireUi(bool(heading.fState&MFS_CHECKED)==solarActive,"Solar heading tracks manual bodies and solar rotation only");
  int checked=0;
  for(int i=0;i<GetMenuItemCount(solar);i++){
   MENUITEMINFOW item{sizeof(item)};item.fMask=MIIM_DATA|MIIM_FTYPE|MIIM_ID|MIIM_STATE;
   requireUi(GetMenuItemInfoW(solar,i,TRUE,&item),"Solar item available");
   if(i==1){requireUi(item.fType&MFT_SEPARATOR,"Random is separated from bodies");continue;}
   requireUi(!wcscmp(reinterpret_cast<const wchar_t*>(item.dwItemData),names[i]),"Solar items have the intended order and labels");
   if(item.fState&MFS_CHECKED){checked++;requireUi(item.wID==visualizationMenuBase+choice,"Only the selected solar command is checked");}
  }
  requireUi(checked==(solarActive?1:0),"Solar rotation checks Random rather than its current body");
  for(int i=0;i<GetMenuItemCount(root);i++){
   if(i==2)continue;
   int command=int(GetMenuItemID(root,i))-visualizationMenuBase;
   requireUi(command>=0&&command<int(std::size(visualizations))&&!bodyFacts(visualizations[command].scene)&&command!=solarRandomChoice,"Solar choices are absent from the parent menu");
  }
  DestroyMenu(root);
 }
 selectVisualization(1);
}
void captureThemedMenu(const std::filesystem::path& path,int submenu){
 WNDCLASSW cls{};cls.hInstance=instance;cls.lpfnWndProc=[](HWND hwnd,UINT msg,WPARAM w,LPARAM l)->LRESULT{if(msg==WM_DESTROY)return 0;return controllerProc(hwnd,msg,w,l);};cls.lpszClassName=L"VolturaMenuTestOwner";RegisterClassW(&cls);
 HWND owner=CreateWindowW(cls.lpszClassName,L"Menu test",WS_POPUP,0,0,0,0,nullptr,nullptr,instance,nullptr);
 HMENU root=submenu?visualizationMenu():CreatePopupMenu();
 HMENU menu=submenu==2?GetSubMenu(root,2):root;
 if(!submenu){AppendMenuW(menu,MF_OWNERDRAW,1,L"Open Worlds");AppendMenuW(menu,MF_OWNERDRAW|MF_POPUP,reinterpret_cast<UINT_PTR>(visualizationMenu()),L"Visualizations");AppendMenuW(menu,MF_OWNERDRAW,2,L"Pause");}
 MENUINFO info{sizeof(info),MIM_BACKGROUND};info.hbrBack=ui::surfaceBrush;SetMenuInfo(menu,&info);
 // Reopen after stale state, as happens when a display disappears and returns.
 menuDpi=72;prepareTrayMenu(owner,{100,100});menuShot=path;measuredMenu=menu;nativeMenuScaleCorrect=false;
 RECT ownerRect{};GetWindowRect(owner,&ownerRect);
 requireUi(ownerRect.left==100&&ownerRect.top==100&&!IsWindowVisible(owner),"Menu owner follows invocation without becoming visible");
 requireUi(menuDpi==GetDpiForWindow(owner),"Menu font and native owner use the same current DPI");
 LOGFONTW actual{};GetObjectW(menuFont,sizeof(actual),&actual);
 requireUi(actual.lfHeight==-MulDiv(16,menuDpi,96),"Menu font is recreated at the current owner DPI");
 auto hook=SetWindowsHookExW(WH_CBT,ui::menuCreation,nullptr,GetCurrentThreadId());
 SetTimer(nullptr,0,250,captureMenu);TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,100,100,0,owner,nullptr);
 if(hook)UnhookWindowsHookEx(hook);DeleteObject(menuFont);menuFont=nullptr;DestroyMenu(root);DestroyWindow(owner);
 requireUi(nativeMenuScaleCorrect,"Native popup row height and rendered text must scale together");
}
HWND navigationOwner=nullptr;
HMENU navigationRoot=nullptr;
int navigationStep=0;
bool navigationOpened=false;
void CALLBACK navigateSolarMenu(HWND,UINT,UINT_PTR timer,DWORD){
 if(navigationStep<3)PostMessageW(navigationOwner,WM_KEYDOWN,VK_DOWN,1);
 if(navigationStep==3)PostMessageW(navigationOwner,WM_KEYDOWN,VK_RIGHT,1);
 if(navigationStep==4){
  EnumThreadWindows(GetCurrentThreadId(),[](HWND window,LPARAM)->BOOL{
   MENUBARINFO bar{sizeof(bar)};
   if(GetMenuBarInfo(window,OBJID_CLIENT,0,&bar)&&bar.hMenu==GetSubMenu(navigationRoot,2))navigationOpened=true;
   return TRUE;
  },0);
  PostMessageW(navigationOwner,WM_KEYDOWN,VK_RETURN,1);
 }
 if(++navigationStep>8){EndMenu();KillTimer(nullptr,timer);}
}
void requireSolarNavigation(){
 navigationOwner=CreateWindowW(L"STATIC",L"Menu navigation",WS_POPUP,100,100,1,1,nullptr,nullptr,instance,nullptr);
 navigationRoot=visualizationMenu();navigationStep=0;navigationOpened=false;
 prepareTrayMenu(navigationOwner,{100,100});SetForegroundWindow(navigationOwner);
 // Use the real owner drawing and menu-loop handling for native input messages.
 SetWindowLongPtrW(navigationOwner,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(controllerProc));
 auto hook=SetWindowsHookExW(WH_CBT,ui::menuCreation,nullptr,GetCurrentThreadId());
 auto timer=SetTimer(nullptr,0,200,navigateSolarMenu);
 int command=TrackPopupMenu(navigationRoot,TPM_RETURNCMD|TPM_NONOTIFY,100,100,0,navigationOwner,nullptr);
 KillTimer(nullptr,timer);if(hook)UnhookWindowsHookEx(hook);
 DestroyMenu(navigationRoot);
 SetWindowLongPtrW(navigationOwner,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(DefWindowProcW));DestroyWindow(navigationOwner);
 DeleteObject(menuFont);menuFont=nullptr;
 requireUi(navigationOpened,"Native navigation opens Solar System submenu");
 requireUi(command==visualizationMenuBase+solarRandomChoice,"Keyboard selects solar Random through the submenu");
 requireUi(selectVisualization(command-visualizationMenuBase),"Nested command uses existing selection handling");
}
HWND presetDeletionOwner{};
std::filesystem::path presetConfirmationShot;
bool presetConfirmationThemed=false,presetConfirmationAccept=true;
void CALLBACK confirmPresetDeletion(HWND,UINT,UINT_PTR timer,DWORD){auto dialog=FindWindowW(L"VolturaWorldConfirm",L"Delete preset");if(!dialog)dialog=FindWindowW(L"VolturaWorldConfirm",L"Replace World package");if(!dialog)dialog=FindWindowW(L"VolturaWorldConfirm",L"Replace saved preset");if(dialog&&GetWindow(dialog,GW_OWNER)==presetDeletionOwner){KillTimer(nullptr,timer);RedrawWindow(dialog,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);auto dc=GetDC(dialog);presetConfirmationThemed=GetPixel(dc,4,4)==ui::background;ReleaseDC(dialog,dc);if(!presetConfirmationShot.empty())screenshot(presetConfirmationShot,dialog);SendMessageW(dialog,WM_COMMAND,presetConfirmationAccept?IDYES:IDNO,0);}}
std::filesystem::path presetExportDestination;bool presetExportChosen=false;ULONGLONG presetExportDialogSeen=0;
void CALLBACK choosePresetExport(HWND,UINT,UINT_PTR timer,DWORD){auto dialog=FindWindowW(L"#32770",L"Export desktop preset");if(!dialog)dialog=FindWindowW(L"#32770",L"Import preset");if(!dialog||GetWindow(dialog,GW_OWNER)!=presetDeletionOwner)return;if(!presetExportDialogSeen)presetExportDialogSeen=GetTickCount64();HWND edit{};EnumChildWindows(dialog,[](HWND child,LPARAM result)->BOOL{wchar_t cls[80]{},value[512]{};GetClassNameW(child,cls,80);GetWindowTextW(child,value,512);if(!wcscmp(cls,L"Edit")&&(!wcscmp(value,L"Desktop.world")||!wcscmp(value,L"*.world")||GetDlgCtrlID(child)==edt1)){*reinterpret_cast<HWND*>(result)=child;return FALSE;}return TRUE;},reinterpret_cast<LPARAM>(&edit));if(!edit&&GetTickCount64()-presetExportDialogSeen<5000)return;KillTimer(nullptr,timer);if(!edit){std::cerr<<"Save As filename field not found\n";EnumChildWindows(dialog,[](HWND child,LPARAM)->BOOL{wchar_t cls[80]{},value[512]{};GetClassNameW(child,cls,80);GetWindowTextW(child,value,512);std::cerr<<"  "<<GetDlgCtrlID(child)<<" "<<utf8(cls)<<" "<<utf8(value)<<"\n";return TRUE;},0);PostMessageW(dialog,WM_COMMAND,IDCANCEL,0);return;}SetWindowTextW(edit,presetExportDestination.c_str());presetExportChosen=true;PostMessageW(dialog,WM_COMMAND,IDOK,0);}
void requireSavedPresets(const std::filesystem::path& shots){
 auto oldSettings=settings;auto oldProfile=profile;auto root=std::filesystem::temp_directory_path()/wide(newPresetId());std::filesystem::create_directories(root);profile=root;
 auto wait=[&](HWND window){auto deadline=GetTickCount64()+30000;do{pump();if(IsWindowEnabled(GetDlgItem(window,20)))return;}while(GetTickCount64()<deadline);throw std::runtime_error("Preset operation timed out.");};
 auto click=[&](HWND window,int id){SendMessageW(window,WM_COMMAND,MAKEWPARAM(id,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(window,id)));pump();};
 auto choose=[&](HWND window,int index){SendDlgItemMessageW(window,10,LB_SETCURSEL,index,0);SendMessageW(window,WM_COMMAND,MAKEWPARAM(10,LBN_SELCHANGE),reinterpret_cast<LPARAM>(GetDlgItem(window,10)));pump();};
 try{
  Assignment first;first.scene=Scene::Sun;first.params.brightness=.72f;first.params.palette=2;first.reaction=Reaction::Gentle;first.random=true;first.randomPool=RandomPool::SolarSystem;first.randomInterval=60;first.showBodyStatistics=true;first.performance={Quality::High,FrameLimit::Fps30};first.output=OutputPreference::Sdr;
  Assignment second;second.scene=Scene::Off;settings.displays={{surfaces[0].monitor.id,first},{surfaces[1].monitor.id,second}};settings.pauseBattery=false;settings.audioDevice=L"not-an-installed-audio-device";syncControls();
  SendMessageW(GetDlgItem(library,presetsId),BM_CLICK,0,0);pump();auto window=FindWindowW(L"VolturaSavedPresets",nullptr);requireUi(window!=nullptr,"Saved presets button opens management window");wait(window);
  requireUi(!IsWindowVisible(GetDlgItem(window,35)),"Default Presets form fits without an outer scrollbar");RECT footerBounds{};GetWindowRect(GetDlgItem(window,36),&footerBounds);for(int id:{10,11,12,13,20,21,22,23,24,25,26,27,34}){RECT controlBounds{};GetWindowRect(GetDlgItem(window,id),&controlBounds);requireUi(controlBounds.bottom<=footerBounds.top,"Every preset field and action fits above the footer");}
  requireUi(!IsWindowEnabled(GetDlgItem(window,24)),"Apply disabled without selection");SetDlgItemTextW(window,11,L"Evening – 東京");SetDlgItemTextW(window,12,L"Two-display preset");click(window,20);wait(window);requireUi(SendDlgItemMessageW(window,10,LB_GETCOUNT,0,0)==1,"Save current lists named desktop preset");requireUi(selected().scene==Scene::Sun,"Saving does not alter playback");
  choose(window,0);for(int dark=0;dark<2;dark++){ui::refresh(window,dark);RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_FRAME|RDW_ALLCHILDREN|RDW_UPDATENOW);pump();requireUi(!(GetWindowLongPtrW(window,GWL_STYLE)&WS_VSCROLL),"Preset window has no native scrollbar to flash");auto ownedBar=GetDlgItem(window,35);auto barDc=GetDC(ownedBar);auto trackColor=GetPixel(barDc,1,1);ReleaseDC(ownedBar,barDc);if(IsWindowVisible(ownedBar))requireUi(trackColor==ui::surface,"Owned scrollbar uses library colors");auto name=GetDlgItem(window,11);requireUi(!(GetWindowLongPtrW(name,GWL_EXSTYLE)&WS_EX_CLIENTEDGE),"Preset fields have no native bevel");for(int id:{10,11,12,13}){auto field=GetDlgItem(window,id);RECT client{};GetClientRect(field,&client);auto dc=GetDC(field);auto color=GetPixel(dc,client.right-12,client.bottom/2);ReleaseDC(field,dc);requireUi(color==ui::surface,"Preset inputs and list use library surface color in both themes");}SetFocus(name);pump();auto dc=GetWindowDC(name);auto edge=GetPixel(dc,0,5);ReleaseDC(name,dc);requireUi(edge==ui::accent,"Focused preset input has library accent border");SetFocus(GetDlgItem(window,20));pump();if(!shots.empty())screenshot(shots/(dark?L"presets-dark.bmp":L"presets-light.bmp"),window);}
  // Moving focus among visible buttons/inputs must not repaint the whole form.
  auto unrelatedField=GetDlgItem(window,12);SetWindowSubclass(unrelatedField,countPresetMoves,78,0);unrelatedPresetMoves=0;SetFocus(GetDlgItem(window,11));pump();SetFocus(GetDlgItem(window,20));pump();requireUi(unrelatedPresetMoves==0,"Preset focus changes do not relayout unrelated controls");requireUi((GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_COMPOSITED)!=0,"Preset child painting is buffered");RemoveWindowSubclass(unrelatedField,countPresetMoves,78);
  // Observe the native handlers themselves, before any later themed repaint.
  auto description=GetDlgItem(window,12);NativeScrollProbe scrollProbe;SetWindowSubclass(description,probeNativeScrollbar,97,reinterpret_cast<DWORD_PTR>(&scrollProbe));std::wstring longText;for(int i=0;i<50;i++)longText+=L"Scrollable description line\r\n";SetWindowTextW(description,longText.c_str());pump();auto descriptionBar=ui::fieldScrollbar(description);requireUi(descriptionBar!=nullptr,"Native description has an app-owned scrollbar sibling");
  for(int dark=0;dark<2;dark++){ui::refresh(window,dark);for(int i=0;i<12;i++){SendMessageW(description,WM_MOUSEWHEEL,MAKEWPARAM(0,-WHEEL_DELTA),0);SendMessageW(description,WM_VSCROLL,SB_LINEUP,0);SendMessageW(description,EM_LINESCROLL,0,1);}auto info=ui::scrollbarInfo(descriptionBar);requireUi(info.nMax>=int(info.nPage),"Long description has a scrollable range");SendMessageW(descriptionBar,WM_KEYDOWN,VK_END,0);requireUi(SendMessageW(description,EM_GETFIRSTVISIBLELINE,0,0)>0,"Owned scrollbar keyboard moves native text");auto barState=reinterpret_cast<ui::scroll_detail::Bar*>(GetWindowLongPtrW(descriptionBar,GWLP_USERDATA));auto thumb=ui::scroll_detail::thumb(*barState);SendMessageW(descriptionBar,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM((thumb.left+thumb.right)/2,(thumb.top+thumb.bottom)/2));SendMessageW(descriptionBar,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM((thumb.left+thumb.right)/2,0));SendMessageW(descriptionBar,WM_LBUTTONUP,0,0);requireUi(SendMessageW(description,EM_GETFIRSTVISIBLELINE,0,0)==0&&GetCapture()!=descriptionBar,"Owned scrollbar drag reaches top and releases capture");}RemoveWindowSubclass(description,probeNativeScrollbar,97);requireUi(scrollProbe.calls>=48&&scrollProbe.clipped,"Native scrollbar region stays invisible inside wheel, line and drag handlers");SetDlgItemTextW(window,12,L"Two-display preset");
  RECT normalWindow{};GetWindowRect(window,&normalWindow);SetWindowPos(window,nullptr,0,0,normalWindow.right-normalWindow.left,MulDiv(600,GetDpiForWindow(window),96),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);pump();
  auto outerBar=GetDlgItem(window,35);SendMessageW(outerBar,WM_KEYDOWN,VK_END,0);auto end=ui::scrollbarInfo(outerBar);requireUi(end.nPos==std::max(0,end.nMax-int(end.nPage)+1),"Owned outer scrollbar scrolls the form to its end");SendMessageW(outerBar,WM_KEYDOWN,VK_HOME,0);requireUi(ui::scrollbarInfo(outerBar).nPos==0,"Owned outer scrollbar returns to top");
  auto footer=GetDlgItem(window,36);requireUi(footer!=nullptr,"Status has a separate footer control");pump();FooterProbe footerProbe;SetWindowSubclass(footer,probeFooter,96,reinterpret_cast<DWORD_PTR>(&footerProbe));RECT fixedFooter{};GetWindowRect(footer,&fixedFooter);wchar_t statusBefore[2048]{};GetWindowTextW(footer,statusBefore,2048);
  for(int i=0;i<12;i++){SendMessageW(window,WM_MOUSEWHEEL,MAKEWPARAM(0,-WHEEL_DELTA),0);pump();SendMessageW(window,WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),0);pump();RECT actual{};GetWindowRect(footer,&actual);requireUi(EqualRect(&fixedFooter,&actual),"Status footer stays fixed while form content scrolls");}
  requireUi(footerProbe.moves==0,"Scrolling never repositions status text");SendMessageW(window,WM_VSCROLL,SB_TOP,0);pump();int paints=footerProbe.paints;for(int i=0;i<8;i++){SendMessageW(window,WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),0);pump();}requireUi(footerProbe.paints==paints,"Wheel input at boundary does not repaint unchanged status");wchar_t statusAfter[2048]{};GetWindowTextW(footer,statusAfter,2048);requireUi(!wcscmp(statusBefore,statusAfter),"Scrolling preserves status contents");RemoveWindowSubclass(footer,probeFooter,96);
  // An erase request must not expose an empty status frame before text paints.
  HDC screen=GetDC(footer),buffer=CreateCompatibleDC(screen);auto bitmap=CreateCompatibleBitmap(screen,16,16);auto previous=SelectObject(buffer,bitmap);RECT sample{0,0,16,16};auto sentinel=CreateSolidBrush(RGB(255,0,255));FillRect(buffer,&sample,sentinel);SendMessageW(footer,WM_ERASEBKGND,reinterpret_cast<WPARAM>(buffer),0);requireUi(GetPixel(buffer,4,4)==RGB(255,0,255),"Status erasure is suppressed until its buffered text/background paint");DeleteObject(sentinel);SelectObject(buffer,previous);DeleteObject(bitmap);DeleteDC(buffer);ReleaseDC(footer,screen);
  SetWindowPos(window,nullptr,0,0,normalWindow.right-normalWindow.left,normalWindow.bottom-normalWindow.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);pump();requireUi(!IsWindowVisible(GetDlgItem(window,35)),"Restored normal form size needs no outer scrollbar");
  auto stored=listPresets(root/L"presets");requireUi(stored.size()==1&&stored[0].preset.displays.size()==2,"Named preset persists both connected displays");auto originalFile=stored[0].file;
  // Drive the real native Save As dialog for both export routes.
  presetDeletionOwner=window;presetExportDestination=root/L"Shared desktop 東京.world";presetExportChosen=false;presetExportDialogSeen=0;SetDlgItemTextW(window,11,L"Shared desktop");auto exportTimer=SetTimer(nullptr,0,50,choosePresetExport);click(window,25);KillTimer(nullptr,exportTimer);wait(window);requireUi(presetExportChosen&&std::filesystem::exists(presetExportDestination)&&readDesktopPackage(presetExportDestination).title=="Shared desktop","Export selected uses metadata and native Save As destination");
  settings.displays[0].second.scene=Scene::Contour;settings.displays[0].second.random=false;presetExportDestination=root/L"Current desktop.world";presetExportChosen=false;presetExportDialogSeen=0;SetDlgItemTextW(window,11,L"Current desktop");exportTimer=SetTimer(nullptr,0,50,choosePresetExport);click(window,26);KillTimer(nullptr,exportTimer);wait(window);requireUi(presetExportChosen&&readDesktopPackage(presetExportDestination).displays[0].assignment.scene==Scene::Contour,"Export current snapshots live desktop independently of saved preset");requireUi(listPresets(root/L"presets")[0].preset.displays[0].assignment.scene==Scene::Sun,"Export does not update saved preset");choose(window,0);
  settings.displays[0].second.scene=Scene::Orbit;settings.displays[0].second.random=false;settings.displays[1].second.scene=Scene::Bloom;settings.pauseBattery=true;settings.audioDevice.clear();
  click(window,24);requireUi(IsWindowVisible(GetDlgItem(window,100))&&SendDlgItemMessageW(window,100,CB_GETCURSEL,0,0)==1&&SendDlgItemMessageW(window,101,CB_GETCURSEL,0,0)==2,"Mapping matches stable monitor identifiers");
  SendDlgItemMessageW(window,101,CB_SETCURSEL,1,0);click(window,28);requireUi(settings.displays[0].second.scene==Scene::Orbit&&settings.displays[1].second.scene==Scene::Bloom&&settings.pauseBattery,"Duplicate destination leaves assignments/globals unchanged");
  SendDlgItemMessageW(window,100,CB_SETCURSEL,0,0);SendDlgItemMessageW(window,101,CB_SETCURSEL,2,0);click(window,28);wait(window);requireUi(settings.displays[0].second.scene==Scene::Orbit&&settings.displays[1].second.scene==Scene::Off&&!settings.pauseBattery,"Skipped display preserved, mapped display and globals restored");requireUi(settings.audioDevice.empty(),"Unavailable saved audio device falls back to system");
  choose(window,0);click(window,24);click(window,28);wait(window);auto applied=settings.displays[0].second;requireUi(applied.scene==first.scene&&applied.params.brightness==first.params.brightness&&applied.random&&applied.randomInterval==60&&applied.randomPool==first.randomPool&&applied.showBodyStatistics&&applied.performance==first.performance&&applied.output==first.output,"Apply restores complete saved configuration");requireUi(!surfaces[0].randomCycle.advance(settings.displays[0].second,1,true),"Applying Random restarts countdown without switching scene");
  choose(window,0);SetDlgItemTextW(window,11,L"Renamed desktop");click(window,22);wait(window);stored=listPresets(root/L"presets");requireUi(stored[0].file==originalFile&&stored[0].preset.title=="Renamed desktop","Rename retains stable preset identifier");
  choose(window,0);settings.displays[0].second.params.palette=1;click(window,21);wait(window);requireUi(listPresets(root/L"presets")[0].preset.displays[0].assignment.params.palette==1,"Update explicitly replaces saved snapshot");
  choose(window,0);SetDlgItemTextW(window,11,L" ");click(window,20);requireUi(SendDlgItemMessageW(window,10,LB_GETCOUNT,0,0)==1,"Blank name rejected without extra preset");
  choose(window,0);auto count=SendDlgItemMessageW(window,10,LB_GETCOUNT,0,0);click(window,20);wait(window);requireUi(SendDlgItemMessageW(window,10,LB_GETCOUNT,0,0)==count,"Duplicate name rejected");
  click(window,27);requireUi(!IsWindow(window),"Close management window");
  auto portable=readDesktopPackage(originalFile);portable.title="Imported desktop";portable.displays[0].id=L"other-machine-display";auto exported=root/L"portable.world";writeDesktopPackage(exported,portable);auto currentScene=settings.displays[0].second.scene;showSavedPresets(library,presetContext());window=FindWindowW(L"VolturaSavedPresets",nullptr);wait(window);requireUi(IsWindowVisible(GetDlgItem(window,34)),"Preset hub contains Import preset action");presetDeletionOwner=window;presetExportDestination=exported;presetExportChosen=false;presetExportDialogSeen=0;auto importTimer=SetTimer(nullptr,0,50,choosePresetExport);click(window,34);KillTimer(nullptr,importTimer);wait(window);requireUi(presetExportChosen,"Preset hub native picker loads the selected preset");requireUi(SendDlgItemMessageW(window,100,CB_GETCURSEL,0,0)==0,"Unknown monitor starts skipped for explicit mapping");requireUi(settings.displays[0].second.scene==currentScene&&listPresets(root/L"presets").size()==1,"Import preview performs no installation/application");
  SendDlgItemMessageW(window,100,CB_SETCURSEL,1,0);if(!shots.empty()){ui::refresh(window,1);pump();screenshot(shots/L"preset-mapping.bmp",window);}click(window,33);wait(window);requireUi(listPresets(root/L"presets").size()==2,"Confirmed import saves portable desktop preset");
  auto findImported=[&]{for(auto& entry:listPresets(root/L"presets"))if(entry.preset.title=="Imported desktop")return entry;throw std::runtime_error("Imported preset missing.");};auto importedFile=findImported().file;
  // Re-importing an exported preset must be usable without saving a duplicate.
  settings.displays[0].second.scene=Scene::Orbit;showSavedPresets(library,presetContext(),exported);wait(window);SendDlgItemMessageW(window,100,CB_SETCURSEL,1,0);click(window,28);wait(window);requireUi(listPresets(root/L"presets").size()==2&&settings.displays[0].second.scene==portable.displays[0].assignment.scene,"Apply settings imports an existing name without duplicate-save errors");
  portable.displays[0].assignment.params.palette=3;writeDesktopPackage(exported,portable);showSavedPresets(library,presetContext(),exported);wait(window);SendDlgItemMessageW(window,100,CB_SETCURSEL,1,0);presetDeletionOwner=window;presetConfirmationAccept=false;presetConfirmationShot=shots.empty()?std::filesystem::path{}:shots/L"preset-import-replace-dark.bmp";auto replaceTimer=SetTimer(nullptr,0,50,confirmPresetDeletion);click(window,33);KillTimer(nullptr,replaceTimer);requireUi(findImported().preset.displays[0].assignment.params.palette!=3,"Cancelling duplicate-preset replacement preserves saved configuration");
  presetConfirmationAccept=true;replaceTimer=SetTimer(nullptr,0,50,confirmPresetDeletion);click(window,33);KillTimer(nullptr,replaceTimer);wait(window);requireUi(findImported().file==importedFile&&findImported().preset.displays[0].assignment.params.palette==3&&listPresets(root/L"presets").size()==2,"Save and apply explicitly replaces the existing preset without adding a duplicate");
  // If display application fails after replacement, restore the previous archive.
  portable.displays[0].assignment.params.palette=0;writeDesktopPackage(exported,portable);auto failingContext=presetContext();failingContext.apply=[](const DesktopPreset&,const std::vector<int>&,const std::vector<Monitor>&){throw std::runtime_error("Simulated display change");};showSavedPresets(library,std::move(failingContext),exported);wait(window);SendDlgItemMessageW(window,100,CB_SETCURSEL,1,0);replaceTimer=SetTimer(nullptr,0,50,confirmPresetDeletion);click(window,33);KillTimer(nullptr,replaceTimer);wait(window);requireUi(findImported().preset.displays[0].assignment.params.palette==3&&settings.displays[0].second.params.palette==3,"Failed application rolls back saved-preset replacement");presetConfirmationShot.clear();
  // A cancelled import never adds another preset.
  click(window,27);portable.title="Cancelled desktop";writeDesktopPackage(exported,portable);showSavedPresets(library,presetContext(),exported);window=FindWindowW(L"VolturaSavedPresets",nullptr);wait(window);click(window,29);wait(window);click(window,27);requireUi(listPresets(root/L"presets").size()==2,"Cancelled import leaves saved library unchanged");
  // Exercise tall mapping layout and DPI changes without modifying physical displays.
  portable.displays.clear();for(int i=0;i<32;i++)portable.displays.push_back({L"source-"+std::to_wstring(i),L"Display "+std::to_wstring(i+1),first,{}});portable.title="Many displays";writeDesktopPackage(exported,portable);showSavedPresets(library,presetContext(),exported);window=FindWindowW(L"VolturaSavedPresets",nullptr);wait(window);SendMessageW(window,WM_VSCROLL,SB_BOTTOM,0);RECT rect{};GetWindowRect(window,&rect);SendMessageW(window,WM_DPICHANGED,MAKEWPARAM(144,144),reinterpret_cast<LPARAM>(&rect));pump();SCROLLINFO scroll{sizeof(scroll),SIF_ALL};scroll=ui::scrollbarInfo(GetDlgItem(window,35));requireUi(scroll.nMax>int(scroll.nPage),"32-display mapping has scrollable content at scaled DPI");click(window,27);
  // Decode an embedded video after installation and preserve its positioning/rate.
  DesktopPreset videoPreset;videoPreset.title="Portable video";auto fixture=executable().parent_path().parent_path().parent_path()/L"tests"/L"fixtures"/L"h264.mp4";Assignment video;video.scene=Scene::Video;video.videoSettings={0,1,1.5};video.fit=true;videoPreset.displays={{surfaces[0].monitor.id,L"Video display",video,"source"}};videoPreset.media={{"source",fixture,0,std::filesystem::file_size(fixture)}};writeDesktopPackage(exported,videoPreset);showSavedPresets(library,presetContext(),exported);window=FindWindowW(L"VolturaSavedPresets",nullptr);wait(window);click(window,33);wait(window);auto installedVideo=assignmentFor(settings,surfaces[0].monitor.id);requireUi(installedVideo.scene==Scene::Video&&installedVideo.videoSettings==video.videoSettings&&installedVideo.fit&&std::filesystem::exists(installedVideo.video)&&std::filesystem::path(installedVideo.video).extension()==L".mp4","Portable video preflight and managed configuration apply");requireUi(listPresets(root/L"presets").size()==3,"Video desktop imported into saved presets");click(window,27);
  // Failed decoder preflight leaves all assignments, globals and saved presets intact.
  auto invalidVideo=root/L"damaged.mp4";{std::ofstream out(invalidVideo,std::ios::binary);out<<"0000ftypisom damaged video";}videoPreset.title="Damaged video";videoPreset.media[0].source=invalidVideo;videoPreset.media[0].size=std::filesystem::file_size(invalidVideo);videoPreset.pauseBattery=!settings.pauseBattery;writeDesktopPackage(exported,videoPreset);auto libraryCount=std::distance(std::filesystem::directory_iterator(root/L"library"),std::filesystem::directory_iterator{});showSavedPresets(library,presetContext(),exported);window=FindWindowW(L"VolturaSavedPresets",nullptr);wait(window);click(window,28);wait(window);requireUi(assignmentFor(settings,surfaces[0].monitor.id).video==installedVideo.video&&settings.pauseBattery!=videoPreset.pauseBattery&&listPresets(root/L"presets").size()==3,"Failed video validation is all-or-nothing");requireUi(std::distance(std::filesystem::directory_iterator(root/L"library"),std::filesystem::directory_iterator{})==libraryCount,"Failed video validation cleans installed staging media");click(window,27);
  showSavedPresets(library,presetContext());window=FindWindowW(L"VolturaSavedPresets",nullptr);wait(window);choose(window,1);presetDeletionOwner=window;
  for(int dark=0;dark<2;dark++){ui::refresh(window,dark);presetConfirmationAccept=false;presetConfirmationThemed=false;presetConfirmationShot=shots.empty()?std::filesystem::path{}:shots/(dark?L"preset-confirm-dark.bmp":L"preset-confirm-light.bmp");auto timer=SetTimer(nullptr,0,50,confirmPresetDeletion);click(window,23);KillTimer(nullptr,timer);requireUi(presetConfirmationThemed&&listPresets(root/L"presets").size()==3,"Delete confirmation follows both themes and Cancel preserves preset");}
  presetConfirmationAccept=true;auto timer=SetTimer(nullptr,0,50,confirmPresetDeletion);click(window,23);KillTimer(nullptr,timer);wait(window);requireUi(listPresets(root/L"presets").size()==2&&std::filesystem::exists(installedVideo.video),"Confirmed deletion preserves active video media");
  // Export replacement uses the same themed confirmation, including cancellation.
  presetExportDestination=root/L"Shared desktop 東京.world";auto originalExport=readDesktopPackage(presetExportDestination).title;SetDlgItemTextW(window,11,L"Replacement desktop");presetExportChosen=false;presetExportDialogSeen=0;presetConfirmationAccept=false;presetConfirmationShot=shots.empty()?std::filesystem::path{}:shots/L"preset-replace-dark.bmp";timer=SetTimer(nullptr,0,50,confirmPresetDeletion);auto exportTimer2=SetTimer(nullptr,0,50,choosePresetExport);click(window,26);KillTimer(nullptr,timer);KillTimer(nullptr,exportTimer2);requireUi(presetExportChosen&&presetConfirmationThemed&&readDesktopPackage(presetExportDestination).title==originalExport,"Cancelling themed overwrite preserves the old export");
  presetExportChosen=false;presetExportDialogSeen=0;presetConfirmationAccept=true;timer=SetTimer(nullptr,0,50,confirmPresetDeletion);exportTimer2=SetTimer(nullptr,0,50,choosePresetExport);click(window,26);KillTimer(nullptr,timer);KillTimer(nullptr,exportTimer2);wait(window);requireUi(readDesktopPackage(presetExportDestination).title=="Replacement desktop","Confirmed themed overwrite exports new configuration");presetConfirmationShot.clear();click(window,27);
  settings=oldSettings;profile=oldProfile;savePending=false;syncControls();std::filesystem::remove_all(root);
 }catch(...){if(auto window=FindWindowW(L"VolturaSavedPresets",nullptr)){SendMessageW(window,WM_CLOSE,0,0);for(int i=0;i<100&&IsWindow(window);i++)pump();}settings=oldSettings;profile=oldProfile;savePending=false;throw;}
}
int wmain(int argc,wchar_t** argv){
 try{
  instance=GetModuleHandleW(nullptr);CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  INITCOMMONCONTROLSEX common{sizeof(common),ICC_STANDARD_CLASSES|ICC_BAR_CLASSES};InitCommonControlsEx(&common);ui::refresh();
  WNDCLASSW cls{};cls.hInstance=instance;cls.lpfnWndProc=libraryProc;cls.lpszClassName=L"VolturaWorldLibrary";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(101));RegisterClassW(&cls);
  Surface first{},second{};first.monitor.id=L"test-one";first.monitor.name=L"Display 1";second.monitor.id=L"test-two";second.monitor.name=L"Display 2";surfaces={first,second};showLibrary();pump();
  if(argc>1)std::filesystem::create_directories(argv[1]);requireSavedPresets(argc>1?std::filesystem::path(argv[1]):std::filesystem::path{});
  {wchar_t label[80]{};GetDlgItemTextW(library,presetsId,label,80);requireUi(!wcscmp(label,L"Presets…")&&!GetDlgItem(library,importId),"Library has a single Presets entry instead of a separate import action");GetDlgItemTextW(library,resetId,label,80);requireUi(!wcscmp(label,L"Restore defaults")&&!IsWindowVisible(GetDlgItem(library,resetId)),"Restore defaults is an adjustment action, hidden in the collapsed view");}
  requireDesktopActionStates();
  requireUi(GetClassLongPtrW(library,GCLP_HICON)!=0,"Application icon resource");
  requireUi(!IsWindowVisible(GetDlgItem(library,qualityId)),"Advanced controls start hidden");
  requireUi(!(GetWindowLongPtrW(library,GWL_EXSTYLE)&WS_EX_LAYERED),"Library starts fully opaque");
  const int offChoice=int(std::size(visualizations))-2,videoChoice=int(std::size(visualizations))-1;
  requireUi(SendDlgItemMessageW(library,visualizationId,CB_GETCOUNT,0,0)==videoChoice,"Built-in visualizations appear in dropdown");
  mouseSelect(visualizationId,1);requireUi(selected().scene==Scene::Orbit,"Visualization dropdown switches scene");
  requireSolarMenu();
  auto menu=visualizationMenu();requireUi(GetMenuItemCount(menu)==videoChoice-int(bodyCatalog.size()),"Visualization menu groups solar bodies");
  requireUi((GetMenuState(menu,visualizationMenuBase+1,MF_BYCOMMAND)&MF_CHECKED)!=0,"Submenu marks active visualization");DestroyMenu(menu);
  requireUi(selectVisualization(2)&&selected().scene==Scene::Mars,"Mars follows Orbit in the catalog");
  wchar_t marsLabel[80]{};GetDlgItemTextW(library,212,marsLabel,80);requireUi(wcscmp(marsLabel,L"Terrain relief")==0,"Mars terrain adjustment label");
  SendDlgItemMessageW(library,paletteId,CB_GETLBTEXT,0,reinterpret_cast<LPARAM>(marsLabel));requireUi(wcscmp(marsLabel,L"Natural Mars")==0,"Mars natural palettes");
  requireUi(IsWindowEnabled(GetDlgItem(library,reactionId))&&IsWindowEnabled(GetDlgItem(library,205)),"Mars audio and atmosphere support adjustments");
  requireUi(selectVisualization(3)&&selected().scene==Scene::Earth,"Earth follows Mars in the catalog");
  GetDlgItemTextW(library,212,marsLabel,80);requireUi(wcscmp(marsLabel,L"Cloud detail")==0,"Earth cloud adjustment label");
  SendDlgItemMessageW(library,paletteId,CB_GETLBTEXT,0,reinterpret_cast<LPARAM>(marsLabel));requireUi(wcscmp(marsLabel,L"Natural Earth")==0,"Earth natural palettes");
  requireUi(selectVisualization(4)&&selected().scene==Scene::Moon,"Moon follows Earth in the catalog");
  GetDlgItemTextW(library,214,marsLabel,80);requireUi(wcscmp(marsLabel,L"Sun angle")==0,"Moon sunlight adjustment label");
  SendDlgItemMessageW(library,paletteId,CB_GETLBTEXT,0,reinterpret_cast<LPARAM>(marsLabel));requireUi(wcscmp(marsLabel,L"Natural Moon")==0,"Moon natural palettes");
  requireUi(selectVisualization(5)&&selected().scene==Scene::Jupiter,"Jupiter follows Moon in the catalog");
  GetDlgItemTextW(library,214,marsLabel,80);requireUi(wcscmp(marsLabel,L"Cloud drift")==0,"Jupiter cloud drift adjustment label");
  SendDlgItemMessageW(library,paletteId,CB_GETLBTEXT,0,reinterpret_cast<LPARAM>(marsLabel));requireUi(wcscmp(marsLabel,L"Natural Jupiter")==0,"Jupiter natural palettes");
  requireUi(selectVisualization(6)&&selected().scene==Scene::Saturn,"Saturn follows Jupiter in the catalog");
  GetDlgItemTextW(library,214,marsLabel,80);requireUi(wcscmp(marsLabel,L"Cloud drift")==0,"Saturn cloud drift label");
  SendDlgItemMessageW(library,paletteId,CB_GETLBTEXT,0,reinterpret_cast<LPARAM>(marsLabel));requireUi(wcscmp(marsLabel,L"Natural Saturn")==0,"Saturn natural palette");
  SendMessageW(GetDlgItem(library,applyAllId),BM_CLICK,0,0);pump();requireUi(assignmentFor(settings,surfaces[1].monitor.id).scene==Scene::Saturn,"Apply to all copies Saturn");
  requireUi(selectVisualization(7)&&selected().scene==Scene::Mercury,"Mercury follows Saturn in the catalog");
  GetDlgItemTextW(library,214,marsLabel,80);requireUi(wcscmp(marsLabel,L"Sun angle")==0,"Mercury Sun angle label");
  GetDlgItemTextW(library,215,marsLabel,80);requireUi(wcscmp(marsLabel,L"Surface contrast")==0,"Mercury surface contrast label");
  SendDlgItemMessageW(library,paletteId,CB_GETLBTEXT,0,reinterpret_cast<LPARAM>(marsLabel));requireUi(wcscmp(marsLabel,L"Natural Mercury")==0,"Mercury natural palette");
  SendMessageW(GetDlgItem(library,applyAllId),BM_CLICK,0,0);pump();requireUi(assignmentFor(settings,surfaces[1].monitor.id).scene==Scene::Mercury,"Apply to all copies Mercury");
  requireUi(selectVisualization(8)&&selected().scene==Scene::Venus,"Venus follows Mercury in the catalog");
  GetDlgItemTextW(library,212,marsLabel,80);requireUi(wcscmp(marsLabel,L"Cloud detail")==0,"Venus cloud detail label");
  SendDlgItemMessageW(library,paletteId,CB_GETLBTEXT,0,reinterpret_cast<LPARAM>(marsLabel));requireUi(wcscmp(marsLabel,L"Natural Venus")==0,"Venus natural palette");
  SendMessageW(GetDlgItem(library,applyAllId),BM_CLICK,0,0);pump();requireUi(assignmentFor(settings,surfaces[1].monitor.id).scene==Scene::Venus,"Apply to all copies Venus");
  HMENU venusMenu=visualizationMenu();requireUi(GetMenuState(venusMenu,visualizationMenuBase+8,MF_BYCOMMAND)&MF_CHECKED,"Tray marks Venus");DestroyMenu(venusMenu);
  auto sceneChoice=[](Scene scene){for(int i=0;i<int(std::size(visualizations));++i)if(visualizations[i].scene==scene)return i;return -1;};
  requireUi(sceneChoice(Scene::Neptune)==9&&selectVisualization(sceneChoice(Scene::Neptune))&&selected().scene==Scene::Neptune,"Neptune can be selected");
  GetDlgItemTextW(library,213,marsLabel,80);requireUi(wcscmp(marsLabel,L"Camera drift")==0,"Neptune camera control label");
  SendDlgItemMessageW(library,paletteId,CB_GETLBTEXT,0,reinterpret_cast<LPARAM>(marsLabel));requireUi(wcscmp(marsLabel,L"Cinematic blue")==0,"Neptune cinematic palette");
  SendMessageW(GetDlgItem(library,applyAllId),BM_CLICK,0,0);pump();requireUi(assignmentFor(settings,surfaces[1].monitor.id).scene==Scene::Neptune,"Apply to all copies Neptune");
  HMENU neptuneMenu=visualizationMenu();requireUi(GetMenuState(neptuneMenu,visualizationMenuBase+sceneChoice(Scene::Neptune),MF_BYCOMMAND)&MF_CHECKED,"Tray marks Neptune");DestroyMenu(neptuneMenu);
  requireUi(sceneChoice(Scene::Uranus)==10&&selectVisualization(sceneChoice(Scene::Uranus))&&selected().scene==Scene::Uranus,"Uranus follows Neptune");
  GetDlgItemTextW(library,213,marsLabel,80);requireUi(wcscmp(marsLabel,L"Band contrast")==0,"Uranus fixed-camera control label");
  SendDlgItemMessageW(library,paletteId,CB_GETLBTEXT,0,reinterpret_cast<LPARAM>(marsLabel));requireUi(wcscmp(marsLabel,L"Natural Uranus")==0,"Uranus natural palette");
  requireUi(IsWindowEnabled(GetDlgItem(library,reactionId))&&IsWindowEnabled(GetDlgItem(library,206)),"Uranus audio controls enabled");
  SendMessageW(GetDlgItem(library,applyAllId),BM_CLICK,0,0);pump();requireUi(assignmentFor(settings,surfaces[1].monitor.id).scene==Scene::Uranus,"Apply to all copies Uranus");
  HMENU uranusMenu=visualizationMenu();requireUi(GetMenuState(uranusMenu,visualizationMenuBase+sceneChoice(Scene::Uranus),MF_BYCOMMAND)&MF_CHECKED,"Tray marks Uranus");DestroyMenu(uranusMenu);
  requireUi(sceneChoice(Scene::Pluto)==11&&selectVisualization(sceneChoice(Scene::Pluto))&&selected().scene==Scene::Pluto,"Pluto follows Uranus");
  const wchar_t* plutoLabels[]={L"Surface detail",L"Camera drift",L"Sun angle",L"Atmosphere"};
  for(int i=0;i<4;i++){GetDlgItemTextW(library,212+i,marsLabel,80);requireUi(wcscmp(marsLabel,plutoLabels[i])==0,"Pluto adjustment labels");}
  const wchar_t* plutoPalettes[]={L"Natural Pluto",L"Warm ice",L"Cool ice",L"Neutral survey"};
  for(int i=0;i<4;i++){SendDlgItemMessageW(library,paletteId,CB_GETLBTEXT,i,reinterpret_cast<LPARAM>(marsLabel));requireUi(wcscmp(marsLabel,plutoPalettes[i])==0,"Pluto palettes");}
  SendMessageW(GetDlgItem(library,applyAllId),BM_CLICK,0,0);pump();requireUi(assignmentFor(settings,surfaces[1].monitor.id).scene==Scene::Pluto,"Apply to all copies Pluto");
  HMENU plutoMenu=visualizationMenu();requireUi(GetMenuState(plutoMenu,visualizationMenuBase+sceneChoice(Scene::Pluto),MF_BYCOMMAND)&MF_CHECKED,"Tray marks Pluto");DestroyMenu(plutoMenu);
  requireUi(sceneChoice(Scene::Sun)==12&&selectVisualization(sceneChoice(Scene::Sun))&&selected().scene==Scene::Sun,"Sun follows Pluto");
  const wchar_t* sunLabels[]={L"Surface detail",L"Camera drift",L"Solar activity",L"Corona"};
  for(int i=0;i<4;i++){GetDlgItemTextW(library,212+i,marsLabel,80);requireUi(wcscmp(marsLabel,sunLabels[i])==0,"Sun adjustment labels");}
  const wchar_t* expectedSunPalettes[]={L"SDO orange (false color)",L"Golden corona (false color)",L"Deep amber (false color)",L"Neutral intensity (false color)"};
  for(int i=0;i<4;i++){SendDlgItemMessageW(library,paletteId,CB_GETLBTEXT,i,reinterpret_cast<LPARAM>(marsLabel));requireUi(wcscmp(marsLabel,expectedSunPalettes[i])==0,"Sun false-color palettes");}
  requireUi(IsWindowEnabled(GetDlgItem(library,reactionId))&&IsWindowEnabled(GetDlgItem(library,206)),"Sun audio controls enabled");
  selected().params.turbulence=1.6f;selected().params.density=.4f;
  SendMessageW(GetDlgItem(library,applyAllId),BM_CLICK,0,0);pump();auto sunAll=assignmentFor(settings,surfaces[1].monitor.id);requireUi(sunAll.scene==Scene::Sun&&sunAll.params.turbulence==1.6f&&sunAll.params.density==.4f,"Apply to all copies Sun settings");
  HMENU sunMenu=visualizationMenu();requireUi(GetMenuState(sunMenu,visualizationMenuBase+sceneChoice(Scene::Sun),MF_BYCOMMAND)&MF_CHECKED,"Tray marks Sun");DestroyMenu(sunMenu);
  selectVisualization(7);
  HMENU mercuryMenu=visualizationMenu();requireUi(GetMenuState(mercuryMenu,visualizationMenuBase+7,MF_BYCOMMAND)&MF_CHECKED,"Tray marks planet Mercury");DestroyMenu(mercuryMenu);
  requireUi(wcscmp(visualizations[sceneChoice(Scene::PlasticMetal)].name,L"Plastic metal")==0,"Metallic scene is named Plastic metal");
  requireUi(selectVisualization(sceneChoice(Scene::Smoke))&&selected().scene==Scene::Smoke,"Quick switch updates selected display");
  GetDlgItemTextW(library,212,marsLabel,80);requireUi(wcscmp(marsLabel,L"Detail")==0,"Leaving Mars restores regular adjustment labels");
  requireUi(SendDlgItemMessageW(library,visualizationId,CB_GETCURSEL,0,0)==sceneChoice(Scene::Smoke),"Quick switch synchronizes dropdown");
  requireUi(selectVisualization(sceneChoice(Scene::Bloom))&&selected().scene==Scene::Bloom,"Bloom can be selected from the catalog");
  for(int i=sceneChoice(Scene::Silk);i<randomChoice;i++)requireUi(selectVisualization(i)&&selected().scene==visualizations[i].scene,"New World can be selected from the catalog");
  requireUi(!selectVisualization(videoChoice)&&!selectVisualization(-1),"Unavailable video and invalid selections leave scene intact");
  // Real native surfaces on the test desktop prove that Off reveals the desktop
  // and affects only the selected display, including Apply to all displays.
  for(auto& surface:surfaces)surface.hwnd=CreateWindowExW(WS_EX_TOOLWINDOW,L"STATIC",L"Off test surface",WS_POPUP|WS_VISIBLE,0,0,32,32,nullptr,nullptr,instance,nullptr);
  selectVisualization(randomChoice);requireUi(selected().random&&selected().scene!=Scene::Off,"Random starts a built-in visualization");
  requireUi(IsWindowEnabled(GetDlgItem(library,randomIntervalId)),"Random interval is enabled");
  mouseSelect(randomIntervalId,0);requireUi(selected().randomInterval==30,"Random interval is configurable");
  if(argc>1){auto out=std::filesystem::path(argv[1]);std::filesystem::create_directories(out);pump();screenshot(out/L"random.bmp");}
  auto randomScene=selected().scene;advanceRandomVisualizations(29000);requireUi(selected().scene==randomScene,"Random retains scene until interval");
  pauseState.manual=true;advanceRandomVisualizations(30000);requireUi(selected().scene==randomScene,"Random respects pause");pauseState.manual=false;
  advanceRandomVisualizations(1000);requireUi(selected().scene!=randomScene&&selected().random,"Random switches after active interval");
  requireUi(SendDlgItemMessageW(library,visualizationId,CB_GETCURSEL,0,0)==randomChoice,"Random selection survives automatic switching");
  menu=visualizationMenu();requireUi(GetMenuState(menu,visualizationMenuBase+randomChoice,MF_BYCOMMAND)&MF_CHECKED,"Tray marks Random");DestroyMenu(menu);
  requireUi(IsWindowEnabled(GetDlgItem(library,bodyStatisticsId))&&!selected().showBodyStatistics,"Statistics available in general Random and off by default");
  SendMessageW(GetDlgItem(library,bodyStatisticsId),BM_CLICK,0,0);pump();requireUi(selected().showBodyStatistics,"Statistics checkbox updates assignment");
  selectVisualization(solarRandomChoice);requireUi(selected().random&&selected().randomPool==RandomPool::SolarSystem&&bodyFacts(selected().scene)&&selected().showBodyStatistics,"Solar Random preserves statistics and starts a body");
  if(argc>1){auto out=std::filesystem::path(argv[1]);pump();screenshot(out/L"solar-statistics.bmp");}
  auto solarScene=selected().scene;advanceRandomVisualizations(30000);requireUi(bodyFacts(selected().scene)&&selected().scene!=solarScene,"Solar Random advances to another body");
  requireUi(SendDlgItemMessageW(library,visualizationId,CB_GETCURSEL,0,0)==solarRandomChoice,"Solar Random stays selected after switching");
  menu=visualizationMenu();requireUi((GetMenuState(menu,visualizationMenuBase+solarRandomChoice,MF_BYCOMMAND)&MF_CHECKED)&&!(GetMenuState(menu,visualizationMenuBase+randomChoice,MF_BYCOMMAND)&MF_CHECKED),"Tray distinguishes Solar Random from general Random");DestroyMenu(menu);
  SendMessageW(GetDlgItem(library,applyAllId),BM_CLICK,0,0);pump();requireUi(assignmentFor(settings,surfaces[1].monitor.id).random,"Apply to all copies Random mode");
  auto other=assignmentFor(settings,surfaces[1].monitor.id);requireUi(other.randomPool==RandomPool::SolarSystem&&other.showBodyStatistics,"Apply to all copies pool and statistics");
  selectVisualization(sceneChoice(Scene::Earth));requireUi(selected().showBodyStatistics&&IsWindowEnabled(GetDlgItem(library,bodyStatisticsId)),"Manual body retains statistics preference");
  SendMessageW(GetDlgItem(library,resetId),BM_CLICK,0,0);pump();requireUi(!selected().showBodyStatistics,"Reset disables statistics");
  SendMessageW(GetDlgItem(library,bodyStatisticsId),BM_CLICK,0,0);pump();

  mouseSelect(visualizationId,offChoice);requireUi(selected().scene==Scene::Off&&!IsWindowVisible(surfaces[0].hwnd)&&IsWindowVisible(surfaces[1].hwnd),"Off dropdown hides only the selected display");
  menu=visualizationMenu();requireUi((GetMenuState(menu,visualizationMenuBase+offChoice,MF_BYCOMMAND)&MF_CHECKED)!=0,"Context menu marks Off checked");DestroyMenu(menu);
  requireUi(selectVisualization(0)&&IsWindowVisible(surfaces[0].hwnd),"Selecting a World restores its surface");
  requireUi(selectVisualization(offChoice)&&!IsWindowVisible(surfaces[0].hwnd),"Context menu Off hides the surface");
  SendMessageW(GetDlgItem(library,applyAllId),BM_CLICK,0,0);pump();requireUi(!IsWindowVisible(surfaces[1].hwnd)&&assignmentFor(settings,surfaces[1].monitor.id).scene==Scene::Off,"Apply to all hides every display");
  requireUi(!selected().random&&!IsWindowEnabled(GetDlgItem(library,randomIntervalId)),"Manual selection stops Random");
  requireUi(selected().showBodyStatistics&&!IsWindowEnabled(GetDlgItem(library,bodyStatisticsId)),"Ineligible scene disables checkbox while preserving preference");
  selectVisualization(0);SendMessageW(GetDlgItem(library,applyAllId),BM_CLICK,0,0);pump();requireUi(IsWindowVisible(surfaces[1].hwnd),"Apply to all restores every display");
  for(auto& surface:surfaces){DestroyWindow(surface.hwnd);surface.hwnd=nullptr;}
  mouseSelect(monitorId,1);mouseSelect(paletteId,2);requireUi(selected().params.palette==2,"Palette model changes");
  mouseSelect(reactionId,2);requireUi(selected().reaction==Reaction::Expressive,"Audio reaction model changes");
  auto otherApp=CreateWindowExW(WS_EX_TOPMOST,L"STATIC",L"Other app",WS_OVERLAPPEDWINDOW|WS_VISIBLE,10,10,200,120,nullptr,nullptr,instance,nullptr);
  auto hiddenApp=CreateWindowW(L"STATIC",L"Hidden app",WS_OVERLAPPEDWINDOW,10,10,200,120,nullptr,nullptr,instance,nullptr);
  auto wallpaper=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,L"STATIC",L"Wallpaper",WS_POPUP|WS_VISIBLE,0,0,32,32,nullptr,nullptr,instance,nullptr);
  SendMessageW(GetDlgItem(library,advancedId),BM_CLICK,0,0);pump();requireUi(IsWindowVisible(GetDlgItem(library,qualityId)),"Advanced controls expand");
  requireUi(IsIconic(otherApp),"Opening Advanced minimizes even topmost app windows");
  requireUi(IsWindowVisible(library)&&!IsIconic(library),"Opening Advanced leaves Worlds visible");
  requireUi(!IsWindowVisible(hiddenApp)&&!IsIconic(wallpaper)&&IsWindowVisible(wallpaper),"Opening Advanced preserves hidden apps and wallpaper helpers");
  ShowWindow(otherApp,SW_SHOWNOACTIVATE);
  SendMessageW(GetDlgItem(library,advancedId),BM_CLICK,0,0);pump();
  requireUi(!IsIconic(otherApp),"Collapsing Advanced does not minimize other apps");
  SendMessageW(GetDlgItem(library,advancedId),BM_CLICK,0,0);pump();
  requireUi(IsIconic(otherApp),"Every expansion reveals the desktop again");
  DestroyWindow(otherApp);DestroyWindow(hiddenApp);DestroyWindow(wallpaper);
  requireQuietSync();
  auto originalDpi=uiDpi;auto originalSide=sideExpansion;
  for(UINT dpi:{96u,144u,192u})for(bool side:{false,true}){uiDpi=dpi;sideExpansion=side;layoutLibrary();requireAdvancedLayout();}
  uiDpi=originalDpi;sideExpansion=originalSide;resizeLibrary();pump();
  auto otherPerformance=assignmentFor(settings,surfaces[0].monitor.id).performance;
  mouseSelect(qualityId,0);requireUi(selected().performance.quality==Quality::Eco,"Per-display quality model changes");mouseSelect(fpsId,1);requireUi(selected().performance.frameLimit==FrameLimit::Fps30,"Per-display FPS model changes");
  requireUi(assignmentFor(settings,surfaces[0].monitor.id).performance==otherPerformance,"Changing selected performance preserves the other display");
  selectedSurface().outputFallback=true;syncControls();requireUi(IsWindowVisible(GetDlgItem(library,retryId)),"HDR fallback exposes Retry now");selectedSurface().outputFallback=false;syncControls();
  mouseSelect(outputId,1);requireUi(selected().output==OutputPreference::Sdr,"Output selector changes selected preference");
  requireUi(assignmentFor(settings,surfaces[0].monitor.id).output==OutputPreference::Auto,"Output selection preserves other display");
  surfaces[0].monitor.refreshHz=24;selected().performance={Quality::High,FrameLimit::Fps60};syncControls();
  SendMessageW(GetDlgItem(library,applyAllId),BM_CLICK,0,0);pump();
  requireUi(assignmentFor(settings,surfaces[0].monitor.id).performance.quality==Quality::High&&assignmentFor(settings,surfaces[0].monitor.id).performance.frameLimit==otherPerformance.frameLimit,"Apply to all preserves unsupported destination FPS");
  requireUi(assignmentFor(settings,surfaces[0].monitor.id).output==OutputPreference::Sdr,"Apply to all copies Output");
  surfaces[0].monitor.refreshHz=60;

  if(SendDlgItemMessageW(library,deviceId,CB_GETCOUNT,0,0)<2){audioDevices.emplace_back(L"test-output",L"Test output");SendDlgItemMessageW(library,deviceId,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Test output"));}
  mouseSelect(deviceId,1);requireUi(!settings.audioDevice.empty(),"Output model changes");
  selected().scene=Scene::Video;selected().video=L"retained.mp4";syncControls();mouseSelect(visualizationId,0);
  menu=visualizationMenu();requireUi(GetMenuItemCount(menu)==int(std::size(visualizations)-bodyCatalog.size()),"Submenu remembers imported video after switching away");
  MENUITEMINFOW videoItem{sizeof(videoItem)};videoItem.fMask=MIIM_DATA;
  requireUi(GetMenuItemInfoW(menu,visualizationMenuBase+videoChoice,FALSE,&videoItem)&&std::wstring(reinterpret_cast<const wchar_t*>(videoItem.dwItemData))==L"Imported video","Remembered video has a submenu entry");DestroyMenu(menu);
  requireUi(selectVisualization(videoChoice)&&selected().scene==Scene::Video&&selected().video==L"retained.mp4","Context-menu switching restores remembered video");
  mouseSelect(visualizationId,0);mouseSelect(visualizationId,videoChoice);requireUi(selected().scene==Scene::Video&&selected().video==L"retained.mp4","Imported video can be reselected");mouseSelect(videoFitId,1);requireUi(selected().fit,"Video positioning changes");
  requireUi(!IsWindowEnabled(GetDlgItem(library,videoXId))&&!IsWindowEnabled(GetDlgItem(library,videoCenterId)),"Fit disables crop controls");
  mouseSelect(videoFitId,0);requireUi(IsWindowEnabled(GetDlgItem(library,videoXId)),"Fill enables crop controls");
  SendDlgItemMessageW(library,videoXId,TBM_SETPOS,TRUE,0);SendMessageW(library,WM_HSCROLL,TB_THUMBPOSITION,reinterpret_cast<LPARAM>(GetDlgItem(library,videoXId)));
  SendDlgItemMessageW(library,videoYId,TBM_SETPOS,TRUE,100);SendMessageW(library,WM_HSCROLL,TB_THUMBPOSITION,reinterpret_cast<LPARAM>(GetDlgItem(library,videoYId)));
  requireUi(selected().videoSettings.positionX==0&&selected().videoSettings.positionY==1,"Crop sliders update selected display");
  mouseSelect(videoFitId,1);requireUi(selected().videoSettings.positionX==0&&selected().videoSettings.positionY==1,"Fit retains saved crop");mouseSelect(videoFitId,0);
  mouseSelect(videoSpeedId,6);requireUi(selectedSurface().pendingVideoSpeed==2&&selected().videoSettings.speed==1,"Speed request is not persisted before native acknowledgement");
  SendMessageW(GetDlgItem(library,videoCenterId),BM_CLICK,0,0);pump();requireUi(selected().videoSettings.positionX==.5&&selected().videoSettings.positionY==.5&&selectedSurface().pendingVideoSpeed==2,"Center preserves speed request");
  SendMessageW(GetDlgItem(library,resetId),BM_CLICK,0,0);pump();requireUi(selected().video==L"retained.mp4"&&selected().scene==Scene::Video,"Reset preserves content");requireUi(selected().params.palette==0&&selected().reaction==Reaction::Off&&!selected().fit&&selected().performance.frameLimit==FrameLimit::Auto&&selected().performance.quality==Quality::Auto&&settings.audioDevice.empty(),"Reset defaults");requireUi(selected().videoSettings==VideoSettings{}&&!selectedSurface().pendingVideoSpeed,"Reset restores centered 1x video");requireUi(selected().output==OutputPreference::Auto&&assignmentFor(settings,surfaces[0].monitor.id).output==OutputPreference::Sdr,"Reset restores Auto only on selected display");
  selected().scene=Scene::Aurora;syncControls();
  auto slider=GetDlgItem(library,200);SetFocus(slider);int before=int(SendMessageW(slider,TBM_GETPOS,0,0));SendMessageW(slider,WM_KEYDOWN,VK_RIGHT,0);SendMessageW(slider,WM_KEYUP,VK_RIGHT,0);pump();requireUi(SendMessageW(slider,TBM_GETPOS,0,0)>before&&selected().params.brightness>1,"Themed slider keyboard input updates model");
  RECT channel{},thumb{};SendMessageW(slider,TBM_GETCHANNELRECT,0,reinterpret_cast<LPARAM>(&channel));SendMessageW(slider,TBM_GETTHUMBRECT,0,reinterpret_cast<LPARAM>(&thumb));
  PostMessageW(slider,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM((thumb.left+thumb.right)/2,(thumb.top+thumb.bottom)/2));pump();
  PostMessageW(slider,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(channel.left+40,(thumb.top+thumb.bottom)/2));pump();
  PostMessageW(slider,WM_LBUTTONUP,0,MAKELPARAM(channel.left+40,(thumb.top+thumb.bottom)/2));pump();requireUi(SendMessageW(slider,TBM_GETPOS,0,0)<before,"Themed slider mouse drag");
  MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(library,MONITOR_DEFAULTTONEAREST),&monitor);
  for(int i=0;i<6;i++){
   SetWindowPos(library,nullptr,monitor.rcWork.right-80,monitor.rcWork.bottom-80,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
   SendMessageW(GetDlgItem(library,advancedId),BM_CLICK,0,0);pump();RECT window{};GetWindowRect(library,&window);
   if(advancedOpen){
    BYTE alpha{};DWORD flags{};
    requireUi(GetLayeredWindowAttributes(library,nullptr,&alpha,&flags)&&alpha==179&&flags==LWA_ALPHA,"Advanced settings reveal the live World at 70 percent opacity");
    SendMessageW(library,WM_CLOSE,0,0);showLibrary();pump();
    requireUi(GetLayeredWindowAttributes(library,nullptr,&alpha,&flags)&&alpha==179,"Reopening expanded settings preserves transparency");
   }else requireUi(!(GetWindowLongPtrW(library,GWL_EXSTYLE)&WS_EX_LAYERED),"Collapsing advanced settings restores full opacity");
   requireUi(window.left>=monitor.rcWork.left&&window.top>=monitor.rcWork.top&&window.right<=monitor.rcWork.right&&window.bottom<=monitor.rcWork.bottom,"Expansion and collapse remain inside monitor work area");
   if(sideExpansion)requireUi(window.bottom-window.top<px(790),"Side expansion fits video controls within the work area");
  }

  for(int dark=0;dark<2;dark++){
   ui::refresh(library,dark);pump();
   requireFlatComboHighlight();
   for(int i=0;i<4;i++){SendMessageW(GetDlgItem(library,advancedId),BM_CLICK,0,0);pump();requireBackground();}
  }
  if(argc>1){std::filesystem::path out=argv[1];std::filesystem::create_directories(out);for(int dark=0;dark<2;dark++){ui::refresh(library,dark);pump();screenshot(out/(dark?L"advanced-dark.bmp":L"advanced-light.bmp"));}SendMessageW(GetDlgItem(library,advancedId),BM_CLICK,0,0);for(int dark=0;dark<2;dark++){ui::refresh(library,dark);pump();screenshot(out/(dark?L"simple-dark.bmp":L"simple-light.bmp"));}}
  if(argc>1){std::filesystem::path out=argv[1];
   auto button=GetDlgItem(library,advancedId);
   SendMessageW(library,WM_CHANGEUISTATE,MAKEWPARAM(UIS_CLEAR,UISF_HIDEFOCUS),0);SetFocus(button);
   for(int dark=0;dark<2;dark++){ui::refresh(library,dark);pump();screenshot(out/(dark?L"focus-dark.bmp":L"focus-light.bmp"),button);}
   for(int dark=0;dark<2;dark++){
    ui::refresh(library,dark);pump();mouseSelect(paletteId,0);
    SendDlgItemMessageW(library,paletteId,CB_SHOWDROPDOWN,TRUE,0);pump();COMBOBOXINFO info{sizeof(info)};GetComboBoxInfo(GetDlgItem(library,paletteId),&info);
    screenshot(out/(dark?L"dropdown-dark.bmp":L"dropdown-light.bmp"),info.hwndList);
    screenshot(out/(dark?L"field-dark.bmp":L"field-light.bmp"),GetDlgItem(library,paletteId));
    SendDlgItemMessageW(library,paletteId,CB_SHOWDROPDOWN,FALSE,0);
    if(!advancedOpen){SendMessageW(GetDlgItem(library,advancedId),BM_CLICK,0,0);pump();}
    auto deviceWindow=GetDlgItem(library,deviceId);
    if(SendMessageW(deviceWindow,CB_GETCOUNT,0,0)<30){for(int i=0;i<30;i++){auto name=L"Test playback device "+std::to_wstring(i);audioDevices.emplace_back(name,name);SendMessageW(deviceWindow,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));}}
    SendMessageW(deviceWindow,CB_SHOWDROPDOWN,TRUE,0);pump();GetComboBoxInfo(deviceWindow,&info);
    auto previousTop=SendMessageW(info.hwndList,LB_GETTOPINDEX,0,0);SendMessageW(info.hwndList,WM_VSCROLL,SB_LINEDOWN,0);pump();requireUi(SendMessageW(info.hwndList,LB_GETTOPINDEX,0,0)>previousTop,"Native dropdown scrollbar still scrolls");
    screenshot(out/(dark?L"scroll-dropdown-dark.bmp":L"scroll-dropdown-light.bmp"),info.hwndList);
    SendMessageW(deviceWindow,CB_SHOWDROPDOWN,FALSE,0);
    mouseSelect(paletteId,3);requireUi(selected().params.palette==3,"Mouse selection after theme change");
    auto comboWindow=GetDlgItem(library,paletteId);SetFocus(comboWindow);SendMessageW(comboWindow,WM_KEYDOWN,VK_UP,0);SendMessageW(comboWindow,WM_KEYUP,VK_UP,0);pump();requireUi(selected().params.palette==2,"Keyboard selection after theme change");
   }
  }
  for(int dark=0;dark<2;dark++){ui::refresh(library,dark);requireMenuTextRecovery();}
  if(argc>1){for(int dark=0;dark<2;dark++){ui::refresh(library,dark);captureThemedMenu(std::filesystem::path(argv[1])/(dark?L"menu-dark.bmp":L"menu-light.bmp"),false);captureThemedMenu(std::filesystem::path(argv[1])/(dark?L"submenu-dark.bmp":L"submenu-light.bmp"),true);captureThemedMenu(std::filesystem::path(argv[1])/(dark?L"solar-submenu-dark.bmp":L"solar-submenu-light.bmp"),2);}}
  requireSolarNavigation();
  requireRenderRecovery();
  if(argc>1)screenshot(std::filesystem::path(argv[1])/L"performance-status.bmp",library);
  DestroyWindow(library);library=nullptr;surfaces.clear();DeleteObject(font);font=nullptr;DeleteObject(headingFont);headingFont=nullptr;CoUninitialize();std::cout<<"All dropdowns selected with mouse messages; model updates, reset, expansion, icon and renderer recovery passed. Private desktop, no installer or user settings touched.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
