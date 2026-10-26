#pragma once
#include "ui_theme.h"
#include <algorithm>
namespace worlds::ui {
namespace confirmation {
struct State {HWND window{},message{},yes{},no{};HFONT font{};UINT dpi=96;bool accepted=false;int px(int n)const{return MulDiv(n,dpi,96);}};
inline void layout(State& s){RECT client{};GetClientRect(s.window,&client);SetWindowPos(s.message,nullptr,s.px(24),s.px(22),client.right-s.px(48),client.bottom-s.px(102),SWP_NOZORDER|SWP_NOACTIVATE);SetWindowPos(s.yes,nullptr,client.right-s.px(304),client.bottom-s.px(60),s.px(130),s.px(36),SWP_NOZORDER|SWP_NOACTIVATE);SetWindowPos(s.no,nullptr,client.right-s.px(154),client.bottom-s.px(60),s.px(130),s.px(36),SWP_NOZORDER|SWP_NOACTIVATE);}
inline LRESULT CALLBACK procedure(HWND hwnd,UINT msg,WPARAM w,LPARAM l){auto s=reinterpret_cast<State*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));if(msg==WM_NCCREATE){s=static_cast<State*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);s->window=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(s));}if(!s)return DefWindowProcW(hwnd,msg,w,l);
 if(msg==WM_CLOSE){DestroyWindow(hwnd);return 0;}
 if(msg==WM_COMMAND){if(LOWORD(w)==IDYES){s->accepted=true;DestroyWindow(hwnd);return 0;}if(LOWORD(w)==IDNO||LOWORD(w)==IDCANCEL||LOWORD(w)==IDOK){DestroyWindow(hwnd);return 0;}}
 if(msg==WM_SIZE&&s->message){layout(*s);return 0;}
 if(msg==WM_ERASEBKGND){RECT r{};GetClientRect(hwnd,&r);FillRect(reinterpret_cast<HDC>(w),&r,backgroundBrush);return 1;}
 if(msg==WM_CTLCOLORSTATIC||msg==WM_CTLCOLORBTN){SetTextColor(reinterpret_cast<HDC>(w),foreground);SetBkColor(reinterpret_cast<HDC>(w),background);return reinterpret_cast<LRESULT>(backgroundBrush);}
 if(msg==WM_DRAWITEM){draw(*reinterpret_cast<DRAWITEMSTRUCT*>(l),s->font,false,s->dpi);return TRUE;}
 if(msg==WM_SETTINGCHANGE||msg==WM_THEMECHANGED){refresh(hwnd);return 0;}
 if(msg==WM_DPICHANGED){s->dpi=HIWORD(w);auto old=s->font;s->font=CreateFontW(-s->px(16),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Segoe UI");for(auto child:{s->message,s->yes,s->no})SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(s->font),TRUE);DeleteObject(old);auto r=reinterpret_cast<RECT*>(l);SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;}
 return DefWindowProcW(hwnd,msg,w,l);
}
}
inline bool confirm(HWND owner,const std::wstring& title,const std::wstring& message,const wchar_t* action){
 using namespace confirmation;State s;s.dpi=GetDpiForWindow(owner);s.font=CreateFontW(-s.px(16),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Segoe UI");
 WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=procedure;cls.lpszClassName=L"VolturaWorldConfirm";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hIcon=LoadIconW(cls.hInstance,MAKEINTRESOURCEW(101));RegisterClassW(&cls);
 RECT parent{};GetWindowRect(owner,&parent);RECT size{0,0,s.px(540),s.px(210)};AdjustWindowRectExForDpi(&size,WS_CAPTION|WS_SYSMENU,FALSE,WS_EX_DLGMODALFRAME,s.dpi);int width=size.right-size.left,height=size.bottom-size.top;MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(owner,MONITOR_DEFAULTTONEAREST),&monitor);int x=std::clamp(int((parent.left+parent.right-width)/2),int(monitor.rcWork.left),std::max(int(monitor.rcWork.left),int(monitor.rcWork.right)-width)),y=std::clamp(int((parent.top+parent.bottom-height)/2),int(monitor.rcWork.top),std::max(int(monitor.rcWork.top),int(monitor.rcWork.bottom)-height));
 auto hwnd=CreateWindowExW(WS_EX_DLGMODALFRAME|WS_EX_CONTROLPARENT|WS_EX_COMPOSITED,cls.lpszClassName,title.c_str(),WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN,x,y,width,height,owner,nullptr,cls.hInstance,&s);if(!hwnd){DeleteObject(s.font);return false;}
 s.message=CreateWindowW(L"STATIC",message.c_str(),WS_CHILD|WS_VISIBLE|SS_NOPREFIX,0,0,0,0,hwnd,nullptr,cls.hInstance,nullptr);s.yes=CreateWindowW(L"BUTTON",action,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,hwnd,reinterpret_cast<HMENU>(IDYES),cls.hInstance,nullptr);s.no=CreateWindowW(L"BUTTON",L"Cancel",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,hwnd,reinterpret_cast<HMENU>(IDNO),cls.hInstance,nullptr);for(auto child:{s.message,s.yes,s.no})SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(s.font),FALSE);BOOL darkTitle=dark;DwmSetWindowAttribute(hwnd,20,&darkTitle,sizeof(darkTitle));layout(s);
 bool enabled=IsWindowEnabled(owner);EnableWindow(owner,FALSE);ShowWindow(hwnd,SW_SHOW);SetForegroundWindow(hwnd);SetFocus(s.no);MSG msg{};bool quit=false;while(IsWindow(hwnd)){int result=GetMessageW(&msg,nullptr,0,0);if(result<=0){quit=result==0;break;}if(!IsDialogMessageW(hwnd,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}if(IsWindow(hwnd))DestroyWindow(hwnd);if(IsWindow(owner)){EnableWindow(owner,enabled);SetActiveWindow(owner);}DeleteObject(s.font);if(quit)PostQuitMessage(int(msg.wParam));return s.accepted;
}
}
