#include "import_ui.h"
#include "video.h"
#include <objbase.h>
#include <fstream>
#include <iostream>
#include <functional>
using namespace worlds;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static bool await(const std::function<bool()>& ready){
 auto deadline=GetTickCount64()+10000;
 do{MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}if(ready())return true;Sleep(10);}while(GetTickCount64()<deadline);return false;
}
int wmain(int argc,wchar_t** argv){try{
 wchar_t desktop[256]{};DWORD needed{};GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()),UOI_NAME,desktop,sizeof(desktop),&needed);
 require(std::wstring(desktop).starts_with(L"VolturaTests-"),"Use private desktop test harness");
 require(argc==3,"Provide preview and video fixture paths");CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
 auto root=std::filesystem::temp_directory_path()/(L"Voltura-import-test-"+std::to_wstring(GetCurrentProcessId()));
 require(!std::filesystem::exists(root),"Test directory already exists");std::filesystem::create_directory(root);
 WorldPackage package;package.title="Import preview test";package.description="An explicitly confirmed preset";package.attribution="Test fixture";package.assignment.scene=Scene::Orbit;
 {std::ifstream input(argv[1],std::ios::binary);package.preview.assign(std::istreambuf_iterator<char>(input),{});}
 auto file=root/L"preset.world";writePackage(file,package);
 HWND owner=CreateWindowExW(0,L"STATIC",L"Import test owner",WS_POPUP,0,0,100,100,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 bool applied=false;auto callback=[&](const Assignment& assignment){require(assignment.scene==Scene::Orbit,"Wrong preset applied");applied=true;};
 showWorldImport(owner,file,root/L"library",L"Test display",callback);
 HWND dialog=FindWindowW(L"VolturaWorldImport",nullptr);require(dialog!=nullptr,"Missing import window");
 require(await([&]{return IsWindowEnabled(GetDlgItem(dialog,IDOK))!=FALSE;}),"Validated package did not enable confirmation");
 require(!applied&&!std::filesystem::exists(root/L"library"),"Package installed before confirmation");
 require(FindWindowExW(dialog,nullptr,L"STATIC",L"Import preview test")!=nullptr,"Preview title missing");
 bool bitmap=false;EnumChildWindows(dialog,[](HWND child,LPARAM context)->BOOL{
  wchar_t type[32]{};GetClassNameW(child,type,32);
  if(!wcscmp(type,L"Static")||!wcscmp(type,L"STATIC")){if((GetWindowLongPtrW(child,GWL_STYLE)&SS_TYPEMASK)==SS_BITMAP&&SendMessageW(child,STM_GETIMAGE,IMAGE_BITMAP,0))*reinterpret_cast<bool*>(context)=true;}return TRUE;
 },reinterpret_cast<LPARAM>(&bitmap));require(bitmap,"Preview image missing");
 SendMessageW(dialog,WM_COMMAND,IDCANCEL,0);
 require(!applied&&!std::filesystem::exists(root/L"library"),"Cancel changed the library");
 showWorldImport(owner,file,root/L"library",L"Test display",callback);dialog=FindWindowW(L"VolturaWorldImport",nullptr);
 require(await([&]{return IsWindowEnabled(GetDlgItem(dialog,IDOK))!=FALSE;}),"Second preview failed");
 SendMessageW(dialog,WM_COMMAND,IDOK,0);
 require(await([&]{return !IsWindow(dialog);})&&applied,"Confirmed import did not finish");
 size_t saved=0;for(auto& item:std::filesystem::directory_iterator(root/L"library")){require(readPackage(item.path()/L"source.world").title==package.title,"Offline package did not roundtrip");saved++;}
 require(saved==1,"Unexpected number of saved packages");
 auto malformed=root/L"malformed.world";{std::ofstream out(malformed);out<<"broken";}
 showWorldImport(owner,malformed,root/L"library",L"Test display",callback);dialog=FindWindowW(L"VolturaWorldImport",nullptr);
 require(await([&]{return FindWindowExW(dialog,nullptr,L"STATIC",L"Incomplete package")!=nullptr;}),"Malformed package error missing");
 require(!IsWindowEnabled(GetDlgItem(dialog,IDOK)),"Malformed package can be applied");SendMessageW(dialog,WM_CLOSE,0,0);
 auto original=root/L"original.mp4";std::filesystem::copy_file(argv[2],original);
 Assignment videoAssignment;bool videoApplied=false;
 auto videoCallback=[&](const Assignment& assignment){videoAssignment=assignment;videoApplied=true;};
 auto videoLibrary=root/L"videos";
 showWorldImport(owner,original,videoLibrary,L"Test display",videoCallback,true);dialog=FindWindowW(L"VolturaWorldImport",nullptr);
 require(await([&]{return IsWindowEnabled(GetDlgItem(dialog,IDOK))!=FALSE;}),"Video preview failed");
 SendMessageW(dialog,WM_COMMAND,IDCANCEL,0);
 require(!videoApplied&&!std::filesystem::exists(videoLibrary),"Cancelled video import wrote library");
 showWorldImport(owner,original,videoLibrary,L"Test display",videoCallback,true);dialog=FindWindowW(L"VolturaWorldImport",nullptr);
 require(await([&]{return IsWindowEnabled(GetDlgItem(dialog,IDOK))!=FALSE;}),"Second video preview failed");
 SendMessageW(dialog,WM_COMMAND,IDOK,0);
 require(await([&]{return !IsWindow(dialog);})&&videoApplied,"Video import did not finish");
 require(videoAssignment.scene==Scene::Video&&videoAssignment.reaction==Reaction::Off,"Video assignment is incorrect");
 std::filesystem::rename(original,root/L"moved.mp4");
 require(inspectVideo(videoAssignment.video).width==320,"Managed video broke after original moved");
 require(std::filesystem::file_size(videoAssignment.video)==std::filesystem::file_size(root/L"moved.mp4"),"Managed video copy size mismatch");
 DestroyWindow(owner);std::filesystem::remove_all(root);CoUninitialize();
 std::cout<<"PASS: native preview bitmap/title, explicit confirmation, cancellation without writes, offline import, malformed-package rejection. Private desktop automation.\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
