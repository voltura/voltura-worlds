#include "shell_ipc.h"
#include <sddl.h>
#include <vector>
#include <deque>
namespace worlds::shellmenu {
namespace {
struct Handle { HANDLE value=INVALID_HANDLE_VALUE;explicit Handle(HANDLE h=INVALID_HANDLE_VALUE):value(h){}~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}operator HANDLE()const{return value;} };
std::wstring userSid(){
 Handle token; if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token.value))return {};
 DWORD size=0;GetTokenInformation(token,TokenUser,nullptr,0,&size);std::vector<BYTE> bytes(size);
 if(!size||!GetTokenInformation(token,TokenUser,bytes.data(),size,&size))return {};
 LPWSTR sid=nullptr;if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(bytes.data())->User.Sid,&sid))return {};
 std::wstring result=sid;LocalFree(sid);return result;
}
std::wstring pipeName(){DWORD session=0;if(!ProcessIdToSessionId(GetCurrentProcessId(),&session))return {};auto sid=userSid();if(sid.empty())return {};return L"\\\\.\\pipe\\Voltura.Worlds.Menu.v1."+sid+L"."+std::to_wstring(session);}
bool sameUser(DWORD pid){
 Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid)),token;
 if(!process.value||!OpenProcessToken(process,TOKEN_QUERY,&token.value))return false;
 DWORD size=0;GetTokenInformation(token,TokenUser,nullptr,0,&size);std::vector<BYTE> bytes(size);
 if(!size||!GetTokenInformation(token,TokenUser,bytes.data(),size,&size))return false;
 LPWSTR sid=nullptr;if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(bytes.data())->User.Sid,&sid))return false;
 bool match=userSid()==sid;LocalFree(sid);DWORD ours=0,theirs=0;
 return match&&ProcessIdToSessionId(GetCurrentProcessId(),&ours)&&ProcessIdToSessionId(pid,&theirs)&&ours==theirs;
}
bool transfer(HANDLE pipe,void* bytes,DWORD size,bool write,DWORD timeout){
 Handle event(CreateEventW(nullptr,TRUE,FALSE,nullptr));if(!event.value)return false;
 OVERLAPPED operation{};operation.hEvent=event;DWORD done=0;
 BOOL ok=write?WriteFile(pipe,bytes,size,&done,&operation):ReadFile(pipe,bytes,size,&done,&operation);
 if(!ok&&GetLastError()==ERROR_IO_PENDING){
  if(WaitForSingleObject(event,timeout)!=WAIT_OBJECT_0){CancelIoEx(pipe,&operation);GetOverlappedResult(pipe,&operation,&done,TRUE);return false;}
  ok=GetOverlappedResult(pipe,&operation,&done,FALSE);
 }
 return ok&&done==size;
}
bool valid(const Request& r){return r.magic==requestMagic&&r.version==protocolVersion&&r.invoke<=1&&wcsnlen_s(r.command,std::size(r.command))<std::size(r.command)&&wcsnlen_s(r.displayId,std::size(r.displayId))<std::size(r.displayId)&&(!r.invoke||validCommand(r.command));}
}
struct Server::Impl {
 std::mutex mutex;State state;std::deque<Request> requests;std::jthread thread;HWND controller{};
 void serve(std::stop_token stop){
  auto name=pipeName(),sid=userSid();if(name.empty()||sid.empty())return;
  PSECURITY_DESCRIPTOR descriptor=nullptr;
  if(!ConvertStringSecurityDescriptorToSecurityDescriptorW((L"D:P(A;;GA;;;"+sid+L")").c_str(),SDDL_REVISION_1,&descriptor,nullptr))return;
  SECURITY_ATTRIBUTES security{sizeof(security),descriptor,FALSE};
  Handle pipe(CreateNamedPipeW(name.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,sizeof(Response),sizeof(Request),0,&security));
  LocalFree(descriptor);if(pipe.value==INVALID_HANDLE_VALUE)return;
  while(!stop.stop_requested()){
   Handle event(CreateEventW(nullptr,TRUE,FALSE,nullptr));if(!event.value)break;
   OVERLAPPED operation{};operation.hEvent=event;bool connected=ConnectNamedPipe(pipe,&operation)!=FALSE;
   if(!connected){auto error=GetLastError();if(error==ERROR_PIPE_CONNECTED)connected=true;else if(error==ERROR_IO_PENDING){
    while(!stop.stop_requested()&&WaitForSingleObject(event,50)==WAIT_TIMEOUT){}
    DWORD done=0;if(stop.stop_requested())CancelIoEx(pipe,&operation);
    connected=GetOverlappedResult(pipe,&operation,&done,TRUE)!=FALSE;
   }}
   if(connected&&!stop.stop_requested()){
    ULONG pid=0;Request request{};
    if(GetNamedPipeClientProcessId(pipe,&pid)&&sameUser(pid)&&transfer(pipe,&request,sizeof(request),false,100)&&valid(request)){
     Response response{};{std::lock_guard lock(mutex);response.state=state;}
     // A stalled UI must not expose stale controls. Opening a menu never launches.
     if(GetTickCount64()-response.state.timestamp>2500)response.state={};
     if(request.invoke&&response.state.processId){
      std::lock_guard lock(mutex);
      if(requests.size()<32){
       requests.push_back(request);
       if(PostMessageW(controller,invokeMessage,0,0))response.accepted=1;
       else requests.pop_back();
      }
     }
     if(transfer(pipe,&response,sizeof(response),true,100)){
      // Do not disconnect before the client has consumed the buffered reply.
      DWORD acknowledged=0;transfer(pipe,&acknowledged,sizeof(acknowledged),false,100);
     }
    }
   }
   DisconnectNamedPipe(pipe);
  }
 }
};
Server::Server():impl(std::make_unique<Impl>()){}
Server::~Server(){stop();}
bool Server::start(HWND controller){stop();impl->controller=controller;impl->thread=std::jthread([this](std::stop_token stop){try{impl->serve(stop);}catch(...){}});return true;}
void Server::publish(const State& state){std::lock_guard lock(impl->mutex);impl->state=state;}
std::optional<Request> Server::takeRequest(){std::lock_guard lock(impl->mutex);if(impl->requests.empty())return {};auto request=impl->requests.front();impl->requests.pop_front();return request;}
void Server::stop(){if(impl->thread.joinable()){impl->thread.request_stop();impl->thread.join();}}
bool exchange(const Request& request,Response& response,const std::filesystem::path& executable){
 try{
  auto name=pipeName();if(name.empty())return false;
  // Explorer can query the three independent menu roots concurrently. Give
  // their short cached-state exchanges time to serialize on the same pipe.
  Handle pipe;auto deadline=GetTickCount64()+100;
  for(;;){
   pipe.value=CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED|SECURITY_SQOS_PRESENT|SECURITY_IDENTIFICATION,nullptr);
   if(pipe.value!=INVALID_HANDLE_VALUE)break;
   if(GetLastError()!=ERROR_PIPE_BUSY)return false;
   auto now=GetTickCount64();if(now>=deadline||!WaitNamedPipeW(name.c_str(),DWORD(deadline-now)))return false;
  }
  ULONG pid=0;if(!GetNamedPipeServerProcessId(pipe,&pid)||!sameUser(pid))return false;
  Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid));wchar_t path[32768]{};DWORD length=DWORD(std::size(path));
  if(!process.value||!QueryFullProcessImageNameW(process,0,path,&length)||_wcsicmp(path,executable.c_str()))return false;
  DWORD mode=PIPE_READMODE_MESSAGE;if(!SetNamedPipeHandleState(pipe,&mode,nullptr,nullptr))return false;
  if(!transfer(pipe,const_cast<Request*>(&request),sizeof(request),true,100)||!transfer(pipe,&response,sizeof(response),false,100))return false;
  DWORD acknowledged=requestMagic;if(!transfer(pipe,&acknowledged,sizeof(acknowledged),true,100))return false;
  return response.state.version==protocolVersion&&response.state.processId==pid&&response.state.displayCount<=std::size(response.state.displays)&&GetTickCount64()-response.state.timestamp<=2500;
 }catch(...){return false;}
}
}
