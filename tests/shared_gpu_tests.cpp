#include "renderer.h"
#include <iostream>
#include <fstream>
#include <thread>
using namespace worlds;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int wmain(int argc,wchar_t** argv){try{
 if(argc!=3)throw std::runtime_error("Expected shader and output directories");
 CoInitializeEx(nullptr,COINIT_MULTITHREADED);std::filesystem::path output=argv[2];std::filesystem::create_directories(output);
 auto context=GpuContext::acquire({},true);require(GpuContext::acquire({},true)==context,"Same adapter shares context");
 HWND a=CreateWindowW(L"STATIC",L"Shared GPU A",WS_POPUP,0,0,320,180,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 HWND b=CreateWindowW(L"STATIC",L"Shared GPU B",WS_POPUP,0,0,320,180,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);require(a&&b,"Create test windows");
 {
  auto first=std::make_unique<Renderer>(a,320,180,argv[1],true,false,context);
  auto second=std::make_unique<Renderer>(b,320,180,argv[1],true,false,context);
  require(first->pipelineIdentity()==second->pipelineIdentity(),"Shared immutable pipelines");require(first->targetIdentity()!=second->targetIdentity(),"Independent render targets");
  Assignment earth;earth.scene=Scene::Earth;FrameData data;data.width=320;data.height=180;data.time=120;
  first->render(earth,data,false);first->capture(output/L"first.png");auto one=context->sharedTextureBytes();
  second->render(earth,data,false);second->capture(output/L"second.png");
  require(first->textureIdentity(396)==second->textureIdentity(396)&&first->textureIdentity(397)==second->textureIdentity(397),"Same asset has one GPU allocation");
  require(context->sharedTextureBytes()==one&&one>0,"Shared asset accounting counted once");
  auto read=[](const auto& path){std::ifstream in(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(in),{});};require(read(output/L"first.png")==read(output/L"second.png"),"Independent renderers produce identical fixed frames");
  first->resize(640,360);first->render(earth,data,false);second->render(earth,data,false);require(context->sharedTextureBytes()==one,"Resize retains shared texture identity");
  Assignment moon;moon.scene=Scene::Moon;first->render(moon,data,false);auto both=context->sharedTextureBytes();require(both>one,"Mixed scenes keep live assets only");
  first->validateDebugMessages();second->validateDebugMessages();
  // Independent simulations on a shared device must never alias mutable volumes.
  Assignment smoke;smoke.scene=Scene::Smoke;first->setGrid(24);second->setGrid(32);
  first->render(smoke,data,false);second->render(smoke,data,false);
  require(first->simulationIdentity()&&second->simulationIdentity()&&first->simulationIdentity()!=second->simulationIdentity(),"Independent Smoke volumes");
  second->render(earth,data,false);first->render(moon,data,false);
  for(unsigned cycle=0;cycle<8;cycle++){first->resize(320+cycle*8,180+cycle*4);first->render(cycle%2?earth:moon,data,false);second->render(earth,data,false);}
  first->render(moon,data,false);first->validateDebugMessages();second->validateDebugMessages();first.reset();require(context->sharedTextureBytes()==one,"Last Moon reference releases its assets");
  second->render(earth,data,false);second->validateDebugMessages();second.reset();require(context->sharedTextureBytes()==0,"Last display releases all immutable assets");
  std::ofstream metrics(output/L"memory.txt");metrics<<"One Earth asset allocation: "<<one<<" bytes\nTwo displays: "<<one<<" shared asset bytes\nMixed Earth/Moon: "<<both<<" bytes\nAfter both displays close: 0 shared asset bytes\n";
 }
 // Actual device removal validates registry invalidation and resource teardown.
 {
  Renderer renderer(a,320,180,argv[1],true,false,context);Assignment silk;silk.scene=Scene::Silk;renderer.render(silk,FrameData{},false);renderer.idle();
  Microsoft::WRL::ComPtr<ID3D12Device5> modern;require(SUCCEEDED(context->device.As(&modern)),"Device removal test interface");modern->RemoveDevice();require(renderer.deviceRemoved(),"Device removal detected");
 }
 context.reset();auto recovered=GpuContext::acquire({},true);require(!recovered->removed(),"Removed context replaced");
 {Renderer renderer(b,320,180,argv[1],true,false,recovered);Assignment earth;earth.scene=Scene::Earth;renderer.render(earth,FrameData{},false);renderer.validateDebugMessages();}
 DestroyWindow(a);DestroyWindow(b);CoUninitialize();std::cout<<"PASS: shared devices/pipelines/textures, independent targets and pixels, memory accounting, resize, scene lifetime, device removal and recovery. Private desktop, not physical display proof.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
