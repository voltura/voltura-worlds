#include "renderer.h"
#include <d3d12sdklayers.h>
#include "performance.h"
#include "desktop.h"
#include "hdr_oracle.h"
#include "json.hpp"
#include <iostream>
#include <fstream>
#include <limits>
using namespace worlds;
static void require(bool v,const char* text){if(!v)throw std::runtime_error(text);}
namespace worlds {
// Access limited to this diagnostic executable; normal playback has no synthetic overrides.
struct HdrTestHarness {
 static void draw(Renderer& r,std::array<float,3> c,bool card=false,bool orbit=false){
  FrameData frame;frame.width=float(r.width_);frame.height=float(r.height_);frame.pass=orbit?1.f:0.f;if(orbit)frame.brightness=0;r.begin();
  auto view=r.rtv_->GetCPUDescriptorHandleForHeapStart();view.ptr+=2*r.rtvStride_;
  r.transition(r.hdr_.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
  float value[]={c[0],c[1],c[2],.5f};r.list_->ClearRenderTargetView(view,value,0,nullptr);
  r.transition(r.hdr_.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  for(unsigned i=0;i<2;i++){
   r.transition(r.bloomTargets_[i].Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
   view=r.rtv_->GetCPUDescriptorHandleForHeapStart();view.ptr+=(3+i)*r.rtvStride_;float zero[4]{};r.list_->ClearRenderTargetView(view,zero,0,nullptr);
   r.transition(r.bloomTargets_[i].Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  }
  auto target=r.buffers_[r.frameIndex_].Get();r.transition(target,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);
  view=r.rtv_->GetCPUDescriptorHandleForHeapStart();view.ptr+=r.frameIndex_*r.rtvStride_;r.list_->OMSetRenderTargets(1,&view,FALSE,nullptr);
  D3D12_VIEWPORT vp{0,0,float(r.width_),float(r.height_),0,1};D3D12_RECT rect{0,0,LONG(r.width_),LONG(r.height_)};
  r.list_->RSSetViewports(1,&vp);r.list_->RSSetScissorRects(1,&rect);ID3D12DescriptorHeap* heaps[]={r.views_.Get()};r.list_->SetDescriptorHeaps(1,heaps);
  r.list_->SetGraphicsRootSignature(r.root_.Get());r.list_->SetGraphicsRoot32BitConstants(0,32,&frame,0);r.outputConstants();r.list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  r.list_->SetGraphicsRootDescriptorTable(2,r.gpu(66));r.list_->SetPipelineState(r.output_.hdr?r.postHdr_.Get():r.post_.Get());r.list_->DrawInstanced(3,1,0,0);
  if(card){r.list_->SetGraphicsRootDescriptorTable(3,r.gpu(66));r.list_->SetPipelineState(r.bodyStatisticsHdr_.Get());r.list_->DrawInstanced(3,1,0,0);}
  r.transition(target,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT);r.submit();
 }
};
}
static double half(uint16_t bits){unsigned e=(bits>>10)&31,m=bits&1023;double v=e==31?std::numeric_limits<double>::infinity():e?std::ldexp(1.+m/1024.,int(e)-15):std::ldexp(double(m),-24);return bits&32768?-v:v;}
static std::array<double,4> pixel(const std::filesystem::path& file){std::ifstream in(file,std::ios::binary);uint16_t values[4]{};in.read(reinterpret_cast<char*>(values),8);require(bool(in),"Read FP16 pixel");return {half(values[0]),half(values[1]),half(values[2]),half(values[3])};}
int wmain(int argc,wchar_t** argv){try{
 static_assert(sizeof(FrameData)==128);
 if(argc!=3&&argc!=4)throw std::runtime_error("Expected shaders and evidence directory");
 std::filesystem::path output=argv[2];std::filesystem::create_directories(output);CoInitializeEx(nullptr,COINIT_MULTITHREADED);
 require(!resolveOutput(OutputPreference::Auto,false,false,203,400).hdr,"Capable but inactive is SDR");
 require(!resolveOutput(OutputPreference::Sdr,false,true,203,400).hdr,"SDR override");
 require(!resolveOutput(OutputPreference::Auto,true,true,203,400).hdr,"Video override");
 require(activeHdrSignal(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)&&!activeHdrSignal(DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709)&&!activeHdrSignal(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709),"SDR ACM and mere capability are not active HDR");
 require(sdrWhiteNits(1000)==80&&sdrWhiteNits(2000)==160&&sdrWhiteNits(0)==0,"Documented white scale and validation");
 require(!resolveTargets(OutputPreference::Auto,false,{{true,203,1000},{false,80,400}}).hdr,"Cloned SDR target vetoes HDR");
 auto cloned=resolveTargets(OutputPreference::Auto,false,{{true,203,1000},{true,160,600}});require(cloned.hdr&&cloned.white==160&&cloned.peak==600,"Clones use minimum luminance limits");
 auto partial=resolveTargets(OutputPreference::Auto,false,{{true,80,200},{true,0,0}});require(partial.peak==200&&partial.white==80&&partial.peakFallback&&partial.whiteFallback,"Missing clone metadata does not raise known luminance limits");
 require(!resolveTargets(OutputPreference::Auto,false,{}).hdr,"Missing display APIs fail to SDR");
 auto fallback=resolveOutput(OutputPreference::Auto,false,true,NAN,0);require(fallback.white==203&&fallback.peak==400&&fallback.whiteFallback&&fallback.peakFallback,"Luminance fallbacks");
 require(resolveOutput(OutputPreference::Auto,false,true,500,300).white==300,"White bounded to peak");
 // Numerical continuity, neutral monotonicity and peak bounds independent of the GPU.
 for(bool orbit:{false,true})for(double white:{80.,203.,400.}){double previous=0;for(unsigned i=0;i<=10000;i++){
  double x=i*.01;auto v=hdrOracle({x,x,x},white,400,orbit);require(v[0]+1e-12>=previous&&std::isfinite(v[0])&&v[0]<=5&&v[0]==v[1]&&v[1]==v[2],"Neutral ramp monotonic, neutral, bounded");previous=v[0];
 }auto a=hdrOracle({1,1,1},white,400,orbit),b=hdrOracle({1.000001,1.000001,1.000001},white,400,orbit);require(std::abs(a[0]-b[0])<.00001,"Highlight threshold continuity");}
 require(hdrOracle({8,8,8},203,400)[0]>203./80,"Real highlight headroom");
 Monitor target;target.handle=reinterpret_cast<HMONITOR>(1);target.adapter.LowPart=1;auto different=target;different.handle=reinterpret_cast<HMONITOR>(2);require(!sameOutputSurface(target,different),"Target monitor identity matters");different=target;different.adapter.LowPart=2;require(!sameOutputSurface(target,different),"Target adapter LUID matters");different=target;different.refreshHz=24;require(sameOutputSurface(target,different),"Refresh/output changes preserve surface identity");
 Settings settings;Assignment saved;saved.output=OutputPreference::Sdr;settings.displays.emplace_back(L"display",saved);auto settingsDir=output/L"settings";saveSettings(settingsDir,settings);
 require(assignmentFor(loadSettings(settingsDir),L"display").output==OutputPreference::Sdr,"Output persists");
 auto path=settingsDir/L"settings.json";nlohmann::json json;{std::ifstream in(path);in>>json;}json["displays"][0].erase("output");{std::ofstream out(path);out<<json;}
 require(assignmentFor(loadSettings(settingsDir),L"display").output==OutputPreference::Auto,"Missing output migrates to Auto");
 json["displays"][0]["output"]="hdr";{std::ofstream out(path);out<<json;}bool invalidRejected=false;try{loadSettings(settingsDir);}catch(...){invalidRejected=true;}require(invalidRejected,"Unknown output preferences rejected");
 require(copyForDisplay(saved,Assignment{},24).output==OutputPreference::Sdr,"Apply to all copies preference");
 require(importForDisplay(Assignment{},saved).output==OutputPreference::Sdr,"World import preserves destination Output");
 int attempts=0;auto rollback=applyOutputWithFallback(resolveOutput(OutputPreference::Auto,false,true,203,400),[&](const OutputConfig& c){attempts++;if(c.hdr)throw std::runtime_error("Injected presentation failure");},[]{return false;});require(attempts==2&&!rollback.hdr&&!rollback.reason.empty(),"HDR failure rolls back to known SDR");
 attempts=0;auto retry=applyOutputWithFallback(resolveOutput(OutputPreference::Auto,false,true,203,400),[&](const OutputConfig&){attempts++;},[]{return false;});require(attempts==1&&retry.hdr,"Relevant change can retry HDR");
 nlohmann::json observed=nlohmann::json::array();for(const auto& monitor:enumerateMonitors()){auto state=discoverOutput(monitor,OutputPreference::Auto,false);observed.push_back({{"display",utf8(monitor.name)},{"hdrEligible",state.hdr},{"whiteNits",state.white},{"peakNits",state.peak},{"reason",utf8(state.reason)}});}
 auto context=GpuContext::acquire({},true);ComPtr<ID3D12InfoQueue> validation;require(SUCCEEDED(context->device.As(&validation)),"D3D12 debug validation must be enabled for HDR acceptance");validation.Reset();HWND window=CreateWindowW(L"STATIC",L"HDR diagnostic",WS_POPUP,0,0,320,180,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 HWND otherWindow=CreateWindowW(L"STATIC",L"SDR diagnostic",WS_POPUP,0,0,320,180,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);require(window&&otherWindow,"Diagnostic windows");
 double maxError=0,maximum=0;uint64_t sdrBytes=0,hdrBytes=0;
 {
 Renderer r(window,320,180,argv[1],true,false,context),other(otherWindow,320,180,argv[1],true,false,context);
 Assignment earth;earth.scene=Scene::Earth;FrameData frame;frame.time=120;
 r.render(earth,frame,false);other.render(earth,frame,false);auto texture=r.textureIdentity(396);auto otherTarget=other.targetIdentity();auto sharedBytes=context->sharedTextureBytes();
 auto cfg=resolveOutput(OutputPreference::Auto,false,true,203,400);sdrBytes=r.mutableAllocations();require(r.configureOutput(cfg)&&r.output().hdr,"FP16 transition");hdrBytes=r.mutableAllocations();for(unsigned repeat=0;repeat<32;repeat++)require(!r.configureOutput(cfg),"Unchanged output does not rebuild or retry");
 require(hdrBytes-sdrBytes==uint64_t(320)*180*8,"FP16 double-buffer memory");
 require(r.outputPipelineIdentity()!=other.outputPipelineIdentity(),"Format-dependent output PSOs");
 require(r.textureIdentity(396)==texture&&other.textureIdentity(396)==texture&&other.targetIdentity()==otherTarget&&context->sharedTextureBytes()==sharedBytes,"Output change retains shared textures and other surface");
 for(bool orbit:{false,true})for(float white:{80.f,203.f,400.f}){
  cfg.white=white;r.configureOutput(cfg);
  for(unsigned i=0;i<=80;i++){
   float x=i*.125f;std::array<float,3> c{x,x,x};HdrTestHarness::draw(r,c,false,orbit);r.capture(output/L"ramp.png");auto gpu=pixel(output/L"ramp.rgba16f");auto expected=hdrOracle({x,x,x},white,400,orbit);
   for(int channel=0;channel<3;channel++){double error=std::abs(gpu[channel]-expected[channel]);maxError=std::max(maxError,error);maximum=std::max(maximum,gpu[channel]);require(std::isfinite(gpu[channel])&&error<.006&&gpu[channel]<=5,"GPU mapping agrees with CPU oracle");}require(gpu[3]==1,"Opaque HDR output");
  }
  for(auto c:std::array<std::array<float,3>,3>{{{.5f,.25f,.75f},{8,2,1},{0,0,0}}}){
   HdrTestHarness::draw(r,c,false,orbit);r.capture(output/L"color.png");auto gpu=pixel(output/L"color.rgba16f");auto expected=hdrOracle({c[0],c[1],c[2]},white,400,orbit);for(int k=0;k<3;k++)require(std::abs(gpu[k]-expected[k])<.006,"Chromatic mapping");
  }
  for(auto card:std::array<std::array<float,3>,2>{{{.5f,.25f,.75f},{.03125f,0,1}}}){HdrTestHarness::draw(r,card,true,orbit);r.capture(output/L"card.png");auto gpu=pixel(output/L"card.rgba16f");auto background=hdrOracle({card[0],card[1],card[2]},white,400,orbit);
  for(int k=0;k<3;k++){double v=card[k],linear=v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);double expected=.5*(background[k]+linear*white/80);require(std::abs(gpu[k]-expected)<.006,"Linear SDR card decoding, white scaling and alpha blend");}}
 }
 for(unsigned cycle=0;cycle<16;cycle++){
  r.configureOutput(cycle%2?OutputConfig{}:cfg);r.resize(320+(cycle%3)*8,180+(cycle%3)*4);r.render(earth,frame,false);other.render(earth,frame,false);r.setPaused(true);r.setPaused(false);
  require(other.targetIdentity()==otherTarget&&r.textureIdentity(396)==texture,"Transitions isolated from other surface and textures");
 }
 require(r.mutableAllocations()==sdrBytes,"No retained output-format allocation history");
 if(argc==4){Assignment video;video.scene=Scene::Video;video.video=argv[3];video.fit=true;video.output=OutputPreference::Auto;
 for(unsigned cycle=0;cycle<4;cycle++){
  r.configureOutput(resolveOutput(video.output,true,true,203,400));require(!r.output().hdr,"World to video switches to SDR");
  auto end=GetTickCount64()+2000;while(GetTickCount64()<end){r.render(video,frame,false);if(r.videoPosition()>.05)break;Sleep(10);}
  require(r.videoPosition()>.05&&video.fit&&video.video==argv[3]&&video.output==OutputPreference::Auto,"Video plays and preferences survive output changes");
  r.setPaused(true);r.setPaused(false);r.configureOutput(cfg);r.render(earth,frame,false);require(r.output().hdr,"Video to built-in returns to HDR");
 }
 }
 // Three independent adaptive schedules using mixed effective output formats.
 HWND thirdWindow=CreateWindowW(L"STATIC",L"Third cadence",WS_POPUP,0,0,320,180,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 {Renderer third(thirdWindow,320,180,argv[1],true,false,context);std::array<Renderer*,3> renderers{&r,&other,&third};std::array<PerformanceState,3> perf;std::array<unsigned,3> counts{};
 for(unsigned i=0;i<3;i++){perf[i].configure({},i==0?24:i==1?30:60,Scene::Earth,0);perf[i].setActive(true,0);require(perf[i].adaptive(),"Output preserves adaptive scheduling");}
 for(unsigned tick=0;tick<1000;tick++){double now=tick*.001;if(tick==500){auto otherDeadline=perf[1].deadline.next;r.configureOutput(OutputConfig{});perf[0].restart(now);require(perf[1].deadline.next==otherDeadline,"Output change preserves unrelated schedule");}
 for(unsigned i=0;i<3;i++)if(perf[i].deadline.due(now)){renderers[i]->render(earth,frame,false);perf[i].deadline.submitted(now,perf[i].fps());counts[i]++;}}
 require(counts[0]==24&&counts[1]==30&&counts[2]==60,"Mixed HDR/SDR keeps independent 24/30/60 cadence");third.validateDebugMessages();}
 DestroyWindow(thirdWindow);
 r.validateDebugMessages();other.validateDebugMessages();
 r.idle();ComPtr<ID3D12Device5> modern;check(context->device.As(&modern),"Device removal interface");modern->RemoveDevice();require(r.deviceRemoved(),"Actual test device removed");
 }
 context.reset();context=GpuContext::acquire({},true);require(!context->removed(),"New device after removal");{Renderer recovered(window,320,180,argv[1],true,false,context);recovered.configureOutput(resolveOutput(OutputPreference::Auto,false,true,203,400));Assignment a;a.scene=Scene::Silk;recovered.render(a,FrameData{},false);recovered.validateDebugMessages();}
 DestroyWindow(window);DestroyWindow(otherWindow);CoUninitialize();
 std::ofstream report(output/L"results.json");report<<nlohmann::json({{"maxAbsoluteScRgbError",maxError},{"maximumScRgb",maximum},{"sdrMutableBytes",sdrBytes},{"hdrMutableBytes",hdrBytes},{"transitionCycles",16},{"physicalHdrVerified",false},{"observedOutputs",observed}}).dump(2);
 std::cout<<"PASS HDR CPU/GPU oracle, cards, FP16 memory, shared assets, format transitions, actual device removal/recovery. Maximum error "<<maxError<<"\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

