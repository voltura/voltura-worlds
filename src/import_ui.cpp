#include "import_ui.h"
#include "video.h"
#include "ui_theme.h"
#include <wincodec.h>
#include <wrl/client.h>
#include <thread>
#include <memory>
#include <fstream>
#include <algorithm>
namespace worlds {
namespace {
constexpr UINT ready=WM_APP+20;
HWND activeImport{};
UINT_PTR nextImportIdentity=0;
struct RetiredImport {std::shared_ptr<std::atomic_bool> finished;std::jthread thread;};
std::vector<RetiredImport> retiredImports;
struct Import {
 UINT_PTR identity=++nextImportIdentity;
 HWND window{},image{},title{},description{},status{},button{},cancel{};HFONT font{};HBITMAP bitmap{},displayBitmap{};
 std::filesystem::path file,library,temporaryVideo,installedDirectory;std::wstring destination,error;bool personalVideo=false,applied=false;LUID adapter{};
 std::function<void(const Assignment&)> apply;
 WorldPackage package;Assignment installed;std::jthread worker;bool installing=false;
 ~Import(){worker.request_stop();if(worker.joinable())worker.join();if(!applied&&!installedDirectory.empty()&&installedDirectory.parent_path()==library){std::error_code ec;std::filesystem::remove_all(installedDirectory,ec);}if(!temporaryVideo.empty()){std::error_code cleanupError;std::filesystem::remove(temporaryVideo,cleanupError);}if(displayBitmap&&displayBitmap!=bitmap)DeleteObject(displayBitmap);if(bitmap)DeleteObject(bitmap);if(font)DeleteObject(font);}
};
void layout(Import& state){
 UINT dpi=GetDpiForWindow(state.window);auto px=[dpi](int value){return MulDiv(value,dpi,96);};
 RECT client{};GetClientRect(state.window,&client);int width=client.right-px(40),bottom=client.bottom-px(20);
 int imageHeight=std::max(px(60),bottom-px(310));
 auto place=[&](HWND child,int x,int y,int w,int h){SetWindowPos(child,nullptr,x,y,w,h,SWP_NOZORDER|SWP_NOACTIVATE);};
 place(state.image,px(20),px(16),width,imageHeight);
 place(state.title,px(20),imageHeight+px(28),width,px(28));
 place(state.description,px(20),imageHeight+px(64),width,px(116));
 place(state.status,px(20),imageHeight+px(194),width,px(60));
 place(state.button,client.right-px(374),bottom-px(34),px(190),px(34));
 place(state.cancel,client.right-px(168),bottom-px(34),px(148),px(34));
 HFONT font=CreateFontW(-px(16),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 for(HWND child:{state.title,state.description,state.status,state.button,state.cancel})SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
 if(state.font)DeleteObject(state.font);state.font=font;
}
void theme(Import& state){
 ui::refresh(state.window);
 for(HWND child:{state.image,state.title,state.description,state.status,state.button,state.cancel})SetWindowTheme(child,ui::dark?L"DarkMode_Explorer":L"Explorer",nullptr);
}
void sizeToWorkArea(HWND hwnd,UINT dpi){
 MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&monitor);
 RECT r{0,0,MulDiv(580,dpi,96),MulDiv(606,dpi,96)};
 AdjustWindowRectExForDpi(&r,DWORD(GetWindowLongPtrW(hwnd,GWL_STYLE)),FALSE,WS_EX_DLGMODALFRAME,dpi);
 const auto& work=monitor.rcWork;int width=std::min(int(r.right-r.left),int(work.right-work.left)),height=std::min(int(r.bottom-r.top),int(work.bottom-work.top));
 SetWindowPos(hwnd,nullptr,work.left+(work.right-work.left-width)/2,work.top+(work.bottom-work.top-height)/2,width,height,SWP_NOZORDER|SWP_NOACTIVATE);
}
void checked(HRESULT result){if(FAILED(result))throw std::runtime_error("Cannot decode World preview");}
HBITMAP preview(const std::vector<unsigned char>& bytes){
 using Microsoft::WRL::ComPtr;
 ComPtr<IWICImagingFactory> factory;checked(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
 ComPtr<IWICStream> stream;checked(factory->CreateStream(&stream));checked(stream->InitializeFromMemory(const_cast<BYTE*>(bytes.data()),static_cast<DWORD>(bytes.size())));
 ComPtr<IWICBitmapDecoder> decoder;checked(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder));
 ComPtr<IWICBitmapFrameDecode> frame;checked(decoder->GetFrame(0,&frame));UINT w{},h{};checked(frame->GetSize(&w,&h));
 double scale=std::min(540.0/w,280.0/h);w=std::max(1u,UINT(w*scale));h=std::max(1u,UINT(h*scale));
 ComPtr<IWICBitmapScaler> scaler;checked(factory->CreateBitmapScaler(&scaler));checked(scaler->Initialize(frame.Get(),w,h,WICBitmapInterpolationModeFant));
 ComPtr<IWICFormatConverter> converter;checked(factory->CreateFormatConverter(&converter));checked(converter->Initialize(scaler.Get(),GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
 BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-int(h);info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
 void* pixels{};HBITMAP bitmap=CreateDIBSection(nullptr,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
 if(!bitmap)throw std::runtime_error("Cannot allocate preview image");
 HRESULT result=converter->CopyPixels(nullptr,w*4,w*h*4,static_cast<BYTE*>(pixels));if(FAILED(result)){DeleteObject(bitmap);checked(result);}return bitmap;
}
void work(Import& state){
 try{state.worker=std::jthread([&state](std::stop_token stop){
  HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
  try{
   if(FAILED(initialized))throw std::runtime_error("Cannot initialize import worker");
   if(state.installing){
    if(state.personalVideo){state.installed=state.package.assignment;auto video=copyVideoToLibrary(state.library,state.file,stop,state.adapter);state.installedDirectory=video.parent_path();state.installed.video=video.wstring();}
    else state.installed=installPackage(state.library,state.package,state.installedDirectory);
   }else{
    if(state.personalVideo){
     state.package.preview=videoPreview(state.file,stop,state.adapter);state.package.title=utf8(state.file.stem().wstring());if(state.package.title.size()>120)state.package.title="Personal video";
     state.package.description="Personal video. Muted, looping, and copied into your offline library.";state.package.assignment.scene=Scene::Video;
    }else{
     state.package=readPackage(state.file);
     if(state.package.assignment.scene==Scene::Video){
      GUID id{};checked(CoCreateGuid(&id));wchar_t name[40]{};StringFromGUID2(id,name,40);auto file=std::filesystem::temp_directory_path()/(L"Voltura-preview-"+std::wstring(name)+L".mp4");
      HANDLE output=CreateFileW(file.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_TEMPORARY,nullptr);if(output==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot validate video preview");state.temporaryVideo=file;
      DWORD written{};BOOL saved=WriteFile(output,state.package.video.data(),DWORD(state.package.video.size()),&written,nullptr);CloseHandle(output);if(!saved||written!=state.package.video.size())throw std::runtime_error("Cannot stage video validation");preflightVideo(file,stop,state.adapter);
     }
    }
    if(state.package.assignment.scene==Scene::Video)state.package.assignment.reaction=Reaction::Off;
    state.bitmap=preview(state.package.preview);
   }
  }catch(const std::exception& error){try{state.error=describeError(error);}catch(...){state.error=L"Cannot read this World.";}}
  if(SUCCEEDED(initialized))CoUninitialize();if(!stop.stop_requested())PostMessageW(state.window,ready,state.identity,0);
 });}catch(const std::exception& error){state.error=describeError(error);PostMessageW(state.window,ready,state.identity,0);}
}
LRESULT CALLBACK procedure(HWND hwnd,UINT message,WPARAM w,LPARAM l){
 auto state=reinterpret_cast<Import*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
 if(!state)return DefWindowProcW(hwnd,message,w,l);
 if(message==WM_SIZE){if(state->cancel)layout(*state);return 0;}
 if(message==WM_DPICHANGED){auto r=reinterpret_cast<RECT*>(l);SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);sizeToWorkArea(hwnd,HIWORD(w));return 0;}
 if(message==WM_SETTINGCHANGE||message==WM_THEMECHANGED){theme(*state);return 0;}
 if(message==WM_ERASEBKGND){RECT r{};GetClientRect(hwnd,&r);FillRect(reinterpret_cast<HDC>(w),&r,ui::backgroundBrush);return 1;}
 if(message==WM_CTLCOLORSTATIC||message==WM_CTLCOLOREDIT||message==WM_CTLCOLORBTN){bool description=reinterpret_cast<HWND>(l)==state->description;SetTextColor(reinterpret_cast<HDC>(w),ui::foreground);SetBkColor(reinterpret_cast<HDC>(w),description?ui::surface:ui::background);return reinterpret_cast<LRESULT>(description?ui::surfaceBrush:ui::backgroundBrush);}
 if(message==WM_DRAWITEM){ui::draw(*reinterpret_cast<DRAWITEMSTRUCT*>(l),state->font);return TRUE;}
 if(message==ready){
  if(w!=state->identity)return 0;
  if(state->worker.joinable())state->worker.join();
  if(!state->error.empty()){SetWindowTextW(state->status,state->error.c_str());return 0;}
  // The callback may already have changed settings before reporting a UI error.
  // Retain the files once application begins; cancellation before this point
  // still removes this import's unused managed directory.
  if(state->installing){state->applied=true;try{state->apply(state->installed);}catch(const std::exception& e){MessageBoxW(hwnd,describeError(e).c_str(),L"Cannot apply World",MB_OK|MB_ICONWARNING);}DestroyWindow(hwnd);return 0;}
  SendMessageW(state->image,STM_SETIMAGE,IMAGE_BITMAP,reinterpret_cast<LPARAM>(state->bitmap));
  state->displayBitmap=reinterpret_cast<HBITMAP>(SendMessageW(state->image,STM_GETIMAGE,IMAGE_BITMAP,0));
  layout(*state);
  SetWindowTextW(state->title,wide(state->package.title).c_str());
  auto text=wide(state->package.description);if(!state->package.attribution.empty())text+=L"\r\n\r\nCredit: "+wide(state->package.attribution);
  SetWindowTextW(state->description,text.c_str());
  auto reaction=state->package.assignment.reaction;
  auto status=L"Apply "+wide(sceneName(state->package.assignment.scene))+L" to "+state->destination+L". Audio: "+(reaction==Reaction::Off?L"Off":reaction==Reaction::Gentle?L"Gentle":L"Expressive")+L".";
  if(state->package.assignment.scene==Scene::Video)status+=L" Video is muted and looping; choose Fill or Fit in the library.";
  SetWindowTextW(state->status,status.c_str());EnableWindow(state->button,TRUE);return 0;
 }
 if(message==WM_COMMAND){
  if(LOWORD(w)==IDOK){if(!IsWindowEnabled(state->button))return 0;EnableWindow(state->button,FALSE);state->installing=true;SetWindowTextW(state->status,L"Copying World into your offline library…");work(*state);return 0;}
  if(LOWORD(w)==IDCANCEL){DestroyWindow(hwnd);return 0;}
 }
 if(message==WM_CLOSE){DestroyWindow(hwnd);return 0;}
 if(message==WM_NCDESTROY){
  SetWindowLongPtrW(hwnd,GWLP_USERDATA,0);activeImport=nullptr;
  state->worker.request_stop();
  // A media decoder or filesystem operation may still be returning. Reap the
  // worker off the UI thread, retaining its state until that work has stopped.
  std::unique_ptr<Import> reclaim(state);
  try{
   std::erase_if(retiredImports,[](const auto& task){return task.finished->load();});
   auto finished=std::make_shared<std::atomic_bool>(false);
   retiredImports.reserve(retiredImports.size()+1);
   std::jthread reaper([state,finished]{delete state;finished->store(true);});
   reclaim.release();
   retiredImports.push_back({std::move(finished),std::move(reaper)});
  }catch(const std::exception&){
   // If asynchronous cleanup cannot start, the local owner stops and joins the
   // import here rather than leaking its state or throwing through Windows.
  }
 }
 return DefWindowProcW(hwnd,message,w,l);
}
}
void closeWorldImports(){
 if(IsWindow(activeImport))DestroyWindow(activeImport);
 for(auto& task:retiredImports)if(task.thread.joinable())task.thread.join();
 retiredImports.clear();
}
void showWorldImport(HWND owner,const std::filesystem::path& file,const std::filesystem::path& library,
 const std::wstring& destination,std::function<void(const Assignment&)> apply,bool personalVideo,LUID adapter){
 if(IsWindow(activeImport)){SetForegroundWindow(activeImport);throw std::runtime_error("Finish or cancel the current import first.");}
 WNDCLASSW cls{};cls.lpfnWndProc=procedure;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"VolturaWorldImport";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassW(&cls);
 auto state=std::make_unique<Import>();state->file=file;state->library=library;state->destination=destination;state->apply=std::move(apply);state->personalVideo=personalVideo;state->adapter=adapter;
 HWND hwnd=CreateWindowExW(WS_EX_DLGMODALFRAME,cls.lpszClassName,personalVideo?L"Import video":L"Import preset",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,600,650,owner,nullptr,cls.hInstance,state.get());
 if(!hwnd)throw std::runtime_error("Cannot open import preview");
 auto data=state.release();data->window=hwnd;activeImport=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(data));
 try{
 data->font=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 auto control=[&](const wchar_t* type,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id){if(!wcscmp(type,L"BUTTON"))style=(style&~DWORD(BS_TYPEMASK))|BS_OWNERDRAW;auto result=CreateWindowExW(0,type,text,WS_CHILD|WS_VISIBLE|style,x,y,width,height,hwnd,(HMENU)(INT_PTR)id,cls.hInstance,nullptr);if(!result)throw std::runtime_error("Cannot create import control");SendMessageW(result,WM_SETFONT,(WPARAM)data->font,TRUE);return result;};
 data->image=control(L"STATIC",L"",SS_BITMAP|SS_CENTERIMAGE,20,16,540,280,0);
 data->title=control(L"STATIC",L"Loading World…",SS_NOPREFIX,20,308,540,28,0);
 data->description=control(L"EDIT",L"",ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL,20,344,540,116,0);
 data->status=control(L"STATIC",L"Validating the package. Your desktop will change only after you choose Import and apply.",SS_NOPREFIX,20,474,540,60,0);
 data->button=control(L"BUTTON",L"Import and apply",WS_TABSTOP|BS_DEFPUSHBUTTON,206,552,190,34,IDOK);EnableWindow(data->button,FALSE);
 data->cancel=control(L"BUTTON",L"Cancel",WS_TABSTOP,412,552,148,34,IDCANCEL);
 theme(*data);sizeToWorkArea(hwnd,GetDpiForWindow(hwnd));layout(*data);
 ShowWindow(hwnd,SW_SHOW);SetForegroundWindow(hwnd);work(*data);
 }catch(...){DestroyWindow(hwnd);throw;}
}
}
