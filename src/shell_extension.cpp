#include "shell_ipc.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <shlguid.h>
#include <shellapi.h>
#include <strsafe.h>
#include <wrl.h>
#include <atomic>
#include <memory>
#include <new>
using Microsoft::WRL::ComPtr;
using namespace worlds::shellmenu;
namespace {
// Each group is a separate registered shell command, with its own command IDs.
const CLSID classicClsids[]={
 {0x8aa13292,0x0832,0x4d9d,{0xab,0x10,0x86,0x37,0xd2,0x38,0x0e,0x4a}},
 {0x8aa13292,0x0832,0x4d9d,{0xab,0x10,0x86,0x37,0xd2,0x38,0x0e,0x4e}},
 {0x8aa13292,0x0832,0x4d9d,{0xab,0x10,0x86,0x37,0xd2,0x38,0x0e,0x4f}}
};
const CLSID compactClsids[]={
 {0x8aa13292,0x0832,0x4d9d,{0xab,0x10,0x86,0x37,0xd2,0x38,0x0e,0x4b}},
 {0x8aa13292,0x0832,0x4d9d,{0xab,0x10,0x86,0x37,0xd2,0x38,0x0e,0x4c}},
 {0x8aa13292,0x0832,0x4d9d,{0xab,0x10,0x86,0x37,0xd2,0x38,0x0e,0x4d}}
};
HINSTANCE module{};std::atomic<long> objects=0;
std::filesystem::path appPath(){
 wchar_t path[32768]{};auto n=GetModuleFileNameW(module,path,DWORD(std::size(path)));if(!n||n>=std::size(path))return {};
 auto folder=std::filesystem::path(path).parent_path();
 // Normal build output, or installed shell/<revision>/DLL.
 if(folder.parent_path().filename()==L"shell")folder=folder.parent_path().parent_path();
 return folder/L"VolturaWorlds.exe";
}
bool desktopSite(IUnknown* site){
 if(!site)return false;
 ComPtr<IOleWindow> window;HWND hwnd=nullptr;
 if(SUCCEEDED(site->QueryInterface(IID_PPV_ARGS(&window)))&&SUCCEEDED(window->GetWindow(&hwnd))){
  wchar_t name[64]{};GetClassNameW(GetAncestor(hwnd,GA_ROOT),name,64);
  if(!wcscmp(name,L"Progman")||!wcscmp(name,L"WorkerW"))return true;
 }
 ComPtr<IServiceProvider> services;ComPtr<IFolderView> view;ComPtr<IPersistFolder2> folder;
 if(FAILED(site->QueryInterface(IID_PPV_ARGS(&services))))return false;
 if(FAILED(services->QueryService(SID_SFolderView,IID_PPV_ARGS(&view)))){
  ComPtr<IShellBrowser> browser;ComPtr<IShellView> shellView;
  if(FAILED(services->QueryService(SID_STopLevelBrowser,IID_PPV_ARGS(&browser)))||FAILED(browser->QueryActiveShellView(&shellView))||FAILED(shellView.As(&view)))return false;
 }
 if(FAILED(view->GetFolder(IID_PPV_ARGS(&folder))))return false;
 PIDLIST_ABSOLUTE pidl=nullptr;if(FAILED(folder->GetCurFolder(&pidl)))return false;
 bool desktop=pidl&&ILIsEmpty(pidl);CoTaskMemFree(pidl);return desktop;
}
struct Context {
 State state{};bool live=false;std::wstring displayId;std::filesystem::path executable;
 const DisplayState* display()const{if(!live)return nullptr;for(DWORD i=0;i<state.displayCount;i++)if(displayId==state.displays[i].id)return &state.displays[i];return nullptr;}
};
std::shared_ptr<Context> captureContext(IUnknown* site){
 auto context=std::make_shared<Context>();context->executable=appPath();
 POINT point{};GetCursorPos(&point);
 // Keyboard context menus use the monitor of the invoking desktop view.
 if((GetKeyState(VK_APPS)&0x8000)||((GetKeyState(VK_SHIFT)&0x8000)&&(GetKeyState(VK_F10)&0x8000))){
  ComPtr<IOleWindow> window;HWND hwnd=nullptr;
  if(site&&SUCCEEDED(site->QueryInterface(IID_PPV_ARGS(&window)))&&SUCCEEDED(window->GetWindow(&hwnd))){RECT r{};if(GetWindowRect(hwnd,&r))point={r.left+(r.right-r.left)/2,r.top+(r.bottom-r.top)/2};}
 }
 MONITORINFOEXW monitor{};monitor.cbSize=sizeof(monitor);GetMonitorInfoW(MonitorFromPoint(point,MONITOR_DEFAULTTONEAREST),&monitor);
 Response response{};context->live=exchange(Request{},response,context->executable);
 if(context->live){
  context->state=response.state;
  for(DWORD i=0;i<context->state.displayCount;i++){
   const auto& d=context->state.displays[i];
   if(wcsnlen_s(d.id,std::size(d.id))==std::size(d.id)||wcsnlen_s(d.device,std::size(d.device))==std::size(d.device)||wcsnlen_s(d.choice,std::size(d.choice))==std::size(d.choice)){context->live=false;break;}
   if(!_wcsicmp(d.device,monitor.szDevice))context->displayId=d.id;
  }
 }
 return context;
}
HRESULT invoke(const std::shared_ptr<Context>& context,const Entry& entry,HWND owner){
 if(!entry.enabled||!validCommand(entry.id))return E_INVALIDARG;
 if(entry.id==L"open"){
  auto result=ShellExecuteW(owner,L"open",context->executable.c_str(),nullptr,context->executable.parent_path().c_str(),SW_SHOWNORMAL);
  return reinterpret_cast<INT_PTR>(result)>32?S_OK:E_FAIL;
 }
 if(entry.id==L"select-more"&&context->live)AllowSetForegroundWindow(context->state.processId);
 Request request{};request.invoke=1;wcscpy_s(request.command,entry.id.c_str());wcscpy_s(request.displayId,context->displayId.c_str());
 Response response{};return exchange(request,response,context->executable)&&response.accepted?S_OK:E_FAIL;
}
class Command;
class Enumerator final:public IEnumExplorerCommand {
 std::atomic<ULONG> refs{1};std::vector<ComPtr<IExplorerCommand>> commands;ULONG index=0;
public:
 explicit Enumerator(std::vector<ComPtr<IExplorerCommand>> values):commands(std::move(values)){++objects;}
 ~Enumerator(){--objects;}
 IFACEMETHODIMP QueryInterface(REFIID iid,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(iid==IID_IUnknown||iid==IID_IEnumExplorerCommand)*out=static_cast<IEnumExplorerCommand*>(this);else return E_NOINTERFACE;AddRef();return S_OK;}
 IFACEMETHODIMP_(ULONG) AddRef()override{return ++refs;}
 IFACEMETHODIMP_(ULONG) Release()override{auto n=--refs;if(!n)delete this;return n;}
 IFACEMETHODIMP Next(ULONG count,IExplorerCommand** out,ULONG* fetched)override{if(!out||(!fetched&&count!=1))return E_POINTER;ULONG n=0;while(n<count&&index<commands.size()){out[n]=commands[index++].Get();out[n++]->AddRef();}if(fetched)*fetched=n;return n==count?S_OK:S_FALSE;}
 IFACEMETHODIMP Skip(ULONG count)override{auto remaining=ULONG(commands.size())-index;index+=std::min(count,remaining);return count<=remaining?S_OK:S_FALSE;}
 IFACEMETHODIMP Reset()override{index=0;return S_OK;}
 IFACEMETHODIMP Clone(IEnumExplorerCommand** out)override{if(!out)return E_POINTER;auto copy=new(std::nothrow) Enumerator(commands);if(!copy)return E_OUTOFMEMORY;copy->index=index;*out=copy;return S_OK;}
};
class Command final:public IExplorerCommand,public IObjectWithSite,public IShellExtInit,public IContextMenu {
 std::atomic<ULONG> refs{1};bool classic=false,root=true,initialized=false,desktop=false,classicDesktop=false;
 DesktopGroup group=DesktopGroup::Controls;
 ComPtr<IUnknown> site;std::shared_ptr<Context> context;Entry entry;
 std::vector<Entry> offsets;
 void ensure(){if(initialized)return;initialized=true;desktop=(classic&&classicDesktop)||desktopSite(site.Get());if(desktop)context=captureContext(site.Get());}
 HMENU makeMenu(const std::vector<Entry>& values,UINT first,UINT last){
  // Never return a partial menu when a host supplies too few command IDs.
  auto needed=std::count_if(values.begin(),values.end(),[](const Entry& e){return !e.separator();});
  if(last<first||size_t(needed)>size_t(last-first)+1){SetLastError(ERROR_INSUFFICIENT_BUFFER);return nullptr;}
  HMENU menu=CreatePopupMenu();if(!menu)return nullptr;
  for(const auto& e:values){
   BOOL appended;
   if(e.separator())appended=AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
   else {
    UINT flags=MF_STRING|(e.checked?MF_CHECKED:0)|(e.enabled?0:MF_GRAYED);
    UINT id=first+UINT(offsets.size());offsets.push_back(e);
    appended=AppendMenuW(menu,flags,id,e.title.c_str());
   }
   if(!appended){DestroyMenu(menu);return nullptr;}
  }
  return menu;
 }
public:
 Command(bool legacy,DesktopGroup value):classic(legacy),group(value){++objects;}
 Command(std::shared_ptr<Context> c,Entry e,DesktopGroup value):root(false),initialized(true),desktop(true),group(value),context(std::move(c)),entry(std::move(e)){++objects;}
 ~Command(){--objects;}
 IFACEMETHODIMP QueryInterface(REFIID iid,void** out)override{
  if(!out)return E_POINTER;*out=nullptr;
  if(iid==IID_IUnknown)*out=classic?static_cast<IUnknown*>(static_cast<IContextMenu*>(this)):static_cast<IUnknown*>(static_cast<IExplorerCommand*>(this));
  else if(iid==IID_IExplorerCommand&&!classic)*out=static_cast<IExplorerCommand*>(this);
  else if(iid==IID_IObjectWithSite)*out=static_cast<IObjectWithSite*>(this);
  else if(iid==IID_IShellExtInit&&classic)*out=static_cast<IShellExtInit*>(this);
  else if(iid==IID_IContextMenu&&classic)*out=static_cast<IContextMenu*>(this);
  else return E_NOINTERFACE;AddRef();return S_OK;
 }
 IFACEMETHODIMP_(ULONG) AddRef()override{return ++refs;}
 IFACEMETHODIMP_(ULONG) Release()override{auto n=--refs;if(!n)delete this;return n;}
 IFACEMETHODIMP SetSite(IUnknown* value)override{site=value;initialized=false;context.reset();return S_OK;}
 IFACEMETHODIMP GetSite(REFIID iid,void** out)override{if(!out)return E_POINTER;*out=nullptr;return site?site->QueryInterface(iid,out):E_FAIL;}
 IFACEMETHODIMP Initialize(PCIDLIST_ABSOLUTE folder,IDataObject*,HKEY)override{classicDesktop=folder&&ILIsEmpty(folder);initialized=false;context.reset();return S_OK;}
 IFACEMETHODIMP GetTitle(IShellItemArray*,LPWSTR* out)override{if(!out)return E_POINTER;return SHStrDupW(root?desktopTitle(group):entry.title.c_str(),out);}
 IFACEMETHODIMP GetIcon(IShellItemArray*,LPWSTR* out)override{if(!out)return E_POINTER;*out=nullptr;if(!root)return E_NOTIMPL;try{return SHStrDupW((appPath().wstring()+L",0").c_str(),out);}catch(...){return E_FAIL;}}
 IFACEMETHODIMP GetToolTip(IShellItemArray*,LPWSTR* out)override{if(!out)return E_POINTER;*out=nullptr;return E_NOTIMPL;}
 IFACEMETHODIMP GetCanonicalName(GUID* out)override{
  if(!out)return E_POINTER;*out=classic?classicClsids[size_t(group)]:compactClsids[size_t(group)];
  if(!root){ULONG hash=2166136261u;for(auto c:entry.id){hash^=ULONG(c);hash*=16777619u;}out->Data1=hash;}
  return S_OK;
 }
 IFACEMETHODIMP GetState(IShellItemArray*,BOOL okToBeSlow,EXPCMDSTATE* out)override{
  if(!out)return E_POINTER;*out=ECS_HIDDEN;
  // IPC may take 100 ms. Ask the compact host to retry off its UI thread;
  // classic hosts use our separate IContextMenu instead of adding a duplicate.
  if(root&&!classic&&!okToBeSlow)return E_PENDING;
  try{ensure();if(desktop&&(!root||group==DesktopGroup::Controls||(context&&context->live)))*out=root?ECS_ENABLED:EXPCMDSTATE((entry.enabled?ECS_ENABLED:ECS_DISABLED)|(entry.checked?ECS_CHECKED:0));return S_OK;}catch(...){return S_OK;}
 }
 IFACEMETHODIMP Invoke(IShellItemArray*,IBindCtx*)override{try{ensure();return root?E_NOTIMPL:invoke(context,entry,nullptr);}catch(...){return E_FAIL;}}
 IFACEMETHODIMP GetFlags(EXPCMDFLAGS* out)override{if(!out)return E_POINTER;*out=root?ECF_HASSUBCOMMANDS:entry.separator()?ECF_ISSEPARATOR:ECF_DEFAULT;return S_OK;}
 IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand** out)override{
  if(!out)return E_POINTER;*out=nullptr;if(!root)return E_NOTIMPL;
  try{ensure();if(!context)return E_FAIL;std::vector<ComPtr<IExplorerCommand>> values;
   for(auto e:desktopEntries(context->live?&context->state:nullptr,context->display(),group)){ComPtr<IExplorerCommand> c;c.Attach(new Command(context,std::move(e),group));values.push_back(std::move(c));}
   *out=new Enumerator(std::move(values));return S_OK;
  }catch(...){return E_OUTOFMEMORY;}
 }
 IFACEMETHODIMP QueryContextMenu(HMENU menu,UINT position,UINT first,UINT last,UINT flags)override{
  if(!classic||(flags&CMF_DEFAULTONLY))return MAKE_HRESULT(SEVERITY_SUCCESS,0,0);
  try{ensure();if(!desktop||!context)return MAKE_HRESULT(SEVERITY_SUCCESS,0,0);offsets.clear();auto values=desktopEntries(context->live?&context->state:nullptr,context->display(),group);if(values.empty())return MAKE_HRESULT(SEVERITY_SUCCESS,0,0);auto child=makeMenu(values,first,last);if(!child)return HRESULT_FROM_WIN32(GetLastError()?GetLastError():ERROR_NOT_ENOUGH_MEMORY);
   if(!InsertMenuW(menu,position,MF_BYPOSITION|MF_POPUP,reinterpret_cast<UINT_PTR>(child),desktopTitle(group))){DestroyMenu(child);return E_FAIL;}
   return MAKE_HRESULT(SEVERITY_SUCCESS,0,USHORT(offsets.size()));
  }catch(...){return E_FAIL;}
 }
 IFACEMETHODIMP InvokeCommand(CMINVOKECOMMANDINFO* info)override{
  if(!info)return E_POINTER;try{
   if(HIWORD(info->lpVerb))return E_INVALIDARG;auto index=LOWORD(info->lpVerb);if(index>=offsets.size()||!context)return E_INVALIDARG;
   return invoke(context,offsets[index],info->hwnd);
  }catch(...){return E_FAIL;}
 }
 IFACEMETHODIMP GetCommandString(UINT_PTR id,UINT flags,UINT*,LPSTR buffer,UINT size)override{
  if(id>=offsets.size()||!buffer)return E_INVALIDARG;
  if(flags==GCS_VERBW)return StringCchCopyW(reinterpret_cast<LPWSTR>(buffer),size,offsets[id].id.c_str());
  if(flags==GCS_VALIDATEW||flags==GCS_VALIDATEA)return S_OK;return E_NOTIMPL;
 }
};
class Factory final:public IClassFactory {
 std::atomic<ULONG> refs{1};bool classic;DesktopGroup group;
public:
 Factory(bool legacy,DesktopGroup value):classic(legacy),group(value){++objects;}~Factory(){--objects;}
 IFACEMETHODIMP QueryInterface(REFIID iid,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(iid!=IID_IUnknown&&iid!=IID_IClassFactory)return E_NOINTERFACE;*out=static_cast<IClassFactory*>(this);AddRef();return S_OK;}
 IFACEMETHODIMP_(ULONG) AddRef()override{return ++refs;}
 IFACEMETHODIMP_(ULONG) Release()override{auto n=--refs;if(!n)delete this;return n;}
 IFACEMETHODIMP CreateInstance(IUnknown* outer,REFIID iid,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(outer)return CLASS_E_NOAGGREGATION;auto object=new(std::nothrow) Command(classic,group);if(!object)return E_OUTOFMEMORY;auto result=object->QueryInterface(iid,out);object->Release();return result;}
 IFACEMETHODIMP LockServer(BOOL lock)override{objects+=lock?1:-1;return S_OK;}
};
}
extern "C" BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,void*){if(reason==DLL_PROCESS_ATTACH){module=instance;DisableThreadLibraryCalls(instance);}return TRUE;}
STDAPI DllGetClassObject(REFCLSID clsid,REFIID iid,void** out){
 if(!out)return E_POINTER;*out=nullptr;
 for(size_t i=0;i<std::size(desktopGroups);i++){
  if(clsid!=classicClsids[i]&&clsid!=compactClsids[i])continue;
  auto factory=new(std::nothrow) Factory(clsid==classicClsids[i],desktopGroups[i]);
  if(!factory)return E_OUTOFMEMORY;auto result=factory->QueryInterface(iid,out);factory->Release();return result;
 }
 return CLASS_E_CLASSNOTAVAILABLE;
}
STDAPI DllCanUnloadNow(){return objects==0?S_OK:S_FALSE;}
