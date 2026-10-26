#pragma once
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <string>

namespace worlds::ui {
inline bool dark=false;
inline COLORREF background{},surface{},foreground{},muted{},accent{},border{};
inline HBRUSH backgroundBrush{},surfaceBrush{};
inline LRESULT CALLBACK sliderPaint(HWND hwnd,UINT msg,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR){
 if(msg==WM_ERASEBKGND)return 1;
 if(msg==WM_PAINT||msg==WM_PRINTCLIENT){
  PAINTSTRUCT ps{};HDC dc=msg==WM_PAINT?BeginPaint(hwnd,&ps):reinterpret_cast<HDC>(w);
  RECT client{},channel{},thumb{};GetClientRect(hwnd,&client);FillRect(dc,&client,backgroundBrush);
  SendMessageW(hwnd,TBM_GETCHANNELRECT,0,reinterpret_cast<LPARAM>(&channel));SendMessageW(hwnd,TBM_GETTHUMBRECT,0,reinterpret_cast<LPARAM>(&thumb));
  int dpi=GetDpiForWindow(hwnd),x=(thumb.left+thumb.right)/2,y=(thumb.top+thumb.bottom)/2,thickness=std::max(2,MulDiv(2,dpi,96)),radius=MulDiv(7,dpi,96);
  RECT track{channel.left,y-thickness,channel.right,y+thickness};auto brush=CreateSolidBrush(border);FillRect(dc,&track,brush);DeleteObject(brush);
  track.right=x;brush=CreateSolidBrush(IsWindowEnabled(hwnd)?accent:muted);FillRect(dc,&track,brush);auto old=SelectObject(dc,brush);auto pen=SelectObject(dc,GetStockObject(NULL_PEN));Ellipse(dc,x-radius,y-radius,x+radius,y+radius);SelectObject(dc,old);SelectObject(dc,pen);DeleteObject(brush);
  if(GetFocus()==hwnd){InflateRect(&client,-1,-1);brush=CreateSolidBrush(border);FrameRect(dc,&client,brush);DeleteObject(brush);}
  if(msg==WM_PAINT)EndPaint(hwnd,&ps);return 0;
 }
 auto result=DefSubclassProc(hwnd,msg,w,l);
 if(msg==WM_SETFOCUS||msg==WM_KILLFOCUS||msg==WM_ENABLE||msg==TBM_SETPOS||msg==WM_MOUSEMOVE||msg==WM_LBUTTONUP||msg==WM_KEYDOWN)InvalidateRect(hwnd,nullptr,FALSE);
 if(msg==WM_NCDESTROY)RemoveWindowSubclass(hwnd,sliderPaint,1);return result;
}
inline void frame(HDC dc,RECT r,COLORREF color){auto brush=CreateSolidBrush(color);FrameRect(dc,&r,brush);DeleteObject(brush);}
inline void chevron(HDC dc,int x,int y,int span,bool up,COLORREF color){
 auto pen=CreatePen(PS_SOLID,1,color);auto old=SelectObject(dc,pen);int direction=up?-1:1;
 MoveToEx(dc,x-span,y-direction*span/2,nullptr);LineTo(dc,x,y+direction*span/2);LineTo(dc,x+span,y-direction*span/2);SelectObject(dc,old);DeleteObject(pen);
}
inline void listFrame(HWND hwnd,HDC supplied=nullptr,bool drawBorder=true){
 HDC dc=supplied?supplied:GetWindowDC(hwnd);RECT window{};GetWindowRect(hwnd,&window);RECT r{0,0,window.right-window.left,window.bottom-window.top};if(drawBorder)frame(dc,r,border);
 SCROLLBARINFO info{sizeof(info)};
 if(GetScrollBarInfo(hwnd,OBJID_VSCROLL,&info)&&!(info.rgstate[0]&STATE_SYSTEM_INVISIBLE)){
  auto bar=info.rcScrollBar;OffsetRect(&bar,-window.left,-window.top);FillRect(dc,&bar,surfaceBrush);
  RECT thumb{bar.left+3,bar.top+info.xyThumbTop,bar.right-3,bar.top+info.xyThumbBottom};auto brush=CreateSolidBrush(border);FillRect(dc,&thumb,brush);DeleteObject(brush);
  int span=MulDiv(3,GetDpiForWindow(hwnd),96);chevron(dc,(bar.left+bar.right)/2,bar.top+info.dxyLineButton/2,span,true,muted);chevron(dc,(bar.left+bar.right)/2,bar.bottom-info.dxyLineButton/2,span,false,muted);
 }
 if(!supplied)ReleaseDC(hwnd,dc);
}
inline LRESULT CALLBACK comboList(HWND hwnd,UINT msg,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR){
 if(msg==WM_ERASEBKGND){RECT r{};GetClientRect(hwnd,&r);FillRect(reinterpret_cast<HDC>(w),&r,surfaceBrush);return 1;}
 auto result=DefSubclassProc(hwnd,msg,w,l);
 if(msg==WM_NCPAINT||msg==WM_PAINT)listFrame(hwnd);
 if(msg==WM_PRINT&&(l&PRF_NONCLIENT))listFrame(hwnd,reinterpret_cast<HDC>(w));
 if(msg==WM_NCDESTROY)RemoveWindowSubclass(hwnd,comboList,1);return result;
}
// Keep native text editing, selection, accessibility and scrolling, but use the
// same flat surface, border and focus accent as the library's combo fields.
inline LRESULT CALLBACK fieldChrome(HWND hwnd,UINT msg,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR){
 auto result=DefSubclassProc(hwnd,msg,w,l);
 if(msg==WM_NCPAINT||msg==WM_PAINT||(msg==WM_PRINT&&(l&PRF_NONCLIENT))){
  HDC dc=msg==WM_PRINT?reinterpret_cast<HDC>(w):GetWindowDC(hwnd);listFrame(hwnd,dc);
  RECT window{};GetWindowRect(hwnd,&window);RECT edge{0,0,window.right-window.left,window.bottom-window.top};frame(dc,edge,GetFocus()==hwnd&&IsWindowEnabled(hwnd)?accent:border);
  if(msg!=WM_PRINT)ReleaseDC(hwnd,dc);
 }
 if(msg==WM_SETFOCUS||msg==WM_KILLFOCUS||msg==WM_ENABLE)RedrawWindow(hwnd,nullptr,nullptr,RDW_INVALIDATE|RDW_FRAME);
 if(msg==WM_NCDESTROY)RemoveWindowSubclass(hwnd,fieldChrome,1);return result;
}
inline void themeCombo(HWND hwnd){
 // Disable native chrome; keep the native control's input and accessibility.
 SetWindowTheme(hwnd,L"",L"");COMBOBOXINFO info{sizeof(info)};
 if(GetComboBoxInfo(hwnd,&info)){SetWindowTheme(info.hwndList,L"",L"");SetWindowSubclass(info.hwndList,comboList,1,0);RedrawWindow(info.hwndList,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_FRAME);}
 InvalidateRect(hwnd,nullptr,TRUE);
}
inline LRESULT CALLBACK comboBorder(HWND hwnd,UINT msg,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR){
 if(msg==WM_CTLCOLORLISTBOX){SetTextColor(reinterpret_cast<HDC>(w),foreground);SetBkColor(reinterpret_cast<HDC>(w),surface);return reinterpret_cast<LRESULT>(surfaceBrush);}
 if(msg==WM_ERASEBKGND)return 1;
 if(msg==WM_PAINT||msg==WM_PRINTCLIENT){
  PAINTSTRUCT ps{};HDC dc=msg==WM_PAINT?BeginPaint(hwnd,&ps):reinterpret_cast<HDC>(w);
  RECT r{};GetClientRect(hwnd,&r);FillRect(dc,&r,surfaceBrush);bool enabled=IsWindowEnabled(hwnd),focus=GetFocus()==hwnd;
  frame(dc,r,focus&&enabled?accent:border);COMBOBOXINFO info{sizeof(info)};GetComboBoxInfo(hwnd,&info);
  int span=MulDiv(4,GetDpiForWindow(hwnd),96);chevron(dc,(info.rcButton.left+info.rcButton.right)/2,(r.top+r.bottom)/2,span,false,enabled?foreground:muted);
  int index=int(SendMessageW(hwnd,CB_GETCURSEL,0,0));auto length=SendMessageW(hwnd,CB_GETLBTEXTLEN,index,0);
  if(length>=0&&length<32768){std::wstring text(size_t(length)+1,L'\0');SendMessageW(hwnd,CB_GETLBTEXT,index,reinterpret_cast<LPARAM>(text.data()));
   RECT label=r;label.left+=MulDiv(10,GetDpiForWindow(hwnd),96);label.right=info.rcButton.left-4;SetBkMode(dc,TRANSPARENT);SetTextColor(dc,enabled?foreground:muted);
   auto old=SelectObject(dc,reinterpret_cast<HFONT>(SendMessageW(hwnd,WM_GETFONT,0,0)));DrawTextW(dc,text.c_str(),-1,&label,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);SelectObject(dc,old);
  }
  if(msg==WM_PAINT)EndPaint(hwnd,&ps);return 0;
 }
 auto result=DefSubclassProc(hwnd,msg,w,l);
 if(msg==WM_SETFOCUS||msg==WM_KILLFOCUS||msg==WM_ENABLE||msg==CB_SETCURSEL||msg==CB_SHOWDROPDOWN)InvalidateRect(hwnd,nullptr,FALSE);
 if(msg==WM_NCDESTROY)RemoveWindowSubclass(hwnd,comboBorder,1);return result;
}
inline void refresh(HWND window=nullptr,int darkOverride=-1){
 DWORD light=1,bytes=sizeof(light);
 RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&bytes);
 dark=darkOverride<0?!light:darkOverride!=0; background=dark?RGB(17,22,32):RGB(245,247,251);surface=dark?RGB(28,36,49):RGB(255,255,255);
 foreground=dark?RGB(235,241,250):RGB(24,35,53);muted=dark?RGB(160,176,197):RGB(85,102,124);
 accent=dark?RGB(99,218,231):RGB(0,105,127);border=dark?RGB(65,82,105):RGB(188,202,217);
 if(backgroundBrush)DeleteObject(backgroundBrush);if(surfaceBrush)DeleteObject(surfaceBrush);
 backgroundBrush=CreateSolidBrush(background);surfaceBrush=CreateSolidBrush(surface);
 if(window){BOOL enabled=dark;DwmSetWindowAttribute(window,20,&enabled,sizeof(enabled));EnumChildWindows(window,[](HWND child,LPARAM)->BOOL{wchar_t name[32]{};GetClassNameW(child,name,32);if(!wcscmp(name,L"ComboBox"))themeCombo(child);return TRUE;},0);RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_ERASE);}
}
inline void draw(DRAWITEMSTRUCT& d,HFONT font,bool selected=false,UINT dpi=96){
 bool menu=d.CtlType==ODT_MENU,combo=d.CtlType==ODT_COMBOBOX,list=d.CtlType==ODT_LISTBOX;
 bool hot=(d.itemState&ODS_SELECTED)!=0,disabled=(d.itemState&ODS_DISABLED)!=0;
 COLORREF fill=hot?(dark?RGB(47,73,91):RGB(218,237,243)):surface;
 HBRUSH brush=CreateSolidBrush(fill);FillRect(d.hDC,&d.rcItem,brush);DeleteObject(brush);
 // Native popup rows can already have their new monitor scale while the
 // hidden owner's cached DPI/font still reflects the disconnected display.
 // Size only the text from the final row in the supplied drawing DC's units.
 HFONT popupFont{};
 if(menu){
  LOGFONTW description{};
  if(GetObjectW(font,sizeof(description),&description)){
   description.lfHeight=-MulDiv(d.rcItem.bottom-d.rcItem.top,16,36);
   description.lfWidth=0;
   popupFont=CreateFontIndirectW(&description);
  }
 }
 SetBkMode(d.hDC,TRANSPARENT);SetTextColor(d.hDC,disabled?muted:selected?accent:foreground);auto old=SelectObject(d.hDC,popupFont?popupFont:font);
 std::wstring text;
 if(menu){auto value=reinterpret_cast<const wchar_t*>(d.itemData);if(value)text=value;}
 else if(combo){if(d.itemID!=UINT(-1)){auto length=SendMessageW(d.hwndItem,CB_GETLBTEXTLEN,d.itemID,0);if(length>=0&&length<32768){text.resize(size_t(length)+1);SendMessageW(d.hwndItem,CB_GETLBTEXT,d.itemID,reinterpret_cast<LPARAM>(text.data()));}}}
 else if(list){if(d.itemID!=UINT(-1)){auto length=SendMessageW(d.hwndItem,LB_GETTEXTLEN,d.itemID,0);if(length>=0&&length<32768){text.resize(size_t(length)+1);SendMessageW(d.hwndItem,LB_GETTEXT,d.itemID,reinterpret_cast<LPARAM>(text.data()));}}}
 else {text.resize(size_t(GetWindowTextLengthW(d.hwndItem))+1);GetWindowTextW(d.hwndItem,text.data(),int(text.size()));}
 RECT r=d.rcItem;r.left+=menu?MulDiv(40,dpi,96):12;r.right-=menu?MulDiv(28,dpi,96):12;
 if(menu&&(d.itemState&ODS_CHECKED)){RECT mark=d.rcItem;mark.left+=MulDiv(10,dpi,96);mark.right=mark.left+MulDiv(18,dpi,96);DrawTextW(d.hDC,L"✓",-1,&mark,DT_VCENTER|DT_SINGLELINE|DT_CENTER);}
 DrawTextW(d.hDC,text.c_str(),-1,&r,DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|(combo||menu||list?DT_LEFT:DT_CENTER));SelectObject(d.hDC,old);
 if(popupFont)DeleteObject(popupFont);
 if(!menu&&!combo&&!list){
  // Use the control's edge for focus, keeping the label free of inner outlines.
  bool focused=!disabled&&(d.itemState&ODS_FOCUS)&&!(d.itemState&ODS_NOFOCUSRECT);
  int width=std::max(1,MulDiv(selected||focused?2:1,dpi,96)),inset=width/2;
  auto pen=CreatePen(PS_SOLID,width,selected||focused?accent:border);auto p=SelectObject(d.hDC,pen);auto b=SelectObject(d.hDC,GetStockObject(NULL_BRUSH));
  RoundRect(d.hDC,d.rcItem.left+inset,d.rcItem.top+inset,d.rcItem.right-inset,d.rcItem.bottom-inset,MulDiv(8,dpi,96),MulDiv(8,dpi,96));SelectObject(d.hDC,b);SelectObject(d.hDC,p);DeleteObject(pen);
 }
}
// Windows draws submenu arrows separately using its small system-menu glyph.
// Repaint that gutter after native painting, retaining native menu navigation.
inline void drawMenuArrows(HWND window,HDC supplied=nullptr){
 MENUBARINFO bar{sizeof(bar)};if(!GetMenuBarInfo(window,OBJID_CLIENT,0,&bar)||!bar.hMenu)return;
 HDC dc=supplied?supplied:GetDC(window);UINT dpi=GetDpiForWindow(window);
 for(int i=0;i<GetMenuItemCount(bar.hMenu);i++)if(GetSubMenu(bar.hMenu,i)){
  RECT r{};if(!GetMenuItemRect(nullptr,bar.hMenu,i,&r))continue;
  MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&r),2);
  UINT state=GetMenuState(bar.hMenu,i,MF_BYPOSITION);bool hot=(state&MF_HILITE)!=0;
  r.left=r.right-MulDiv(24,dpi,96);
  auto brush=CreateSolidBrush(hot?(dark?RGB(47,73,91):RGB(218,237,243)):surface);FillRect(dc,&r,brush);DeleteObject(brush);
  int x=r.right-MulDiv(12,dpi,96),y=(r.top+r.bottom)/2,span=MulDiv(4,dpi,96);
  auto pen=CreatePen(PS_SOLID,std::max(1,MulDiv(2,dpi,96)),foreground);auto old=SelectObject(dc,pen);
  MoveToEx(dc,x-span/2,y-span,nullptr);LineTo(dc,x+span/2,y);LineTo(dc,x-span/2,y+span);SelectObject(dc,old);DeleteObject(pen);
 }
 if(!supplied)ReleaseDC(window,dc);
}
inline LRESULT CALLBACK menuPaint(HWND window,UINT msg,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR){
 auto result=DefSubclassProc(window,msg,w,l);
 if(msg==WM_PAINT||msg==WM_NCPAINT||msg==WM_MOUSEMOVE||msg==WM_KEYDOWN||msg==WM_TIMER)drawMenuArrows(window);
 if(msg==WM_PRINTCLIENT)drawMenuArrows(window,reinterpret_cast<HDC>(w));
 if(msg==WM_NCDESTROY)RemoveWindowSubclass(window,menuPaint,1);
 return result;
}
inline LRESULT CALLBACK menuCreation(int code,WPARAM w,LPARAM l){
 if(code==HCBT_CREATEWND){HWND window=reinterpret_cast<HWND>(w);wchar_t name[32]{};GetClassNameW(window,name,32);
  if(!wcscmp(name,L"#32768"))SetWindowSubclass(window,menuPaint,1,0);
 }
 return CallNextHookEx(nullptr,code,w,l);
}
// Explorer owns the overflow policy. Only request promotion of this executable,
// never alter the user's global notification-area preferences.
inline std::wstring normalizedPath(const std::wstring& value){
 wchar_t full[32768]{};DWORD length=GetFullPathNameW(value.c_str(),32768,full,nullptr);
 std::wstring result=length>0&&length<32768?full:value;while(!result.empty()&&(result.back()==L'\\'||result.back()==L'/'))result.pop_back();return result;
}
inline bool promoteTray(const std::wstring& executable,bool& changed){
 changed=false;
 HKEY root{};if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Control Panel\\NotifyIconSettings",0,KEY_READ,&root)!=ERROR_SUCCESS)return false;
 bool found=false;wchar_t keyName[256];auto expected=normalizedPath(executable);
 for(DWORD index=0;;index++){
  DWORD count=256;LONG result=RegEnumKeyExW(root,index,keyName,&count,nullptr,nullptr,nullptr,nullptr);if(result==ERROR_NO_MORE_ITEMS)break;if(result!=ERROR_SUCCESS)continue;
  HKEY key{};if(RegOpenKeyExW(root,keyName,0,KEY_QUERY_VALUE|KEY_SET_VALUE,&key)!=ERROR_SUCCESS)continue;
  wchar_t path[32768]{};DWORD size=sizeof(path);
  if(RegGetValueW(key,nullptr,L"ExecutablePath",RRF_RT_REG_SZ|RRF_RT_REG_EXPAND_SZ,nullptr,path,&size)==ERROR_SUCCESS&&_wcsicmp(normalizedPath(path).c_str(),expected.c_str())==0){
   DWORD current=0,bytes=sizeof(current);RegGetValueW(key,nullptr,L"IsPromoted",RRF_RT_REG_DWORD,nullptr,&current,&bytes);
   if(current==1)found=true;else{DWORD promoted=1;if(RegSetValueExW(key,L"IsPromoted",0,REG_DWORD,reinterpret_cast<const BYTE*>(&promoted),sizeof(promoted))==ERROR_SUCCESS){found=true;changed=true;}}
  }
  RegCloseKey(key);
 }RegCloseKey(root);return found;
}
}
