#include "presets_ui.h"
#include "video.h"
#include "audio.h"
#include "ui_theme.h"
#include "ui_confirm.h"
#include "ui_scrollbar.h"
#include <commdlg.h>
#include <thread>
#include <mutex>
#include <set>
#include <algorithm>
#include <cwctype>
#include <fstream>
#include <array>
namespace worlds {
namespace {
constexpr UINT completed=WM_APP+61,progressed=WM_APP+62;
constexpr int listId=10,nameId=11,descriptionId=12,attributionId=13,saveId=20,updateId=21,renameId=22,deleteId=23,applyId=24,exportId=25,currentExportId=26,closeId=27,confirmId=28,backId=29,mappingCloseId=30,importPresetId=34,mapBase=100;
HWND active{};
 struct Item {HWND window{};int x,y,w,h;bool mapping=false;};
enum class Result {Refresh,Mapping,Applied};
struct State {
 HWND window{},status{},list{},scrollbar{};HFONT font{};PresetUiContext context;std::vector<Item> items;std::vector<SavedPreset> presets;
 std::jthread worker;std::mutex mutex;std::wstring progress,error,notification;bool ownedByWindow=false,busy=false,closing=false,mapping=false,importing=false,confirming=false;int scroll=0,wheelRemainder=0;UINT dpi=96;
 DesktopPreset pending,prepared;std::vector<Monitor> monitors;std::vector<int> destinations;std::vector<SavedPreset> refreshed;Result result=Result::Refresh;
 std::filesystem::path importedSaved,replacePreset,presetBackup;bool saveImported=false;std::vector<std::filesystem::path> installedDirectories;
 int px(int n)const{return MulDiv(n,dpi,96);}
 bool rollbackPending()const{return !presetBackup.empty()||!importedSaved.empty();}
 std::wstring rollbackFailure()const{return !presetBackup.empty()?L"Cannot restore the previous preset. Its backup is retained at "+presetBackup.wstring()+L". Close Presets to retry cleanup.":L"Cannot remove the cancelled saved preset at "+importedSaved.wstring()+L". Close Presets to retry cleanup.";}
 bool rollbackSaved(){if(!presetBackup.empty()){if(!MoveFileExW(presetBackup.c_str(),replacePreset.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return false;presetBackup.clear();}else if(!importedSaved.empty()){std::error_code ec;std::filesystem::remove(importedSaved,ec);if(ec)return false;}importedSaved.clear();return true;}
 ~State(){worker.request_stop();if(worker.joinable())worker.join();for(auto& path:installedDirectories){std::error_code ec;if(std::filesystem::absolute(path).lexically_normal().parent_path()==std::filesystem::absolute(context.profile/L"library").lexically_normal())std::filesystem::remove_all(path,ec);}rollbackSaved();if(font)DeleteObject(font);}
};
struct ModalGuard {bool& flag;bool previous;explicit ModalGuard(bool& value):flag(value),previous(value){flag=true;}~ModalGuard(){flag=previous;}};
bool choosePresetFile(State& s,OPENFILENAMEW& dialog,bool save){
 ModalGuard guard{s.confirming};return (save?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog))!=FALSE;
}
bool confirm(State& s,const std::wstring& title,const std::wstring& message,const wchar_t* action){
 ModalGuard guard{s.confirming};
 return ui::confirm(s.window,title,message,action);
}
std::wstring text(HWND control){int size=GetWindowTextLengthW(control);std::wstring value(size+1,L'\0');GetWindowTextW(control,value.data(),size+1);value.resize(size);return value;}
LRESULT CALLBACK statusPaint(HWND hwnd,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR){
 if(message==WM_ERASEBKGND)return 1;
 if(message==WM_SETTEXT&&text(hwnd)==(l?reinterpret_cast<const wchar_t*>(l):L""))return TRUE;
 if(message==WM_PAINT||message==WM_PRINTCLIENT||(message==WM_PRINT&&(l&PRF_CLIENT))){
  PAINTSTRUCT ps{};HDC destination=message==WM_PAINT?BeginPaint(hwnd,&ps):reinterpret_cast<HDC>(w);RECT r{};GetClientRect(hwnd,&r);
  if(r.right>0&&r.bottom>0){auto dc=CreateCompatibleDC(destination);auto bitmap=CreateCompatibleBitmap(destination,r.right,r.bottom);auto old=SelectObject(dc,bitmap);FillRect(dc,&r,ui::backgroundBrush);auto font=SelectObject(dc,reinterpret_cast<HFONT>(SendMessageW(hwnd,WM_GETFONT,0,0)));SetBkMode(dc,TRANSPARENT);SetTextColor(dc,ui::foreground);auto label=r;auto value=text(hwnd);DrawTextW(dc,value.c_str(),int(value.size()),&label,DT_WORDBREAK|DT_NOPREFIX|DT_EDITCONTROL);BitBlt(destination,0,0,r.right,r.bottom,dc,0,0,SRCCOPY);SelectObject(dc,font);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);}
  if(message==WM_PAINT)EndPaint(hwnd,&ps);return 0;
 }
 return DefSubclassProc(hwnd,message,w,l);
}
void layout(State& s,bool scrolling=false){
 const wchar_t* title=s.mapping&&s.importing?L"Import preset":L"Presets";if(!scrolling&&text(s.window)!=title)SetWindowTextW(s.window,title);
 RECT r{};GetClientRect(s.window,&r);const int margin=s.px(30);int height=s.mapping?298+int(s.pending.displays.size())*76:604;int footer=std::min(int(r.bottom),s.px(112));int viewport=std::max(0,int(r.bottom)-footer),content=s.px(height);
 s.scroll=std::clamp(s.scroll,0,std::max(0,content-viewport));SCROLLINFO si{sizeof(si),SIF_RANGE|SIF_PAGE|SIF_POS,0,content-1,UINT(viewport),s.scroll};ui::updateScrollbar(s.scrollbar,si);ui::scrollbarLine(s.scrollbar,s.px(32));int barWidth=GetSystemMetricsForDpi(SM_CXVSCROLL,s.dpi);
 if(s.scrollbar&&!scrolling)SetWindowPos(s.scrollbar,nullptr,r.right-barWidth,0,barWidth,viewport,SWP_NOZORDER|SWP_NOACTIVATE|(content>viewport?SWP_SHOWWINDOW:SWP_HIDEWINDOW));
 auto batch=BeginDeferWindowPos(int(s.items.size()+1));for(auto& item:s.items){bool visible=item.mapping==s.mapping;if(batch)batch=DeferWindowPos(batch,item.window,nullptr,margin+MulDiv(item.x-20,std::max(0,int(r.right)-2*margin),660),s.px(item.y+14)-s.scroll,MulDiv(item.w,std::max(0,int(r.right)-2*margin),660),s.px(item.h),SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW|(visible?SWP_SHOWWINDOW:SWP_HIDEWINDOW));}if(batch)EndDeferWindowPos(batch);
 if(s.status&&!scrolling)SetWindowPos(s.status,HWND_TOP,margin,viewport+margin,std::max(0,int(r.right)-2*margin),std::max(0,footer-2*margin),SWP_NOACTIVATE|SWP_NOREDRAW);
 RECT redraw=r;if(scrolling)redraw.bottom=viewport;RedrawWindow(s.window,&redraw,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
}
void theme(State& s){ui::refresh(s.window);BOOL dark=ui::dark;DwmSetWindowAttribute(s.window,20,&dark,sizeof(dark));for(auto& item:s.items){SetWindowTheme(item.window,ui::dark?L"DarkMode_Explorer":L"Explorer",nullptr);wchar_t cls[24]{};GetClassNameW(item.window,cls,24);if(!wcscmp(cls,L"ComboBox"))ui::themeCombo(item.window);if(!wcscmp(cls,L"Edit")||!wcscmp(cls,L"ListBox")){SetWindowTheme(item.window,L"",L"");RedrawWindow(item.window,nullptr,nullptr,RDW_INVALIDATE|RDW_FRAME);}}InvalidateRect(s.window,nullptr,TRUE);}
HWND add(State& s,const wchar_t* cls,const std::wstring& caption,DWORD style,int id,int x,int y,int w,int h,bool mapping=false){if(!wcscmp(cls,L"COMBOBOX"))style|=CBS_OWNERDRAWFIXED|CBS_HASSTRINGS;if(!wcscmp(cls,L"BUTTON"))style|=BS_OWNERDRAW;if(!wcscmp(cls,L"EDIT"))style|=WS_BORDER;auto child=CreateWindowExW(0,cls,caption.c_str(),WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|style,x,y,w,h,s.window,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);if(!child)throw std::runtime_error("Cannot create preset control.");SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(s.font),TRUE);try{s.items.push_back({child,x,y,w,h,mapping});}catch(...){DestroyWindow(child);throw;}if(!wcscmp(cls,L"EDIT")||!wcscmp(cls,L"LISTBOX")){SetWindowTheme(child,L"",L"");SetWindowSubclass(child,ui::fieldChrome,1,0);if(!wcscmp(cls,L"EDIT"))SendMessageW(child,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(s.px(10),s.px(10)));}if((!wcscmp(cls,L"EDIT")||!wcscmp(cls,L"LISTBOX"))&&(style&WS_VSCROLL))ui::bindFieldScrollbar(child);if(!wcscmp(cls,L"COMBOBOX")){SendMessageW(child,CB_SETITEMHEIGHT,WPARAM(-1),s.px(26));SendMessageW(child,CB_SETITEMHEIGHT,0,s.px(26));SetWindowSubclass(child,ui::comboBorder,1,0);ui::themeCombo(child);}
 SetWindowSubclass(child,[](HWND window,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR)->LRESULT{if(message==WM_SETFOCUS){auto state=reinterpret_cast<State*>(GetWindowLongPtrW(GetParent(window),GWLP_USERDATA));if(state)for(auto& item:state->items)if(item.window==window){RECT client{};GetClientRect(state->window,&client);client.bottom=std::max(0,int(client.bottom)-state->px(112));wchar_t type[24]{};GetClassNameW(window,type,24);int previousScroll=state->scroll;int top=state->px(item.y+14),bottom=top+state->px(!wcscmp(type,L"ComboBox")?32:item.h);if(top<state->scroll)state->scroll=top;else if(bottom>state->scroll+client.bottom)state->scroll=bottom-int(client.bottom)+state->px(8);if(state->scroll!=previousScroll)layout(*state,true);break;}}return DefSubclassProc(window,message,w,l);},76,0);return child;}
void buttons(State& s){auto index=SendMessageW(s.list,LB_GETCURSEL,0,0);bool selected=index!=LB_ERR;bool valid=selected&&size_t(index)<s.presets.size()&&s.presets[size_t(index)].error.empty(),blocked=s.busy||s.rollbackPending();for(auto& item:s.items){int id=GetDlgCtrlID(item.window);EnableWindow(item.window,!blocked||id==closeId||id==mappingCloseId||id==IDCANCEL);if(!blocked&&(id==updateId||id==renameId||id==deleteId||id==applyId||id==exportId))EnableWindow(item.window,id==deleteId?selected:valid);}SetWindowTextW(GetDlgItem(s.window,s.mapping?mappingCloseId:closeId),s.busy?L"Cancel operation":L"Close");}
void operation(State& s,std::function<void(std::stop_token)> function){if(s.busy)return;if(s.worker.joinable())s.worker.join();s.busy=true;s.error.clear();s.notification.clear();s.progress=L"Preparing…";SetWindowTextW(s.status,s.progress.c_str());buttons(s);try{s.worker=std::jthread([&s,function=std::move(function)](std::stop_token stop){auto hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);try{if(FAILED(hr))throw std::runtime_error("Cannot initialize preset worker.");function(stop);}catch(const std::exception& e){s.error=describeError(e);}if(SUCCEEDED(hr))CoUninitialize();PostMessageW(s.window,completed,0,0);});}catch(...){s.busy=false;buttons(s);throw;}}
PresetProgress progress(State& s){return [&s](const std::wstring& value){{std::lock_guard lock(s.mutex);s.progress=value;}PostMessageW(s.window,progressed,0,0);};}
void refresh(State& s,std::stop_token stop){if(stop.stop_requested())throw std::runtime_error("Operation cancelled.");s.refreshed=listPresets(s.context.profile/L"presets",stop);s.result=Result::Refresh;}
void refreshAfterCommit(State& s,std::stop_token stop,std::wstring outcome){
 // The file/desktop change is already committed. A failed or cancelled list
 // refresh must not report that change as cancelled, or retain stale entries.
 s.notification=std::move(outcome);s.refreshed.clear();s.result=Result::Refresh;
 try{refresh(s,stop);}catch(const std::exception& e){
  s.notification+=stop.stop_requested()?L" List refresh cancelled.":L" List refresh failed: "+describeError(e)+L".";
  s.notification+=L" Close and reopen Presets to reload the list.";
 }
}
SavedPreset selected(State& s,bool allowInvalid=false){auto i=SendMessageW(s.list,LB_GETCURSEL,0,0);if(i<0||size_t(i)>=s.presets.size())throw std::runtime_error("Choose a saved preset first.");if(!allowInvalid&&!s.presets[size_t(i)].error.empty())throw std::runtime_error(s.presets[size_t(i)].error);return s.presets[size_t(i)];}
std::wstring worldLabel(const Assignment& a){if(a.random)return a.randomPool==RandomPool::SolarSystem?L"Random Solar System":L"Random";auto name=wide(sceneName(a.scene));name[0]=towupper(name[0]);std::replace(name.begin(),name.end(),L'_',L' ');return name;}
void fillMetadata(State& s,const DesktopPreset& p){SetDlgItemTextW(s.window,nameId,wide(p.title).c_str());SetDlgItemTextW(s.window,descriptionId,wide(p.description).c_str());SetDlgItemTextW(s.window,attributionId,wide(p.attribution).c_str());std::wstring details;for(size_t i=0;i<std::min<size_t>(p.displays.size(),3);i++){if(i)details+=L"; ";details+=p.displays[i].label+L": "+worldLabel(p.displays[i].assignment);}if(p.displays.size()>3)details+=L"; "+std::to_wstring(p.displays.size()-3)+L" more. Choose Apply to review all displays.";SetWindowTextW(s.status,details.c_str());}
DesktopPreset metadata(State& s,DesktopPreset p){p.title=utf8(text(GetDlgItem(s.window,nameId)));p.description=utf8(text(GetDlgItem(s.window,descriptionId)));p.attribution=utf8(text(GetDlgItem(s.window,attributionId)));validatePresetName(p.title);if(p.description.size()>4000||p.attribution.size()>2000)throw std::runtime_error("Description or attribution is too long.");return p;}
void clearMapping(State& s){for(auto i=s.items.begin();i!=s.items.end();)if(i->mapping){DestroyWindow(i->window);i=s.items.erase(i);}else ++i;}
void mapping(State& s){
 clearMapping(s);s.monitors=s.context.monitors();if(s.monitors.empty())throw std::runtime_error("Connect a display before applying a preset.");s.mapping=true;s.scroll=0;
 try{
 add(s,L"STATIC",s.importing?L"Import preset — choose where to apply it":L"Review display assignments and global preferences",0,0,20,16,660,28,true);
 std::wstring global=L"Pause on battery: "+std::wstring(s.pending.pauseBattery?L"On":L"Off")+L". Audio: "+(s.pending.audioDevice.empty()?L"System output":L"Saved device (system fallback if unavailable)");add(s,L"STATIC",L"Preset name",0,0,20,52,110,28,true);add(s,s.importing?L"EDIT":L"STATIC",wide(s.pending.title),s.importing?ES_AUTOHSCROLL|WS_TABSTOP:SS_NOPREFIX,31,140,48,540,30,true);if(s.pending.description.empty()&&s.pending.attribution.empty())add(s,L"STATIC",L"Restore the preset from this file, or save them as a named preset as well.",SS_NOPREFIX,32,20,88,660,64,true);else add(s,L"EDIT",wide(s.pending.description)+L"\r\n"+wide(s.pending.attribution),ES_MULTILINE|ES_READONLY|WS_VSCROLL|WS_TABSTOP,32,20,88,660,64,true);add(s,L"STATIC",global,0,0,20,164,660,42,true);
 for(size_t i=0;i<s.pending.displays.size();i++){auto& display=s.pending.displays[i];int y=220+int(i)*76;add(s,L"STATIC",display.label+L" — "+worldLabel(display.assignment),SS_NOPREFIX,0,20,y,660,24,true);auto combo=add(s,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,mapBase+int(i),20,y+26,660,200,true);ui::appendComboChoice(combo,L"Skip this display");int match=0;for(size_t k=0;k<s.monitors.size();k++){ui::appendComboChoice(combo,s.monitors[k].name.c_str());if(s.monitors[k].id==display.id)match=int(k)+1;}SendMessageW(combo,CB_SETCURSEL,match,0);}
 int y=220+int(s.pending.displays.size())*76;add(s,L"BUTTON",s.importing?L"Apply preset":L"Apply preset",WS_TABSTOP,confirmId,20,y,180,34,true);if(s.importing)add(s,L"BUTTON",L"Save and apply…",WS_TABSTOP,33,210,y,180,34,true);add(s,L"BUTTON",L"Back",WS_TABSTOP,backId,400,y,120,34,true);add(s,L"BUTTON",L"Close",WS_TABSTOP,mappingCloseId,540,y,140,34,true);theme(s);layout(s);buttons(s);SetFocus(GetDlgItem(s.window,mapBase));
 SetWindowTextW(s.status,s.importing?L"Apply preset restores this file without creating a preset. Save and apply also saves a named preset; an existing name requires replacement confirmation.":L"Choose a destination or Skip for each entry. Each destination can be used once. Videos require compatible Windows decoders.");
 }catch(...){clearMapping(s);s.mapping=s.importing=false;s.scroll=0;s.presets.clear();SendMessageW(s.list,LB_RESETCONTENT,0,0);layout(s);buttons(s);throw;}
}
void cleanInstalled(State& s){for(auto& path:s.installedDirectories){std::error_code ec;if(std::filesystem::absolute(path).lexically_normal().parent_path()==std::filesystem::absolute(s.context.profile/L"library").lexically_normal())std::filesystem::remove_all(path,ec);}s.installedDirectories.clear();}
void copyPresetBackup(State& s,std::stop_token stop){
 auto backup=s.replacePreset;backup+=L"."+wide(newPresetId())+L".backup";
 // Establish ownership atomically before any failure cleanup can remove it.
 HANDLE output=CreateFileW(backup.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(output==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create a new preset backup; replacement cancelled.");
 try{
  std::ifstream input(s.replacePreset,std::ios::binary);if(!input)throw std::runtime_error("Cannot read the preset being replaced.");
  std::array<char,65536> buffer{};uint64_t total=0;
  while(input){
   if(stop.stop_requested())throw std::runtime_error("Operation cancelled.");
   input.read(buffer.data(),buffer.size());auto count=DWORD(input.gcount());total+=count;
   if(total>100ull*1024*1024)throw std::runtime_error("Preset backup exceeds the package size limit.");
   DWORD written{};if(count&&(!WriteFile(output,buffer.data(),count,&written,nullptr)||written!=count))throw std::runtime_error("Cannot write the preset backup.");
  }
  if(input.bad()||!input.eof()||!FlushFileBuffers(output))throw std::runtime_error("Cannot finish the preset backup.");
  s.presetBackup=backup;
 }catch(...){CloseHandle(output);std::error_code ec;std::filesystem::remove(backup,ec);throw;}
 CloseHandle(output);
}
void startApply(State& s,bool saveImport=false){
 if(s.rollbackPending())throw std::runtime_error(utf8(s.rollbackFailure()));s.saveImported=s.importing&&saveImport;s.replacePreset.clear();
 if(s.saveImported){s.pending.title=utf8(text(GetDlgItem(s.window,31)));validatePresetName(s.pending.title);for(auto& entry:s.presets)if(entry.error.empty()&&samePresetName(entry.preset.title,s.pending.title)){if(!confirm(s,L"Replace saved preset",L"A preset named “"+wide(s.pending.title)+L"” is already saved. Replace it with the preset from this file?",L"Replace"))return;s.replacePreset=entry.file;break;}}
 s.destinations.clear();std::set<int> used;for(size_t i=0;i<s.pending.displays.size();i++){int value=int(SendDlgItemMessageW(s.window,mapBase+int(i),CB_GETCURSEL,0,0))-1;if(value>=0&&!used.insert(value).second)throw std::runtime_error("Choose each destination display at most once.");s.destinations.push_back(value);}if(used.empty())throw std::runtime_error("Choose at least one destination display.");
 auto current=s.context.monitors();for(auto k:used)if(size_t(k)>=s.monitors.size()||std::none_of(current.begin(),current.end(),[&](auto& m){return m.id==s.monitors[k].id;}))throw std::runtime_error("Displays changed. Go back and review the mapping again.");
 operation(s,[&s](std::stop_token stop){
  // Only mapped videos need to be installed and supported on this machine.
  auto subset=s.pending;std::set<std::string> usedMedia;for(size_t i=0;i<subset.displays.size();i++)if(s.destinations[i]>=0&&!subset.displays[i].media.empty())usedMedia.insert(subset.displays[i].media);
  std::erase_if(subset.media,[&](auto& m){return !usedMedia.contains(m.name);});s.prepared=installDesktopMedia(s.context.profile/L"library",subset,stop,progress(s));s.installedDirectories.clear();for(auto& m:s.prepared.media)if(std::find(s.installedDirectories.begin(),s.installedDirectories.end(),m.source.parent_path())==s.installedDirectories.end())s.installedDirectories.push_back(m.source.parent_path());
  try{for(size_t i=0;i<s.prepared.displays.size();i++)if(s.destinations[i]>=0&&s.prepared.displays[i].assignment.scene==Scene::Video){progress(s)(L"Validating video on "+s.monitors[s.destinations[i]].name+L"…");preflightVideo(s.prepared.displays[i].assignment.video,stop,s.monitors[s.destinations[i]].adapter,15000,s.prepared.displays[i].assignment.videoSettings.speed);}if(stop.stop_requested())throw std::runtime_error("Operation cancelled.");if(s.saveImported){if(!s.replacePreset.empty())copyPresetBackup(s,stop);s.importedSaved=savePreset(s.context.profile/L"presets",s.pending,s.replacePreset,stop,progress(s));}s.result=Result::Applied;}
  catch(...){cleanInstalled(s);if(!s.rollbackSaved())throw std::runtime_error(utf8(s.rollbackFailure()));throw;}
 });
}
LRESULT CALLBACK procedure(HWND hwnd,UINT message,WPARAM w,LPARAM l){
 auto s=reinterpret_cast<State*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));if(message==WM_NCCREATE){s=static_cast<State*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);s->window=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(s));}if(!s)return DefWindowProcW(hwnd,message,w,l);
 try{
 if(message==WM_CLOSE){if(s->confirming)return 0;if(s->busy){s->closing=true;s->worker.request_stop();SetWindowTextW(s->status,L"Cancelling…");}else DestroyWindow(hwnd);return 0;}
 if(message==WM_NCDESTROY){if(active==hwnd)active=nullptr;SetWindowLongPtrW(hwnd,GWLP_USERDATA,0);if(s->ownedByWindow)delete s;return DefWindowProcW(hwnd,message,w,l);}
 if(message==WM_SIZE){layout(*s);return 0;}
 if(message==WM_DPICHANGED){auto dpi=HIWORD(w);s->scroll=MulDiv(s->scroll,dpi,s->dpi);s->dpi=dpi;auto r=reinterpret_cast<RECT*>(l);auto previous=s->font;s->font=CreateFontW(-s->px(16),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Segoe UI");for(auto& item:s->items){SendMessageW(item.window,WM_SETFONT,reinterpret_cast<WPARAM>(s->font),TRUE);wchar_t kind[24]{};GetClassNameW(item.window,kind,24);if(!wcscmp(kind,L"ComboBox")){SendMessageW(item.window,CB_SETITEMHEIGHT,WPARAM(-1),s->px(26));SendMessageW(item.window,CB_SETITEMHEIGHT,0,s->px(26));}else if(!wcscmp(kind,L"ListBox"))SendMessageW(item.window,LB_SETITEMHEIGHT,0,s->px(26));}SetWindowSubclass(s->status,statusPaint,1,0);SendMessageW(s->status,WM_SETFONT,reinterpret_cast<WPARAM>(s->font),TRUE);DeleteObject(previous);SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);layout(*s);return 0;}
 if(message==WM_MOUSEWHEEL||message==WM_VSCROLL){int previousScroll=s->scroll;SCROLLINFO si{sizeof(si),SIF_ALL};si=ui::scrollbarInfo(s->scrollbar);if(message==WM_MOUSEWHEEL){int movement=GET_WHEEL_DELTA_WPARAM(w)*s->px(48)+s->wheelRemainder;s->scroll-=movement/WHEEL_DELTA;s->wheelRemainder=movement%WHEEL_DELTA;}else switch(LOWORD(w)){case SB_LINEUP:s->scroll-=s->px(32);break;case SB_LINEDOWN:s->scroll+=s->px(32);break;case SB_PAGEUP:s->scroll-=si.nPage;break;case SB_PAGEDOWN:s->scroll+=si.nPage;break;case SB_THUMBTRACK:s->scroll=si.nTrackPos;break;case SB_TOP:s->scroll=0;break;case SB_BOTTOM:s->scroll=si.nMax;break;}s->scroll=std::clamp(s->scroll,0,std::max(0,si.nMax-int(si.nPage)+1));if(s->scroll!=previousScroll)layout(*s,true);return 0;}
 if(message==WM_MEASUREITEM){reinterpret_cast<MEASUREITEMSTRUCT*>(l)->itemHeight=s->px(26);return TRUE;}if(message==WM_DRAWITEM){ui::draw(*reinterpret_cast<DRAWITEMSTRUCT*>(l),s->font,false,s->dpi);return TRUE;}
 if(message==WM_SETTINGCHANGE||message==WM_THEMECHANGED){theme(*s);return 0;}
 if(message==WM_ERASEBKGND){RECT r;GetClientRect(hwnd,&r);FillRect(reinterpret_cast<HDC>(w),&r,ui::backgroundBrush);return 1;}
 if(message==WM_CTLCOLORSTATIC||message==WM_CTLCOLORBTN||message==WM_CTLCOLOREDIT||message==WM_CTLCOLORLISTBOX){wchar_t kind[24]{};GetClassNameW(reinterpret_cast<HWND>(l),kind,24);bool field=!wcscmp(kind,L"Edit")||!wcscmp(kind,L"ListBox");SetTextColor(reinterpret_cast<HDC>(w),IsWindowEnabled(reinterpret_cast<HWND>(l))?ui::foreground:ui::muted);SetBkColor(reinterpret_cast<HDC>(w),field?ui::surface:ui::background);return reinterpret_cast<LRESULT>(field?ui::surfaceBrush:ui::backgroundBrush);}
 if(message==progressed){std::lock_guard lock(s->mutex);SetWindowTextW(s->status,s->progress.c_str());return 0;}
 if(message==completed){s->worker.join();s->busy=false;
  if(s->closing){cleanInstalled(*s);if(s->rollbackSaved()){DestroyWindow(hwnd);return 0;}s->closing=false;s->error=s->rollbackFailure();}
  if(s->error.empty()&&s->result==Result::Applied&&s->worker.get_stop_token().stop_requested()){cleanInstalled(*s);s->error=s->rollbackSaved()?L"Operation cancelled.":s->rollbackFailure();}
  if(!s->error.empty()){SetWindowTextW(s->status,s->error.c_str());buttons(*s);return 0;}
  if(s->result==Result::Mapping){if(s->importing)s->presets=std::move(s->refreshed);mapping(*s);}
  else if(s->result==Result::Applied){
   // COM endpoint enumeration may pump messages while apply holds State refs.
   ModalGuard guard{s->confirming};
   std::wstring appliedMessage=s->importing?(s->saveImported?L"Preset applied and saved.":L"Preset applied. No saved preset was changed."):L"Preset applied.";
   try{s->context.apply(s->prepared,s->destinations,s->monitors);}catch(...){cleanInstalled(*s);if(!s->rollbackSaved())throw std::runtime_error(utf8(s->rollbackFailure()));throw;}
   s->installedDirectories.clear();s->importedSaved.clear();if(!s->presetBackup.empty()){std::error_code ec;std::filesystem::remove(s->presetBackup,ec);s->presetBackup.clear();}
   s->mapping=false;s->importing=false;clearMapping(*s);layout(*s);SetWindowTextW(s->status,L"Preset applied.");
   try{operation(*s,[s,appliedMessage](std::stop_token stop){refreshAfterCommit(*s,stop,appliedMessage);});}
   catch(const std::exception&){s->result=Result::Refresh;s->refreshed.clear();s->presets.clear();SendMessageW(s->list,LB_RESETCONTENT,0,0);SetWindowTextW(s->status,(appliedMessage+L" List refresh could not start. Close and reopen Presets to reload the list.").c_str());}
  }
  else{
   s->presets=std::move(s->refreshed);SendMessageW(s->list,LB_RESETCONTENT,0,0);
   try{for(auto& p:s->presets){auto count=p.preset.displays.size();auto label=wide(p.preset.title)+(p.error.empty()?L" — "+std::to_wstring(count)+(count==1?L" display":L" displays"):L" — Unavailable");if(SendMessageW(s->list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()))<0)throw std::runtime_error("Cannot populate preset list.");}}
   catch(const std::exception& e){s->presets.clear();SendMessageW(s->list,LB_RESETCONTENT,0,0);auto statusText=s->notification;if(!statusText.empty())statusText+=L" ";statusText+=describeError(e)+L" Close and reopen Presets to reload the list.";SetWindowTextW(s->status,statusText.c_str());buttons(*s);return 0;}
   SetWindowTextW(s->status,s->notification.empty()?L"Ready. Save current captures connected displays and global audio/battery preferences.":s->notification.c_str());
  }buttons(*s);return 0;
 }
 if(message==WM_COMMAND){if(s->confirming)return 0;int id=LOWORD(w),event=HIWORD(w);if(id==IDCANCEL||id==closeId||id==mappingCloseId){if(s->busy){s->worker.request_stop();SetWindowTextW(s->status,L"Cancelling…");}else DestroyWindow(hwnd);return 0;}if(s->busy||s->rollbackPending())return 0;if(id==IDOK)id=s->mapping?confirmId:saveId;
  if(id==listId&&event==LBN_SELCHANGE){auto preset=selected(*s,true);if(preset.error.empty())fillMetadata(*s,preset.preset);else SetWindowTextW(s->status,(L"Preset unavailable: "+wide(preset.error)+L". You can delete this entry.").c_str());buttons(*s);return 0;}if(event!=BN_CLICKED)return 0;
  if(id==importPresetId){wchar_t file[32768]=L"*.world";OPENFILENAMEW dialog{sizeof(dialog)};dialog.hwndOwner=hwnd;dialog.lpstrFilter=L"Voltura preset (*.world)\0*.world\0";dialog.lpstrFile=file;dialog.nMaxFile=32768;dialog.lpstrTitle=L"Import preset";dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;if(choosePresetFile(*s,dialog,false)&&s->context.importFile)s->context.importFile(file);return 0;}
  if(id==backId){s->mapping=false;s->importing=false;s->scroll=0;clearMapping(*s);layout(*s);operation(*s,[s](std::stop_token stop){refresh(*s,stop);});return 0;}
  if(id==confirmId||id==33){startApply(*s,id==33);return 0;}
  if(id==applyId){s->pending=selected(*s).preset;s->importing=false;mapping(*s);return 0;}
  if(id==deleteId){auto p=selected(*s,true);if(confirm(*s,L"Delete preset",L"Delete “"+wide(p.preset.title)+L"”? The active desktop will keep its current settings.",L"Delete"))operation(*s,[s,p](std::stop_token stop){std::wstring outcome=L"Preset deleted. Your active desktop is unchanged.";if(stop.stop_requested())throw std::runtime_error("Operation cancelled.");std::filesystem::remove(p.file);refreshAfterCommit(*s,stop,std::move(outcome));});return 0;}
  if(id==saveId||id==updateId||id==renameId){std::filesystem::path replace;DesktopPreset p;if(id==renameId){auto saved=selected(*s);replace=saved.file;p=metadata(*s,saved.preset);}else {p=metadata(*s,s->context.snapshot());if(id==updateId)replace=selected(*s).file;}operation(*s,[s,p,replace](std::stop_token stop){auto outcome=L"Saved “"+wide(p.title)+L"”.";savePreset(s->context.profile/L"presets",p,replace,stop,progress(*s));refreshAfterCommit(*s,stop,std::move(outcome));});return 0;}
  if(id==exportId||id==currentExportId){auto p=metadata(*s,id==exportId?selected(*s).preset:s->context.snapshot());wchar_t file[32768]=L"Desktop.world";OPENFILENAMEW dialog{sizeof(dialog)};dialog.hwndOwner=hwnd;dialog.lpstrFilter=L"Voltura preset (*.world)\0*.world\0";dialog.lpstrFile=file;dialog.nMaxFile=32768;dialog.lpstrDefExt=L"world";dialog.lpstrTitle=L"Export desktop preset";dialog.Flags=OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;if(choosePresetFile(*s,dialog,true)){auto path=std::filesystem::path(file);if(std::filesystem::exists(path)&&!confirm(*s,L"Replace World package",L"Replace the existing file?\r\n"+path.wstring(),L"Replace"))return 0;operation(*s,[s,p,path](std::stop_token stop){auto outcome=L"Exported to "+path.wstring();writeDesktopPackage(path,p,stop,progress(*s));refreshAfterCommit(*s,stop,std::move(outcome));});}return 0;}
 }
 }catch(const std::exception& e){SetWindowTextW(s->status,describeError(e).c_str());buttons(*s);}return DefWindowProcW(hwnd,message,w,l);
}
}
void showSavedPresets(HWND owner,PresetUiContext context,const std::filesystem::path& importFile){
 if(active){ShowWindow(active,SW_RESTORE);SetForegroundWindow(active);if(!importFile.empty()){auto s=reinterpret_cast<State*>(GetWindowLongPtrW(active,GWLP_USERDATA));if(s->busy||s->confirming){SetWindowTextW(s->status,L"Finish or cancel the current operation before importing another preset file.");return;}if(s->rollbackPending()){SetWindowTextW(s->status,s->rollbackFailure().c_str());return;}s->context=std::move(context);s->importing=true;s->mapping=false;clearMapping(*s);layout(*s);operation(*s,[s,importFile](std::stop_token stop){s->pending=readDesktopPackage(importFile,stop);s->refreshed=listPresets(s->context.profile/L"presets",stop);s->result=Result::Mapping;});}return;}
 WNDCLASSW cls{};cls.lpfnWndProc=procedure;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"VolturaSavedPresets";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hIcon=LoadIconW(cls.hInstance,MAKEINTRESOURCEW(101));RegisterClassW(&cls);auto stateOwner=std::make_unique<State>();auto s=stateOwner.get();s->context=std::move(context);s->dpi=GetDpiForWindow(owner);s->font=CreateFontW(-s->px(16),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Segoe UI");
 MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(owner,MONITOR_DEFAULTTONEAREST),&monitor);
 constexpr DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_THICKFRAME|WS_CLIPCHILDREN;constexpr DWORD extended=WS_EX_CONTROLPARENT|WS_EX_COMPOSITED;RECT initial{0,0,s->px(750),s->px(716)};AdjustWindowRectExForDpi(&initial,style,FALSE,extended,s->dpi);int width=std::min(int(initial.right-initial.left),int(monitor.rcWork.right-monitor.rcWork.left)),height=std::min(int(initial.bottom-initial.top),int(monitor.rcWork.bottom-monitor.rcWork.top));auto window=CreateWindowExW(extended,cls.lpszClassName,L"Presets",style,monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left-width)/2,monitor.rcWork.top+(monitor.rcWork.bottom-monitor.rcWork.top-height)/2,width,height,owner,nullptr,cls.hInstance,s);if(!window||!IsWindow(window))throw std::runtime_error("Cannot open Saved presets.");s->ownedByWindow=true;stateOwner.release();active=window;
 try{
 add(*s,L"STATIC",L"Presets",0,0,20,16,660,26);s->list=add(*s,L"LISTBOX",L"",LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|LBS_DISABLENOSCROLL|WS_BORDER|WS_VSCROLL|WS_TABSTOP,listId,20,48,660,112);
 add(*s,L"STATIC",L"Name (required)",0,0,20,172,660,24);add(*s,L"EDIT",L"My desktop",ES_AUTOHSCROLL|WS_TABSTOP,nameId,20,198,660,28);
 add(*s,L"STATIC",L"Description (optional)",0,0,20,234,660,24);add(*s,L"EDIT",L"",ES_MULTILINE|ES_WANTRETURN|ES_AUTOVSCROLL|WS_VSCROLL|WS_TABSTOP,descriptionId,20,260,660,64);
 add(*s,L"STATIC",L"Attribution (optional)",0,0,20,332,660,24);add(*s,L"EDIT",L"",ES_MULTILINE|ES_WANTRETURN|ES_AUTOVSCROLL|WS_VSCROLL|WS_TABSTOP,attributionId,20,358,660,54);
 struct Button {int id;const wchar_t* name;};Button row1[]={{applyId,L"Apply…"},{saveId,L"Save current…"},{importPresetId,L"Import preset…"},{exportId,L"Export selected…"}},row2[]={{updateId,L"Update selected"},{renameId,L"Rename / details"},{deleteId,L"Delete"},{currentExportId,L"Export current…"}};
 for(int i=0;i<4;i++){add(*s,L"BUTTON",row1[i].name,WS_TABSTOP,row1[i].id,20+i*168,428,156,36);add(*s,L"BUTTON",row2[i].name,WS_TABSTOP,row2[i].id,20+i*168,476,156,36);}add(*s,L"BUTTON",L"Close",WS_TABSTOP,closeId,524,524,156,36);
 s->status=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|SS_NOPREFIX,0,0,1,1,window,reinterpret_cast<HMENU>(36),cls.hInstance,nullptr);if(!s->status)throw std::runtime_error("Cannot create preset status control.");SetWindowSubclass(s->status,statusPaint,1,0);SendMessageW(s->status,WM_SETFONT,reinterpret_cast<WPARAM>(s->font),TRUE);s->scrollbar=ui::createScrollbar(window,35,[s](int value){s->scroll=value;layout(*s,true);});theme(*s);layout(*s);buttons(*s);ShowWindow(window,SW_SHOW);SetForegroundWindow(window);
 // Modeless dialog keyboard navigation, scoped to this window's thread.
 SetWindowSubclass(window,[](HWND hwnd,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR)->LRESULT{if(message==WM_GETMINMAXINFO){auto info=reinterpret_cast<MINMAXINFO*>(l);info->ptMinTrackSize={MulDiv(740,GetDpiForWindow(hwnd),96),MulDiv(360,GetDpiForWindow(hwnd),96)};return 0;}return DefSubclassProc(hwnd,message,w,l);},1,0);
 if(importFile.empty())operation(*s,[s](std::stop_token stop){refresh(*s,stop);});else{s->importing=true;operation(*s,[s,importFile](std::stop_token stop){s->pending=readDesktopPackage(importFile,stop);s->refreshed=listPresets(s->context.profile/L"presets",stop);s->result=Result::Mapping;});}
 }catch(...){DestroyWindow(window);throw;}
}
}
