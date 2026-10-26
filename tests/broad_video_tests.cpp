#include "renderer.h"
#include "video.h"
#include "video_format.h"
#include "package.h"
#include "json.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <psapi.h>
using namespace worlds;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void step(VideoPlayer& player,unsigned ms){auto end=GetTickCount64()+ms;while(GetTickCount64()<end){if(player.update())player.consumed();Sleep(5);}}
void ready(VideoPlayer& player){player.setPaused(false);auto end=GetTickCount64()+15000;while(!player.frame()&&GetTickCount64()<end){if(player.update())player.consumed();Sleep(5);}require(player.frame(),"No decoded frame");}
int wmain(int argc,wchar_t** argv){try{
 require(argc>=4,"Provide shader, fixture and evidence directories");CoInitializeEx(nullptr,COINIT_MULTITHREADED);
 std::filesystem::path shaders=argv[1],fixtures=argv[2],out=argv[3];std::filesystem::create_directories(out);nlohmann::json report;
 auto context=GpuContext::acquire({},true);
 if(argc==5){
  unsigned seconds=std::stoul(argv[4]);HWND window=CreateWindowW(L"STATIC",L"Video endurance",WS_POPUP,0,0,320,180,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
  Renderer renderer(window,320,180,shaders,true,false,context);Assignment a;a.scene=Scene::Video;a.video=(fixtures/L"h264.mkv").wstring();a.output=OutputPreference::Auto;
  auto start=GetTickCount64(),end=start+uint64_t(seconds)*1000;uint64_t frames=0,loops=0;double previous=0;std::vector<uint64_t> memory;unsigned lastSecond=UINT_MAX;
  while(GetTickCount64()<end){unsigned elapsed=unsigned((GetTickCount64()-start)/1000);a.videoSettings={double((elapsed/3)%3)*.5,double((elapsed/5)%3)*.5,videoSpeeds[(elapsed/4)%videoSpeeds.size()]};a.fit=(elapsed/7)%2==1;
   renderer.configureOutput(resolveOutput(a.output,true,true,203,400));renderer.render(a,FrameData{},false);auto position=renderer.videoPosition();if(position+.3<previous)loops++;previous=position;frames++;
   if(elapsed!=lastSecond&&elapsed%10==0){lastSecond=elapsed;PROCESS_MEMORY_COUNTERS_EX m{};GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&m),sizeof(m));memory.push_back(m.PrivateUsage);std::cout<<"Endurance "<<elapsed<<"/"<<seconds<<"s, frames="<<frames<<", loops="<<loops<<", private="<<m.PrivateUsage<<std::endl;renderer.validateDebugMessages();}
   Sleep(16);
  }
  require(loops>seconds/5,"Endurance did not loop");if(memory.size()>3)require(memory.back()<memory[2]+48ull*1024*1024,"Endurance memory grew excessively after warmup");report={{"seconds",seconds},{"frames",frames},{"loops",loops},{"privateBytes",memory},{"physicalDisplayVerified",false}};std::ofstream(out/L"endurance.json")<<report.dump(2);renderer.validateDebugMessages();DestroyWindow(window);std::cout<<"PASS: native video endurance\n";return 0;
 }
 bool ratesOnly=GetEnvironmentVariableW(L"WORLDS_VIDEO_RATE_ONLY",nullptr,0)>0;
 if(!ratesOnly){
 for(auto& entry:std::filesystem::directory_iterator(fixtures)){
  auto path=entry.path();if(path.extension()==L".json")continue;auto name=path.filename().string();std::cout<<"Probing "<<name<<std::endl;bool reject=name.starts_with("hdr")||name=="audio-only.mp4"||name=="protected.mp4"||name=="malformed.mp4";
  try{
   auto probe=preflightVideo(path,{},context->luid);require(!probe.preview.empty(),"No preflight preview");auto info=probe.info;
   report[name]={{"accepted",true},{"container",info.container},{"codec",info.codec},{"width",info.width},{"height",info.height},{"encodedWidth",info.encodedWidth},{"encodedHeight",info.encodedHeight},{"rotation",info.rotation},{"pixelAspectX",info.pixelAspectX},{"pixelAspectY",info.pixelAspectY},{"displayAspect",info.displayAspect}};
   std::ofstream preview(out/(name+".png"),std::ios::binary);preview.write(reinterpret_cast<const char*>(probe.preview.data()),probe.preview.size());
  }catch(const std::exception& error){report[name]={{"accepted",false},{"reason",error.what()}};require(name!="h264.mp4"&&name!="rates.mp4"&&name!="markers.mp4","H.264 baseline must decode");}
  if(reject&&report[name]["accepted"].get<bool>())throw std::runtime_error("Unexpected video acceptance: "+name);
 }
 require(!report["hdr.mp4"]["accepted"].get<bool>(),"HDR rejection");
 std::ofstream(out/L"capabilities.json")<<report.dump(2);
 WorldPackage legacy;legacy.title="Legacy video";legacy.assignment.scene=Scene::Video;
 auto read=[](const auto& path){std::ifstream in(path,std::ios::binary);return std::vector<unsigned char>(std::istreambuf_iterator<char>(in),{});};
 legacy.preview=read(out/L"h264.mp4.png");legacy.video=read(fixtures/L"h264.mp4");writePackage(out/L"legacy.world",legacy);require(readPackage(out/L"legacy.world").assignment.videoSettings==VideoSettings{},"Legacy video package defaults");
 for(auto name:{L"hevc.mp4",L"hdr.mp4",L"hdr-vui.mp4"}){legacy.video=read(fixtures/name);bool rejected=false;try{writePackage(out/L"invalid.world",legacy);}catch(...){rejected=true;}require(rejected,"Legacy packages reject non-AVC/HDR video");}
 legacy.video=read(fixtures/L"h264.mp4");legacy.assignment.videoSettings.speed=2;bool rejected=false;try{writePackage(out/L"invalid.world",legacy);}catch(...){rejected=true;}require(rejected,"Legacy exports cannot silently discard speed");
 std::stop_source cancelled;cancelled.request_stop();bool stopped=false;try{preflightVideo(fixtures/L"h264.mp4",cancelled.get_token(),context->luid);}catch(const VideoError& e){stopped=e.kind==VideoErrorKind::Cancelled;}require(stopped,"Preflight cancellation");
 bool timedOut=false;try{preflightVideo(fixtures/L"h264.mp4",{},context->luid,0);}catch(const VideoError& e){timedOut=e.kind==VideoErrorKind::Timeout;}require(timedOut,"First-frame timeout");
 auto copiedRoot=out/L"managed";if(!std::filesystem::exists(copiedRoot))std::filesystem::create_directory(copiedRoot);
 {
  auto large=out/(L"cancel-source-"+std::to_wstring(GetCurrentProcessId())+L".mp4");std::filesystem::copy_file(fixtures/L"h264.mp4",large);
  const unsigned extra=256*1024*1024;{std::ofstream append(large,std::ios::binary|std::ios::app);const unsigned char box[]={16,0,0,0,'f','r','e','e'};append.write(reinterpret_cast<const char*>(box),8);}std::filesystem::resize_file(large,std::filesystem::file_size(large)+extra-8);
  auto cancelLibrary=out/(L"cancel-library-"+std::to_wstring(GetCurrentProcessId()));std::stop_source stop;std::atomic<bool> sawPending=false,finished=false;
  std::jthread cancel([&](std::stop_token token){auto deadline=GetTickCount64()+10000;while(!token.stop_requested()&&!finished&&GetTickCount64()<deadline){std::error_code error;if(std::filesystem::exists(cancelLibrary,error))for(auto& entry:std::filesystem::directory_iterator(cancelLibrary,error))if(entry.path().extension()==L".pending"){sawPending=true;stop.request_stop();return;}Sleep(1);}});
  bool cancelled=false;try{copyVideoToLibrary(cancelLibrary,large,stop.get_token(),context->luid);}catch(const VideoError& e){cancelled=e.kind==VideoErrorKind::Cancelled;}
  finished=true;cancel.request_stop();cancel.join();require(sawPending&&cancelled&&std::filesystem::is_empty(cancelLibrary),"Cancellation during staged import cleans all partial media");std::filesystem::remove(cancelLibrary);std::filesystem::remove(large);
  auto blocked=out/(L"blocked-library-"+std::to_wstring(GetCurrentProcessId()));{std::ofstream file(blocked);file<<"Existing file";}bool failed=false;try{copyVideoToLibrary(blocked,fixtures/L"h264.mp4",{},context->luid);}catch(...){failed=true;}require(failed&&std::filesystem::is_regular_file(blocked)&&std::filesystem::file_size(blocked)==13,"Copy destination failure preserves existing data");std::filesystem::remove(blocked);
 }
 for(auto name:{L"h264.mp4",L"h264.mov",L"h264.mkv",L"vp9.webm"})if(report[std::filesystem::path(name).string()]["accepted"].get<bool>()){
  auto copy=copyVideoToLibrary(copiedRoot,fixtures/name,{},context->luid);auto expected=inspectVideo(fixtures/name).container;require(copy.extension()==std::filesystem::path("."+expected),"Managed extension must match container");preflightVideo(copy,{},context->luid);
 }
 }
 {VideoPlayer player(context->device.Get(),context->queue.Get(),fixtures/L"rates.mp4");ready(player);
  for(double rate:videoSpeeds){player.setPaused(true);require(player.setPlaybackRate(rate),"Ready player rate rejected");double paused=player.position();step(player,100);require(std::abs(player.position()-paused)<.05,"Rate change resumed paused player");player.setPaused(false);double before=player.position();auto time=GetTickCount64();step(player,600);double dt=(GetTickCount64()-time)/1000.;double advance=player.position()-before;std::cout<<"Rate "<<rate<<": "<<advance<<"s / "<<dt<<"s, reported="<<player.playbackRate()<<", start="<<before<<std::endl;require(std::abs(advance-rate*dt)<.16,"Media time does not match selected playback speed");}
  bool rejected=false;try{player.setPlaybackRate(3);}catch(...){rejected=true;}require(rejected,"Unsupported rate validation");
 }
 {VideoPlayer slow(context->device.Get(),context->queue.Get(),fixtures/L"rates.mp4"),fast(context->device.Get(),context->queue.Get(),fixtures/L"rates.mp4");ready(slow);ready(fast);slow.setPlaybackRate(.5);fast.setPlaybackRate(2);double a=slow.position(),b=fast.position();auto end=GetTickCount64()+600;while(GetTickCount64()<end){if(slow.update())slow.consumed();if(fast.update())fast.consumed();Sleep(5);}require(fast.position()-b>(slow.position()-a)*2.5,"Independent rates for the same source");}
 HWND window=CreateWindowW(L"STATIC",L"Video crop",WS_POPUP,0,0,160,320,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 {Renderer renderer(window,160,320,shaders,true,false,context);Assignment video;video.scene=Scene::Video;video.video=(fixtures/L"markers.mp4").wstring();renderer.configureOutput(resolveOutput(OutputPreference::Auto,true,true,203,400));
  auto draw=[&]{auto end=GetTickCount64()+5000;do{renderer.render(video,FrameData{},false);if(renderer.hasVideoFrame())return;Sleep(5);}while(GetTickCount64()<end);throw std::runtime_error("Crop capture did not receive a decoded frame");};
  for(auto [name,x]:std::array<std::pair<const wchar_t*,double>,3>{{{L"left",0},{L"center",.5},{L"right",1}}}){
   video.videoSettings.positionX=x;draw();renderer.capture(out/(std::wstring(name)+L".png"));}
  video.fit=true;renderer.render(video,FrameData{},false);renderer.capture(out/L"fit.png");renderer.validateDebugMessages();require(!renderer.output().hdr,"All videos retain SDR output");
  renderer.resize(320,160);video.fit=false;video.video=(fixtures/L"markers-vertical.mp4").wstring();
  for(auto [name,y]:std::array<std::pair<const wchar_t*,double>,2>{{{L"top",0},{L"bottom",1}}}){video.videoSettings.positionY=y;draw();renderer.capture(out/(std::wstring(name)+L".png"));}
  Assignment world;world.scene=Scene::Earth;renderer.configureOutput(resolveOutput(OutputPreference::Auto,false,true,203,400));renderer.render(world,FrameData{},false);require(renderer.output().hdr,"Return from video restores HDR eligibility");renderer.validateDebugMessages();
 }
 DestroyWindow(window);std::cout<<"PASS: native format probes, cancellation, timeout, managed copies, all speeds, independent clocks, crop captures and HDR integration. Private desktop evidence.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
