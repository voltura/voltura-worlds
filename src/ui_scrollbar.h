#pragma once
#include "ui_theme.h"
#include <windowsx.h>
#include <functional>
#include <algorithm>
#include <stdexcept>
#include <memory>
namespace worlds::ui {
// Native scrollbar pixels never participate in this control's painting or input.
// As in Voltura Markdown, native edit/list windows provide state only; their
// scrollbar regions are clipped away, and a buffered sibling owns the track.
namespace scroll_detail {
struct Bar {HWND window{};SCROLLINFO info{sizeof(SCROLLINFO),SIF_ALL};std::function<void(int)> changed;bool ownedByWindow=false,dragging=false,hover=false,pageRepeat=false;int grab=0,repeat=0,repeatY=0,line=1;};
inline int maximum(const Bar& b){return b.info.nPage?std::max(b.info.nMin,b.info.nMax-int(b.info.nPage)+1):b.info.nMin;}
inline RECT track(const Bar& b){RECT r{};GetClientRect(b.window,&r);int arrow=std::min(int(r.bottom/2),int(r.right));r.top+=arrow;r.bottom-=arrow;return r;}
inline RECT thumb(const Bar& b){auto r=track(b);int length=std::max(0L,r.bottom-r.top);double range=std::max(1.,double(b.info.nMax)-b.info.nMin+1);int size=std::clamp(int(b.info.nPage/range*length),std::min(length,MulDiv(24,GetDpiForWindow(b.window),96)),length);int offset=int(double(b.info.nPos-b.info.nMin)/std::max(1,maximum(b)-b.info.nMin)*(length-size));r.top+=offset;r.bottom=r.top+size;int inset=std::max(2,MulDiv(3,GetDpiForWindow(b.window),96));r.left+=inset;r.right-=inset;return r;}
inline void position(Bar& b,int value){value=std::clamp(value,b.info.nMin,maximum(b));if(value==b.info.nPos)return;b.info.nPos=b.info.nTrackPos=value;InvalidateRect(b.window,nullptr,FALSE);if(b.changed)b.changed(value);}
inline void drag(Bar& b,int y){auto t=track(b),v=thumb(b);int travel=std::max(0L,(t.bottom-t.top)-(v.bottom-v.top));position(b,b.info.nMin+int(double(std::clamp(int(y-t.top-b.grab),0,travel))/std::max(1,travel)*(maximum(b)-b.info.nMin)));}
inline void paint(Bar& b,HDC target){RECT r{};GetClientRect(b.window,&r);if(r.right<=0||r.bottom<=0)return;auto dc=CreateCompatibleDC(target);auto bitmap=CreateCompatibleBitmap(target,r.right,r.bottom);auto old=SelectObject(dc,bitmap);FillRect(dc,&r,surfaceBrush);int arrow=std::min(int(r.bottom/2),int(r.right)),span=std::max(2,MulDiv(3,GetDpiForWindow(b.window),96));bool enabled=IsWindowEnabled(b.window)&&maximum(b)>b.info.nMin;chevron(dc,r.right/2,arrow/2,span,true,muted);chevron(dc,r.right/2,r.bottom-arrow/2,span,false,muted);if(enabled){auto v=thumb(b);auto brush=CreateSolidBrush(b.dragging||b.hover?accent:border);auto previous=SelectObject(dc,brush);auto pen=SelectObject(dc,GetStockObject(NULL_PEN));RoundRect(dc,v.left,v.top,v.right,v.bottom,span,span);SelectObject(dc,pen);SelectObject(dc,previous);DeleteObject(brush);}BitBlt(target,0,0,r.right,r.bottom,dc,0,0,SRCCOPY);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);}
inline LRESULT CALLBACK procedure(HWND hwnd,UINT msg,WPARAM w,LPARAM l){auto b=reinterpret_cast<Bar*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));if(msg==WM_NCCREATE){b=static_cast<Bar*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);b->window=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(b));}if(!b)return DefWindowProcW(hwnd,msg,w,l);
 if(msg==WM_NCDESTROY){SetWindowLongPtrW(hwnd,GWLP_USERDATA,0);if(b->ownedByWindow)delete b;return DefWindowProcW(hwnd,msg,w,l);}
 if(msg==WM_ERASEBKGND)return 1;
 if(msg==WM_PAINT||msg==WM_PRINTCLIENT){PAINTSTRUCT ps{};auto dc=msg==WM_PAINT?BeginPaint(hwnd,&ps):reinterpret_cast<HDC>(w);paint(*b,dc);if(msg==WM_PAINT)EndPaint(hwnd,&ps);return 0;}
 if(msg==SBM_GETSCROLLINFO){auto info=reinterpret_cast<SCROLLINFO*>(l);if(!info)return FALSE;auto mask=info->fMask;*info=b->info;info->fMask=mask;return TRUE;}
 if(msg==SBM_SETSCROLLINFO){auto info=reinterpret_cast<SCROLLINFO*>(l);if(!info)return 0;auto previous=b->info;if(info->fMask&SIF_RANGE){b->info.nMin=info->nMin;b->info.nMax=info->nMax;}if(info->fMask&SIF_PAGE)b->info.nPage=info->nPage;if(info->fMask&SIF_POS)b->info.nPos=info->nPos;b->info.nPos=std::clamp(b->info.nPos,b->info.nMin,maximum(*b));b->info.nTrackPos=b->info.nPos;if(previous.nMin!=b->info.nMin||previous.nMax!=b->info.nMax||previous.nPage!=b->info.nPage||previous.nPos!=b->info.nPos)InvalidateRect(hwnd,nullptr,FALSE);return b->info.nPos;}
 if(msg==WM_LBUTTONDOWN||msg==WM_LBUTTONDBLCLK){if(maximum(*b)<=b->info.nMin)return 0;SetFocus(hwnd);SetCapture(hwnd);int y=GET_Y_LPARAM(l);auto t=track(*b),v=thumb(*b);b->repeat=0;b->pageRepeat=false;b->repeatY=y;if(y<t.top)b->repeat=-b->line;else if(y>=t.bottom)b->repeat=b->line;else if(y<v.top){b->pageRepeat=true;b->repeat=-int(std::max(1u,b->info.nPage));}else if(y>=v.bottom){b->pageRepeat=true;b->repeat=int(std::max(1u,b->info.nPage));}else{b->dragging=true;b->grab=y-v.top;}if(b->repeat){position(*b,b->info.nPos+b->repeat);SetTimer(hwnd,1,350,nullptr);}InvalidateRect(hwnd,nullptr,FALSE);return 0;}
 if(msg==WM_TIMER&&w==1){if(b->repeat&&GetCapture()==hwnd){auto v=thumb(*b);if(b->pageRepeat&&((b->repeat<0&&b->repeatY>=v.top)||(b->repeat>0&&b->repeatY<v.bottom))){b->repeat=0;KillTimer(hwnd,1);return 0;}position(*b,b->info.nPos+b->repeat);SetTimer(hwnd,1,60,nullptr);}return 0;}
 if(msg==WM_MOUSEMOVE){b->repeatY=GET_Y_LPARAM(l);if(b->dragging)drag(*b,GET_Y_LPARAM(l));if(!b->hover){b->hover=true;TRACKMOUSEEVENT event{sizeof(event),TME_LEAVE,hwnd,0};TrackMouseEvent(&event);InvalidateRect(hwnd,nullptr,FALSE);}return 0;}
 if(msg==WM_LBUTTONUP||msg==WM_CANCELMODE||msg==WM_CAPTURECHANGED){b->dragging=false;b->repeat=0;KillTimer(hwnd,1);if(msg!=WM_CAPTURECHANGED&&GetCapture()==hwnd)ReleaseCapture();InvalidateRect(hwnd,nullptr,FALSE);return 0;}
 if(msg==WM_MOUSELEAVE){b->hover=false;InvalidateRect(hwnd,nullptr,FALSE);return 0;}
 if(msg==WM_MOUSEWHEEL){auto target=reinterpret_cast<HWND>(GetPropW(hwnd,L"VolturaScrollTarget"));SendMessageW(target?target:GetParent(hwnd),msg,w,l);return 0;}
 if(msg==WM_GETDLGCODE)return DLGC_WANTARROWS;
 if(msg==WM_KEYDOWN){int value=b->info.nPos;switch(w){case VK_UP:value-=b->line;break;case VK_DOWN:value+=b->line;break;case VK_PRIOR:value-=int(b->info.nPage);break;case VK_NEXT:value+=int(b->info.nPage);break;case VK_HOME:value=b->info.nMin;break;case VK_END:value=maximum(*b);break;default:return DefWindowProcW(hwnd,msg,w,l);}position(*b,value);return 0;}
 if(msg==WM_THEMECHANGED||msg==WM_ENABLE||msg==WM_SIZE||msg==WM_SETFOCUS||msg==WM_KILLFOCUS){InvalidateRect(hwnd,nullptr,FALSE);return 0;}return DefWindowProcW(hwnd,msg,w,l);
}
}
inline HWND createScrollbar(HWND parent,int id,std::function<void(int)> changed){WNDCLASSW cls{};cls.lpfnWndProc=scroll_detail::procedure;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"VolturaThemedScrollbar";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&cls);auto state=std::make_unique<scroll_detail::Bar>();state->changed=std::move(changed);auto window=CreateWindowExW(0,cls.lpszClassName,L"Scroll",WS_CHILD|WS_TABSTOP|WS_CLIPSIBLINGS,0,0,1,1,parent,reinterpret_cast<HMENU>(INT_PTR(id)),cls.hInstance,state.get());if(!window||!IsWindow(window))throw std::runtime_error("Cannot create themed scrollbar.");state->ownedByWindow=true;state.release();return window;}
inline void updateScrollbar(HWND bar,const SCROLLINFO& info){SendMessageW(bar,SBM_SETSCROLLINFO,FALSE,reinterpret_cast<LPARAM>(&info));}
inline void scrollbarLine(HWND bar,int pixels){auto state=reinterpret_cast<scroll_detail::Bar*>(GetWindowLongPtrW(bar,GWLP_USERDATA));if(state)state->line=std::max(1,pixels);}
inline SCROLLINFO scrollbarInfo(HWND bar){SCROLLINFO info{sizeof(info),SIF_ALL};SendMessageW(bar,SBM_GETSCROLLINFO,0,reinterpret_cast<LPARAM>(&info));return info;}
namespace scroll_detail {
struct Binding {HWND native{},bar{};bool syncing=false,list=false;};
inline void sync(Binding& b){if(b.syncing||!IsWindow(b.native))return;b.syncing=true;RECT bounds{};GetWindowRect(b.native,&bounds);int width=bounds.right-bounds.left,height=bounds.bottom-bounds.top,dpi=GetDpiForWindow(b.native),edge=1,barWidth=GetSystemMetricsForDpi(SM_CXVSCROLL,dpi);int left=std::max(edge,width-edge-barWidth);
 auto region=CreateRectRgn(0,0,width,height),excluded=CreateRectRgn(left,edge,std::max(left,width-edge),std::max(edge,height-edge));CombineRgn(region,region,excluded,RGN_DIFF);DeleteObject(excluded);auto current=CreateRectRgn(0,0,0,0);bool same=GetWindowRgn(b.native,current)!=ERROR&&EqualRgn(current,region);DeleteObject(current);if(same)DeleteObject(region);else if(!SetWindowRgn(b.native,region,FALSE))DeleteObject(region);
 POINT p{bounds.left+left,bounds.top+edge};ScreenToClient(GetParent(b.native),&p);RECT actual{};GetWindowRect(b.bar,&actual);MapWindowPoints(nullptr,GetParent(b.native),reinterpret_cast<POINT*>(&actual),2);bool visible=(GetWindowLongPtrW(b.native,GWL_STYLE)&WS_VISIBLE)!=0;int bw=std::max(1,width-edge-left),bh=std::max(1,height-2*edge);if(actual.left!=p.x||actual.top!=p.y||actual.right-actual.left!=bw||actual.bottom-actual.top!=bh||((GetWindowLongPtrW(b.bar,GWL_STYLE)&WS_VISIBLE)!=0)!=visible)SetWindowPos(b.bar,nullptr,p.x,p.y,bw,bh,SWP_NOACTIVATE|SWP_NOZORDER|(visible?SWP_SHOWWINDOW:SWP_HIDEWINDOW));SCROLLINFO info{sizeof(info),SIF_ALL};GetScrollInfo(b.native,SB_VERT,&info);updateScrollbar(b.bar,info);if(IsWindowEnabled(b.bar)!=IsWindowEnabled(b.native))EnableWindow(b.bar,IsWindowEnabled(b.native));b.syncing=false;}
inline LRESULT CALLBACK boundProcedure(HWND hwnd,UINT msg,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data){auto b=reinterpret_cast<Binding*>(data);if(msg==WM_NCDESTROY){RemovePropW(hwnd,L"VolturaScrollBinding");RemoveWindowSubclass(hwnd,boundProcedure,98);if(IsWindow(b->bar))DestroyWindow(b->bar);delete b;return DefSubclassProc(hwnd,msg,w,l);}if(msg==WM_SIZE||msg==WM_WINDOWPOSCHANGED)sync(*b);auto result=DefSubclassProc(hwnd,msg,w,l);if(msg==WM_SIZE||msg==WM_WINDOWPOSCHANGED||msg==WM_SHOWWINDOW||msg==WM_VSCROLL||msg==WM_MOUSEWHEEL||msg==WM_KEYDOWN||msg==WM_CHAR||msg==WM_SETTEXT||msg==EM_REPLACESEL||msg==EM_LINESCROLL||msg==EM_SCROLLCARET||msg==LB_ADDSTRING||msg==LB_RESETCONTENT||msg==LB_SETTOPINDEX||msg==WM_ENABLE||msg==WM_PAINT)sync(*b);return result;}
}
inline HWND fieldScrollbar(HWND native){auto b=reinterpret_cast<scroll_detail::Binding*>(GetPropW(native,L"VolturaScrollBinding"));return b?b->bar:nullptr;}
inline void bindFieldScrollbar(HWND native){
 if(fieldScrollbar(native))return;
 auto owner=std::make_unique<scroll_detail::Binding>();auto b=owner.get();b->native=native;
 wchar_t cls[32]{};GetClassNameW(native,cls,32);b->list=!wcscmp(cls,L"ListBox");
 b->bar=createScrollbar(GetParent(native),0,[b](int value){if(b->list)SendMessageW(b->native,LB_SETTOPINDEX,value,0);else{auto first=SendMessageW(b->native,EM_GETFIRSTVISIBLELINE,0,0);SendMessageW(b->native,EM_LINESCROLL,0,value-first);}scroll_detail::sync(*b);});
 try{
  if(!SetPropW(b->bar,L"VolturaScrollTarget",native)||!SetPropW(native,L"VolturaScrollBinding",b)||!SetWindowSubclass(native,scroll_detail::boundProcedure,98,reinterpret_cast<DWORD_PTR>(b)))throw std::runtime_error("Cannot bind field scrollbar.");
 }catch(...){RemovePropW(native,L"VolturaScrollBinding");DestroyWindow(b->bar);throw;}
 // The native field's destruction hook owns the binding only after all links
 // exist. Do not clip the native scrollbar before that ownership is installed.
 owner.release();scroll_detail::sync(*b);
}
}
