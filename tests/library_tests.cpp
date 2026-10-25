// Exercise the real library window, renderer recovery and notification handlers
// without tray registration, installer, user's settings, or changes to the real desktop.
#define wWinMain unusedApplicationEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>
void requireUi(bool value,const char* text){if(!value)throw std::runtime_error(text);}
void pump(){for(int i=0;i<10;i++){MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}Sleep(5);}}
void requireRenderRecovery(){
 WNDCLASSW cls{};cls.hInstance=instance;cls.lpszClassName=L"VolturaRenderRecoveryTest";
 cls.lpfnWndProc=[](HWND hwnd,UINT msg,WPARAM w,LPARAM l)->LRESULT{return msg==renderStatusMessage?controllerProc(hwnd,msg,w,l):DefWindowProcW(hwnd,msg,w,l);};
 RegisterClassW(&cls);controller=CreateWindowW(cls.lpszClassName,L"",WS_POPUP,0,0,1,1,nullptr,nullptr,instance,nullptr);
 for(auto& surface:surfaces)surface.hwnd=CreateWindowW(L"STATIC",L"",WS_POPUP,0,0,160,90,nullptr,nullptr,instance,nullptr);
 exeDir=executable().parent_path();pauseState={};settings.audioDevice.clear();
 {std::lock_guard lock(stateMutex);
  Assignment bad;bad.scene=Scene::Video;bad.video=(exeDir/L"missing-recovery-test.mp4").wstring();
  Assignment good;good.scene=Scene::Silk;settings.displays={{surfaces[0].monitor.id,bad},{surfaces[1].monitor.id,good}};}
 auto await=[&](auto condition){auto deadline=GetTickCount64()+15000;while(GetTickCount64()<deadline){pump();if(condition())return true;}return false;};
 startRendering();
 bool failed=await([]{return renderingProblem;});
 {std::lock_guard lock(stateMutex);settings.displays[0].second.scene=Scene::Silk;}
 bool recovered=await([]{return !renderingProblem;});
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
 int contentHeight=sideExpansion?572:824;
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
void captureThemedMenu(const std::filesystem::path& path,bool submenu){
 WNDCLASSW cls{};cls.hInstance=instance;cls.lpfnWndProc=[](HWND hwnd,UINT msg,WPARAM w,LPARAM l)->LRESULT{if(msg==WM_DESTROY)return 0;return controllerProc(hwnd,msg,w,l);};cls.lpszClassName=L"VolturaMenuTestOwner";RegisterClassW(&cls);
 HWND owner=CreateWindowW(cls.lpszClassName,L"Menu test",WS_POPUP,0,0,0,0,nullptr,nullptr,instance,nullptr);
 HMENU menu=submenu?visualizationMenu():CreatePopupMenu();
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
 if(hook)UnhookWindowsHookEx(hook);DeleteObject(menuFont);menuFont=nullptr;DestroyMenu(menu);DestroyWindow(owner);
 requireUi(nativeMenuScaleCorrect,"Native popup row height and rendered text must scale together");
}
int wmain(int argc,wchar_t** argv){
 try{
  instance=GetModuleHandleW(nullptr);CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  INITCOMMONCONTROLSEX common{sizeof(common),ICC_STANDARD_CLASSES|ICC_BAR_CLASSES};InitCommonControlsEx(&common);ui::refresh();
  WNDCLASSW cls{};cls.hInstance=instance;cls.lpfnWndProc=libraryProc;cls.lpszClassName=L"VolturaWorldLibrary";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(101));RegisterClassW(&cls);
  Surface first{},second{};first.monitor.id=L"test-one";first.monitor.name=L"Display 1";second.monitor.id=L"test-two";second.monitor.name=L"Display 2";surfaces={first,second};showLibrary();pump();
  requireDesktopActionStates();
  requireUi(GetClassLongPtrW(library,GCLP_HICON)!=0,"Application icon resource");
  requireUi(!IsWindowVisible(GetDlgItem(library,qualityId)),"Advanced controls start hidden");
  requireUi(!(GetWindowLongPtrW(library,GWL_EXSTYLE)&WS_EX_LAYERED),"Library starts fully opaque");
  const int offChoice=int(std::size(visualizations))-2,videoChoice=int(std::size(visualizations))-1;
  requireUi(SendDlgItemMessageW(library,visualizationId,CB_GETCOUNT,0,0)==videoChoice,"Built-in visualizations appear in dropdown");
  mouseSelect(visualizationId,1);requireUi(selected().scene==Scene::Orbit,"Visualization dropdown switches scene");
  auto menu=visualizationMenu();requireUi(GetMenuItemCount(menu)==videoChoice,"Visualization submenu contains catalog");
  requireUi((GetMenuState(menu,visualizationMenuBase+1,MF_BYCOMMAND)&MF_CHECKED)!=0,"Submenu marks active visualization");DestroyMenu(menu);
  requireUi(selectVisualization(2)&&selected().scene==Scene::Smoke,"Quick switch updates selected display");
  requireUi(SendDlgItemMessageW(library,visualizationId,CB_GETCURSEL,0,0)==2,"Quick switch synchronizes dropdown");
  requireUi(selectVisualization(3)&&selected().scene==Scene::Bloom,"Bloom can be selected from the catalog");
  for(int i=4;i<randomChoice;i++)requireUi(selectVisualization(i)&&selected().scene==visualizations[i].scene,"New World can be selected from the catalog");
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
  SendMessageW(GetDlgItem(library,applyAllId),BM_CLICK,0,0);pump();requireUi(assignmentFor(settings,surfaces[1].monitor.id).random,"Apply to all copies Random mode");
  mouseSelect(visualizationId,offChoice);requireUi(selected().scene==Scene::Off&&!IsWindowVisible(surfaces[0].hwnd)&&IsWindowVisible(surfaces[1].hwnd),"Off dropdown hides only the selected display");
  menu=visualizationMenu();requireUi((GetMenuState(menu,visualizationMenuBase+offChoice,MF_BYCOMMAND)&MF_CHECKED)!=0,"Context menu marks Off checked");DestroyMenu(menu);
  requireUi(selectVisualization(0)&&IsWindowVisible(surfaces[0].hwnd),"Selecting a World restores its surface");
  requireUi(selectVisualization(offChoice)&&!IsWindowVisible(surfaces[0].hwnd),"Context menu Off hides the surface");
  SendMessageW(GetDlgItem(library,applyAllId),BM_CLICK,0,0);pump();requireUi(!IsWindowVisible(surfaces[1].hwnd)&&assignmentFor(settings,surfaces[1].monitor.id).scene==Scene::Off,"Apply to all hides every display");
  requireUi(!selected().random&&!IsWindowEnabled(GetDlgItem(library,randomIntervalId)),"Manual selection stops Random");
  selectVisualization(0);SendMessageW(GetDlgItem(library,applyAllId),BM_CLICK,0,0);pump();requireUi(IsWindowVisible(surfaces[1].hwnd),"Apply to all restores every display");
  for(auto& surface:surfaces){DestroyWindow(surface.hwnd);surface.hwnd=nullptr;}
  mouseSelect(monitorId,1);mouseSelect(paletteId,2);requireUi(selected().params.palette==2,"Palette model changes");
  mouseSelect(reactionId,2);requireUi(selected().reaction==Reaction::Expressive,"Audio reaction model changes");
  SendMessageW(GetDlgItem(library,advancedId),BM_CLICK,0,0);pump();requireUi(IsWindowVisible(GetDlgItem(library,qualityId)),"Advanced controls expand");
  requireQuietSync();
  auto originalDpi=uiDpi;auto originalSide=sideExpansion;
  for(UINT dpi:{96u,144u,192u})for(bool side:{false,true}){uiDpi=dpi;sideExpansion=side;layoutLibrary();requireAdvancedLayout();}
  uiDpi=originalDpi;sideExpansion=originalSide;resizeLibrary();pump();
  mouseSelect(qualityId,0);requireUi(settings.quality==Quality::Eco,"Quality model changes");mouseSelect(fpsId,0);requireUi(settings.fps==30,"FPS model changes");
  if(SendDlgItemMessageW(library,deviceId,CB_GETCOUNT,0,0)<2){audioDevices.emplace_back(L"test-output",L"Test output");SendDlgItemMessageW(library,deviceId,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Test output"));}
  mouseSelect(deviceId,1);requireUi(!settings.audioDevice.empty(),"Output model changes");
  selected().scene=Scene::Video;selected().video=L"retained.mp4";syncControls();mouseSelect(visualizationId,0);
  menu=visualizationMenu();requireUi(GetMenuItemCount(menu)==int(std::size(visualizations)),"Submenu remembers imported video after switching away");
  MENUITEMINFOW videoItem{sizeof(videoItem)};videoItem.fMask=MIIM_DATA;
  requireUi(GetMenuItemInfoW(menu,visualizationMenuBase+videoChoice,FALSE,&videoItem)&&std::wstring(reinterpret_cast<const wchar_t*>(videoItem.dwItemData))==L"Imported video","Remembered video has a submenu entry");DestroyMenu(menu);
  requireUi(selectVisualization(videoChoice)&&selected().scene==Scene::Video&&selected().video==L"retained.mp4","Context-menu switching restores remembered video");
  mouseSelect(visualizationId,0);mouseSelect(visualizationId,videoChoice);requireUi(selected().scene==Scene::Video&&selected().video==L"retained.mp4","Imported video can be reselected");mouseSelect(videoFitId,1);requireUi(selected().fit,"Video positioning changes");
  SendMessageW(GetDlgItem(library,resetId),BM_CLICK,0,0);pump();requireUi(selected().video==L"retained.mp4"&&selected().scene==Scene::Video,"Reset preserves content");requireUi(selected().params.palette==0&&selected().reaction==Reaction::Off&&!selected().fit&&settings.fps==60&&settings.quality==Quality::Auto&&settings.audioDevice.empty(),"Reset defaults");
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
   if(sideExpansion)requireUi(window.bottom-window.top<px(640),"Side expansion keeps window compact");
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
  if(argc>1){for(int dark=0;dark<2;dark++){ui::refresh(library,dark);captureThemedMenu(std::filesystem::path(argv[1])/(dark?L"menu-dark.bmp":L"menu-light.bmp"),false);captureThemedMenu(std::filesystem::path(argv[1])/(dark?L"submenu-dark.bmp":L"submenu-light.bmp"),true);}}
  requireRenderRecovery();
  DestroyWindow(library);library=nullptr;surfaces.clear();DeleteObject(font);font=nullptr;DeleteObject(headingFont);headingFont=nullptr;CoUninitialize();std::cout<<"All dropdowns selected with mouse messages; model updates, reset, expansion, icon and renderer recovery passed. Private desktop, no installer or user settings touched.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
