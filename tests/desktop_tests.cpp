#include "desktop.h"
#include <iostream>
#include <functional>
#include <fstream>
#include <thread>
#include <dwmapi.h>
using namespace worlds;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static HWND window(const wchar_t* name,DWORD style,HWND parent=nullptr,int x=-1280,int y=-200,int width=3200,int height=1280){
 auto hwnd=CreateWindowExW(WS_EX_NOACTIVATE,name,L"Worlds isolated host test",style,x,y,width,height,parent,nullptr,GetModuleHandleW(nullptr),nullptr);
 require(hwnd!=nullptr,"Cannot create fixture window");return hwnd;
}
static bool await(const std::function<bool()>& ready,DWORD timeout=12000){
 auto deadline=GetTickCount64()+timeout;
 do{
  MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
  if(ready())return true;Sleep(10);
 }while(GetTickCount64()<deadline);
 return false;
}
static bool startupOpenReceived{};
static std::wstring startupImportReceived;
static LRESULT CALLBACK startupController(HWND hwnd,UINT message,WPARAM w,LPARAM l){
 if(message==WM_APP+3){startupOpenReceived=true;return 0;}
 if(message==WM_COPYDATA){auto copy=reinterpret_cast<COPYDATASTRUCT*>(l);
  if(copy&&copy->dwData==0x574F524C&&copy->lpData&&copy->cbData>=sizeof(wchar_t)){
   auto text=static_cast<const wchar_t*>(copy->lpData);size_t length=copy->cbData/sizeof(wchar_t);
   if(copy->cbData%sizeof(wchar_t)==0&&text[length-1]==0){startupImportReceived.assign(text,length-1);return TRUE;}
  }return FALSE;
 }
 return DefWindowProcW(hwnd,message,w,l);
}
static void delayedStartupHandoff(const wchar_t* executable){
 HANDLE mutex=CreateMutexW(nullptr,TRUE,L"Local\\Voltura.Worlds");
 require(mutex&&GetLastError()!=ERROR_ALREADY_EXISTS,"Close Worlds before startup handoff validation");
 WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=startupController;cls.lpszClassName=L"VolturaWorldController";
 require(RegisterClassW(&cls)!=0,"Register delayed controller fixture");
 PROCESS_INFORMATION process{};HWND controller{};
 try{
  for(bool import:{false,true}){
   startupOpenReceived=false;startupImportReceived.clear();
   auto path=std::filesystem::temp_directory_path()/L"Worlds startup handoff.world";
   std::wstring command=L"\""+std::wstring(executable)+L"\"";
   if(import)command+=L" --import \""+path.wstring()+L"\"";
   STARTUPINFOW startup{sizeof(startup)};
   require(CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process)!=FALSE,"Launch delayed handoff application");
   CloseHandle(process.hThread);process.hThread=nullptr;
   auto delayedUntil=GetTickCount64()+500;
   await([&]{return GetTickCount64()>=delayedUntil;},1000);
   require(WaitForSingleObject(process.hProcess,0)==WAIT_TIMEOUT,"Second launch discarded its request before the controller existed");
   controller=window(L"VolturaWorldController",WS_POPUP);
   require(await([&]{return import?startupImportReceived==path.wstring():startupOpenReceived;}),"Delayed controller did not receive launch request");
   require(await([&]{return WaitForSingleObject(process.hProcess,0)==WAIT_OBJECT_0;}),"Handoff process did not exit");
   DWORD code=1;GetExitCodeProcess(process.hProcess,&code);require(code==0,"Handoff process returned failure");
   CloseHandle(process.hProcess);process.hProcess=nullptr;DestroyWindow(controller);controller=nullptr;
  }
 }catch(...){if(process.hProcess){TerminateProcess(process.hProcess,1);WaitForSingleObject(process.hProcess,5000);CloseHandle(process.hProcess);}if(controller)DestroyWindow(controller);ReleaseMutex(mutex);CloseHandle(mutex);UnregisterClassW(cls.lpszClassName,cls.hInstance);throw;}
 ReleaseMutex(mutex);CloseHandle(mutex);UnregisterClassW(cls.lpszClassName,cls.hInstance);
}
static void normalStartup(const wchar_t* executable){
 PROCESS_INFORMATION process{};STARTUPINFOW startup{sizeof(startup)};
 auto command=L"\""+std::wstring(executable)+L"\"";
 require(CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process)!=FALSE,"Launch normal application");
 CloseHandle(process.hThread);
 try{
  HWND library{},controller{};
  require(await([&]{library=FindWindowW(L"VolturaWorldLibrary",nullptr);return library&&IsWindowVisible(library)&&!IsIconic(library);}),"Normal launch must show the library");
  // The private desktop is not the input desktop, so GetForegroundWindow is
  // unavailable there. Verify the application's active window instead.
  require(await([&]{GUITHREADINFO gui{sizeof(gui)};return GetGUIThreadInfo(GetWindowThreadProcessId(library,nullptr),&gui)&&gui.hwndActive==library;}),"Normal launch must activate the library");
  require(await([&]{controller=FindWindowW(L"VolturaWorldController",nullptr);return controller!=nullptr;}),"Normal launch controller");
  PostMessageW(controller,WM_CLOSE,0,0);
  require(await([&]{return WaitForSingleObject(process.hProcess,0)==WAIT_OBJECT_0;}),"Normal launch exits cleanly");
 }catch(...){TerminateProcess(process.hProcess,1);WaitForSingleObject(process.hProcess,5000);CloseHandle(process.hProcess);throw;}
 CloseHandle(process.hProcess);
}
static void applicationRecovery(const wchar_t* executable){
 HANDLE existing=OpenMutexW(SYNCHRONIZE,FALSE,L"Local\\Voltura.Worlds");
 if(existing){CloseHandle(existing);throw std::runtime_error("Close Worlds before running isolated recovery validation");}
 auto progman=window(L"Progman",WS_POPUP);
 window(L"SHELLDLL_DefView",WS_CHILD,progman,0,0);
 // Preserve an abandoned startup mutex until the child claims it. This
 // models a process whose owning UI thread died before creating a controller.
 HANDLE abandoned{};
 std::thread failedOwner([&]{abandoned=CreateMutexW(nullptr,TRUE,L"Local\\Voltura.Worlds");});failedOwner.join();
 require(abandoned!=nullptr,"Create abandoned startup fixture");
 PROCESS_INFORMATION process{};STARTUPINFOW startup{sizeof(startup)};
 std::wstring command=L"\""+std::wstring(executable)+L"\" --wallpaper";
 require(CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process)!=FALSE,"Launch recovery test application");
 CloseHandle(process.hThread);
 HWND worker{};
 try{
  HWND controller{};
  require(await([&]{controller=FindWindowW(L"VolturaWorldController",nullptr);return controller!=nullptr;}),"Application controller did not start");
  CloseHandle(abandoned);abandoned=nullptr;
  require(WaitForInputIdle(process.hProcess,10000)==0,"Background launch became idle");
  require(!FindWindowW(L"VolturaWorldLibrary",nullptr),"Windows startup must stay hidden even when the desktop host is unavailable");
  require(!FindWindowW(L"VolturaWorldSurface",nullptr),"Failed attachment left a floating artwork window");
  worker=window(L"WorkerW",WS_CHILD,progman,0,0);
  require(await([&]{return FindWindowExW(worker,nullptr,L"VolturaWorldSurface",nullptr)!=nullptr;}),"Application did not automatically attach when host appeared");
  require(!FindWindowW(L"VolturaWorldLibrary",nullptr),"Automatic host recovery must stay hidden");
  PROCESS_INFORMATION open{};STARTUPINFOW openStartup{sizeof(openStartup)};
  auto openCommand=L"\""+std::wstring(executable)+L"\"";
  require(CreateProcessW(executable,openCommand.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&openStartup,&open)!=FALSE,"User opens background instance");
  CloseHandle(open.hThread);
  bool opened=await([]{auto library=FindWindowW(L"VolturaWorldLibrary",nullptr);return library&&IsWindowVisible(library)&&!IsIconic(library);});
  bool exited=await([&]{return WaitForSingleObject(open.hProcess,0)==WAIT_OBJECT_0;});CloseHandle(open.hProcess);
  require(opened&&exited,"User launch must open the existing background instance");
  // Exercise the real file-association command against the running instance.
  auto invalid=std::filesystem::temp_directory_path()/(L"Worlds-handoff-"+std::to_wstring(GetCurrentProcessId())+L".world");
  require(!std::filesystem::exists(invalid),"Handoff fixture already exists");
  {std::ofstream file(invalid);file<<"invalid package";}
  PROCESS_INFORMATION second{};STARTUPINFOW secondStartup{sizeof(secondStartup)};
  auto secondCommand=L"\""+std::wstring(executable)+L"\" --import \""+invalid.wstring()+L"\"";
  require(CreateProcessW(executable,secondCommand.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&secondStartup,&second)!=FALSE,"Launch file handoff instance");
  CloseHandle(second.hThread);
  bool handed=await([&]{return WaitForSingleObject(second.hProcess,0)==WAIT_OBJECT_0;});
  DWORD secondCode=1;if(handed)GetExitCodeProcess(second.hProcess,&secondCode);else TerminateProcess(second.hProcess,1);
  CloseHandle(second.hProcess);require(handed&&secondCode==0,"Import handoff failed");
  HWND import{};require(await([&]{import=FindWindowW(L"VolturaWorldImport",nullptr);return import&&FindWindowExW(import,nullptr,L"STATIC",L"Incomplete package");}),"Running instance did not show validated import preview/error");
  require(!IsWindowEnabled(GetDlgItem(import,IDOK)),"Malformed handed-off package can be applied");
  PostMessageW(import,WM_CLOSE,0,0);require(await([&]{return !IsWindow(import);}),"Import window did not close");
  std::filesystem::remove(invalid);
  DestroyWindow(worker);worker=nullptr;
  // Keep the host absent long enough for a retry, then restore it. This is a
  // controlled stand-in for shell host loss, not an actual Explorer restart.
  auto delay=GetTickCount64()+5500;
  await([&]{return GetTickCount64()>=delay;},6500);
  worker=window(L"WorkerW",WS_CHILD,progman,0,0);
  require(await([&]{return FindWindowExW(worker,nullptr,L"VolturaWorldSurface",nullptr)!=nullptr;}),"Application did not recover after host destruction");
  PostMessageW(controller,WM_CLOSE,0,0);
  require(await([&]{return WaitForSingleObject(process.hProcess,0)==WAIT_OBJECT_0;}),"Application did not shut down cleanly");
  DWORD code{};GetExitCodeProcess(process.hProcess,&code);require(code==0,"Application recovery exited with an error");
  require(!FindWindowExW(worker,nullptr,L"VolturaWorldSurface",nullptr),"Exit left an artwork surface behind");
 }catch(...){TerminateProcess(process.hProcess,1);WaitForSingleObject(process.hProcess,5000);CloseHandle(process.hProcess);if(abandoned)CloseHandle(abandoned);DestroyWindow(progman);throw;}
 CloseHandle(process.hProcess);DestroyWindow(progman);
}
int wmain(int argc,wchar_t** argv){try{
 // Match the application's per-monitor pixel coordinates. DWM frame bounds
 // are physical pixels even when a test process is otherwise DPI-virtualized.
 require(SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)!=FALSE,"Set test DPI awareness");
 wchar_t desktop[256]{};DWORD needed{};
 require(GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()),UOI_NAME,desktop,sizeof(desktop),&needed)!=FALSE,"Read desktop name");
 require(std::wstring(desktop).starts_with(L"VolturaTests-"),"Desktop tests must run through scripts/test_desktop.py");
 for(auto name:{L"Progman",L"WorkerW",L"SHELLDLL_DefView",L"VolturaHostTestSurface",L"VolturaCoverageTest"}){
  WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=DefWindowProcW;cls.lpszClassName=name;
  require(RegisterClassW(&cls)!=0,"Register fixture class");
 }
 require(!findWallpaperParent(),"Missing shell must leave normal wallpaper alone");
 auto progman=window(L"Progman",WS_POPUP);
 window(L"SHELLDLL_DefView",WS_CHILD,progman,0,0);
 auto worker=window(L"WorkerW",WS_POPUP);
 require(SetWindowPos(worker,progman,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE)!=FALSE,"Set classic shell ordering");
 require(findWallpaperParent()==worker,"Classic shell host discovery failed");
 Monitor monitor{};monitor.rect={-1280,-200,0,520};
 auto surface=window(L"VolturaHostTestSurface",WS_POPUP);
 require(!attachWallpaper(surface,monitor,progman),"Attached directly over desktop icons");
 require(attachWallpaper(surface,monitor,worker),"Attach classic host");
 require(wallpaperAttached(surface),"Attached surface not recognized");
 require(attachWallpaper(surface,monitor,worker,false)&&!IsWindowVisible(surface)&&wallpaperAttached(surface),"Off startup must attach without showing a wallpaper surface");
 require(attachWallpaper(surface,monitor,worker)&&(GetWindowLongPtrW(surface,GWL_STYLE)&WS_VISIBLE),"Active scene must restore the attached surface");
 RECT rect{};GetWindowRect(surface,&rect);
 require(EqualRect(&rect,&monitor.rect)!=FALSE,"Negative-coordinate monitor placement changed");
 require((GetWindowLongPtrW(surface,GWL_STYLE)&WS_CHILD)!=0,"Wallpaper is not a child");
 require((GetWindowLongPtrW(surface,GWL_EXSTYLE)&WS_EX_NOACTIVATE)!=0,"Wallpaper can activate");
 DestroyWindow(worker);
 require(!wallpaperAttached(surface),"Destroyed shell host still appears attached");
 require(!findWallpaperParent(),"Missing worker host not detected");
 // Explorer's alternate hierarchy: background WorkerW is a Progman child.
 // Repeated host destruction/recreation checks stale-handle detection.
 for(int iteration=0;iteration<8;iteration++){
  worker=window(L"WorkerW",WS_CHILD,progman,0,0);
  require(findWallpaperParent()==worker,"Nested shell host discovery failed");
  surface=window(L"VolturaHostTestSurface",WS_POPUP);
  auto icons=window(L"SHELLDLL_DefView",WS_CHILD,worker,0,0);
  require(!attachWallpaper(surface,monitor,worker),"Icon-bearing worker accepted");
  DestroyWindow(icons);
  require(attachWallpaper(surface,monitor,worker)&&wallpaperAttached(surface),"Recreated host attachment failed");
  DestroyWindow(worker);
  require(!wallpaperAttached(surface),"Stale surface accepted after host destruction");
 }
 DestroyWindow(progman);
 Monitor covered{};covered.rect={0,0,640,480};
 auto left=window(L"VolturaCoverageTest",WS_POPUP,nullptr,0,0,320,480);
 auto right=window(L"VolturaCoverageTest",WS_POPUP,nullptr,320,0,320,480);
 ShowWindow(left,SW_SHOWNOACTIVATE);
 if(monitorCovered(covered,0)){
  EnumWindows([](HWND hwnd,LPARAM)->BOOL{
   if(!IsWindowVisible(hwnd))return TRUE;wchar_t name[128]{};GetClassNameW(hwnd,name,128);
   RECT bounds{},frame{};GetWindowRect(hwnd,&bounds);auto result=DwmGetWindowAttribute(hwnd,DWMWA_EXTENDED_FRAME_BOUNDS,&frame,sizeof(frame));
   std::wcerr<<name<<L" bounds "<<bounds.left<<L","<<bounds.top<<L","<<bounds.right<<L","<<bounds.bottom<<L" frame "<<frame.left<<L","<<frame.top<<L","<<frame.right<<L","<<frame.bottom<<L" hr "<<result<<L'\n';return TRUE;
  },0);
  require(false,"Partially covered monitor paused");
 }
 ShowWindow(right,SW_SHOWNOACTIVATE);
 require(monitorCovered(covered,0),"Combined opaque coverage missed");
 require(!monitorCovered(covered,GetCurrentProcessId()),"Own windows counted as external coverage");
 ShowWindow(right,SW_HIDE);
 require(!monitorCovered(covered,0),"Hidden window counted as coverage");
 DestroyWindow(left);DestroyWindow(right);
 require(argc==2,"Pass the Worlds executable for application recovery validation");
 delayedStartupHandoff(argv[1]);
 normalStartup(argv[1]);
 applicationRecovery(argv[1]);
 std::cout<<"PASS: absent/classic/nested host, icon-host rejection, negative monitor coordinates, repeated host destruction and reattachment, combined coverage and hidden/own-window exclusions, actual application delayed-host retry/recovery/exit and single-instance file handoff. Private synthetic Windows desktop, not Explorer or physical-display proof.\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
