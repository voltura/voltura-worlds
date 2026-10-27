#include "renderer.h"
#include "video.h"
#include "dds.h"
#include <d3dcompiler.h>
#include <d3d12sdklayers.h>
#include <wincodec.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cstring>
#include "json.hpp"
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion=619; __declspec(dllexport) extern const char* D3D12SDKPath=".\\D3D12\\"; }
namespace worlds {
void check(HRESULT hr,const char* op){if(FAILED(hr)){std::ostringstream s;s<<op<<" (0x"<<std::hex<<unsigned(hr)<<")";throw std::runtime_error(s.str());}}
static std::vector<char> shader(const std::filesystem::path& dir,const wchar_t* name){std::ifstream f(dir/name,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Precompiled shader missing");auto n=f.tellg();std::vector<char>b((size_t)n);f.seekg(0);f.read(b.data(),n);return b;}
static D3D12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE type){D3D12_HEAP_PROPERTIES h{};h.Type=type;return h;}
static D3D12_RESOURCE_DESC bufferDesc(uint64_t size){D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=size;d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;return d;}
Renderer::Renderer(HWND hwnd,unsigned w,unsigned h,const std::filesystem::path& dir,bool debug,bool forceSingleSample,std::shared_ptr<GpuContext> context):window_(hwnd),width_(w),height_(h){
 // A failed constructor does not run ~Renderer. Release native handles on
 // partial setup failure as well as through the normal destructor.
 struct SetupHandles {HANDLE& event;HANDLE& latency;bool complete=false;~SetupHandles(){if(!complete){if(event)CloseHandle(event);if(latency)CloseHandle(latency);}}} setupHandles{event_,latency_};
 assetDirectory_=dir.parent_path()/L"assets";
 context_=context?std::move(context):GpuContext::acquire({},debug);
 if(!context_->factory||!context_->factory->IsCurrent()){ComPtr<IDXGIFactory6> refreshed;check(CreateDXGIFactory2(0,IID_PPV_ARGS(&refreshed)),"Refresh DXGI factory");context_->factory=refreshed;}
 factory_=context_->factory;device_=context_->device;queue_=context_->queue;adapterName_=context_->adapterName;
 DXGI_SWAP_CHAIN_DESC1 sd{};sd.Width=w;sd.Height=h;sd.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.SampleDesc.Count=1;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.BufferCount=2;sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;sd.Scaling=DXGI_SCALING_STRETCH;sd.Flags=DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
 ComPtr<IDXGISwapChain1> sw;check(factory_->CreateSwapChainForHwnd(queue_.Get(),hwnd,&sd,nullptr,nullptr,&sw),"Create wallpaper swap chain");check(sw.As(&swap_),"Query swap chain");factory_->MakeWindowAssociation(hwnd,DXGI_MWA_NO_ALT_ENTER);check(swap_->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709),"Set SDR color space");swap_->SetMaximumFrameLatency(1);latency_=swap_->GetFrameLatencyWaitableObject();
 D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=6;check(device_->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtv_)),"Create render views");rtvStride_=device_->GetDescriptorHandleIncrementSize(hd.Type);
 hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_DSV;hd.NumDescriptors=2;check(device_->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&dsv_)),"Create depth views");
 hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=420;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;check(device_->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&views_)),"Create scene descriptors");descriptorStride_=device_->GetDescriptorHandleIncrementSize(hd.Type);
 check(device_->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence_)),"Create fence");event_=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!event_)throw std::runtime_error("Cannot create GPU wait event");
 for(auto& f:frames_)check(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&f.allocator)),"Create frame allocator");
 check(device_->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,frames_[0].allocator.Get(),nullptr,IID_PPV_ARGS(&list_)),"Create command list");list_->Close();
 D3D12_DESCRIPTOR_RANGE ranges[4]{};ranges[0]={D3D12_DESCRIPTOR_RANGE_TYPE_UAV,8,0,0,0};ranges[1]={D3D12_DESCRIPTOR_RANGE_TYPE_SRV,3,0,0,0};
 D3D12_ROOT_PARAMETER rp[6]{};rp[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;rp[0].Constants={0,0,32};rp[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
 ranges[2]={D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,3,0,0};
 ranges[3]={D3D12_DESCRIPTOR_RANGE_TYPE_SRV,2,4,0,0};
 for(int i=0;i<4;i++){rp[i+1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;rp[i+1].DescriptorTable={1,&ranges[i]};rp[i+1].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;}
 D3D12_STATIC_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.MaxAnisotropy=1;sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_NEVER;sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
 D3D12_STATIC_SAMPLER_DESC samplers[3]={sampler,sampler,sampler};
 samplers[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
 samplers[1].ShaderRegister=1;samplers[1].Filter=D3D12_FILTER_ANISOTROPIC;samplers[1].MaxAnisotropy=8;samplers[1].AddressU=D3D12_TEXTURE_ADDRESS_MODE_WRAP;
 samplers[2].ShaderRegister=2;samplers[2].AddressU=samplers[2].AddressV=samplers[2].AddressW=D3D12_TEXTURE_ADDRESS_MODE_WRAP;
 rp[5].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;rp[5].Constants={1,0,4};rp[5].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
 D3D12_ROOT_SIGNATURE_DESC rd{6,rp,3,samplers,D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};ComPtr<ID3DBlob> blob,error;
 check(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error),"Serialize root signature");if(!context_->root)check(device_->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&context_->root)),"Create root signature");root_=context_->root;
 D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS colorSupport{DXGI_FORMAT_R16G16B16A16_FLOAT,2,D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE,0};
 D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS depthSupport{DXGI_FORMAT_D32_FLOAT,2,D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE,0};
 D3D12_FEATURE_DATA_FORMAT_SUPPORT resolveSupport{DXGI_FORMAT_R16G16B16A16_FLOAT};
 if(!forceSingleSample&&SUCCEEDED(device_->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&resolveSupport,sizeof(resolveSupport)))&&(resolveSupport.Support1&D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RESOLVE)&&SUCCEEDED(device_->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,&colorSupport,sizeof(colorSupport)))&&SUCCEEDED(device_->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,&depthSupport,sizeof(depthSupport)))&&colorSupport.NumQualityLevels&&depthSupport.NumQualityLevels)orbitSamples_=2;
 auto full=shader(dir,L"fullscreen.cso"),ov=shader(dir,L"orbit.cso"),bv=shader(dir,L"bloom_mesh.cso");
 auto pipeline=[&](const wchar_t* file,bool geometry,bool alpha,ComPtr<ID3D12PipelineState>& output,bool display=false,unsigned samples=1,bool folded=false,DXGI_FORMAT displayFormat=DXGI_FORMAT_R8G8B8A8_UNORM){std::wstring key=dir.wstring()+L"/"+file+L":"+std::to_wstring(geometry)+std::to_wstring(alpha)+std::to_wstring(display?displayFormat:DXGI_FORMAT_R16G16B16A16_FLOAT)+L":"+std::to_wstring(samples)+std::to_wstring(folded);
  if(auto found=context_->pipelines.find(key);found!=context_->pipelines.end()){output=found->second;return;}
  auto ps=shader(dir,file);D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root_.Get();pd.VS={folded?bv.data():geometry?ov.data():full.data(),folded?bv.size():geometry?ov.size():full.size()};pd.PS={ps.data(),ps.size()};pd.SampleMask=UINT_MAX;pd.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;pd.NumRenderTargets=1;pd.RTVFormats[0]=display?displayFormat:DXGI_FORMAT_R16G16B16A16_FLOAT;pd.DSVFormat=DXGI_FORMAT_D32_FLOAT;pd.SampleDesc.Count=samples;pd.RasterizerState.MultisampleEnable=samples>1;
  pd.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;pd.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;pd.RasterizerState.DepthClipEnable=TRUE;
  pd.DepthStencilState.DepthEnable=geometry;pd.DepthStencilState.DepthWriteMask=alpha?D3D12_DEPTH_WRITE_MASK_ZERO:D3D12_DEPTH_WRITE_MASK_ALL;pd.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_LESS_EQUAL;
  auto& b=pd.BlendState.RenderTarget[0];b.RenderTargetWriteMask=alpha?(D3D12_COLOR_WRITE_ENABLE_RED|D3D12_COLOR_WRITE_ENABLE_GREEN|D3D12_COLOR_WRITE_ENABLE_BLUE):D3D12_COLOR_WRITE_ENABLE_ALL;b.BlendEnable=alpha;b.SrcBlend=D3D12_BLEND_SRC_ALPHA;b.DestBlend=D3D12_BLEND_INV_SRC_ALPHA;b.BlendOp=D3D12_BLEND_OP_ADD;b.SrcBlendAlpha=D3D12_BLEND_ONE;b.DestBlendAlpha=D3D12_BLEND_ZERO;b.BlendOpAlpha=D3D12_BLEND_OP_ADD;
  check(device_->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&output)),"Create scene pipeline");context_->pipelines[key]=output;};
 // Earth and Mars use analytic coverage on fullscreen triangles; duplicate samples
 // add no edge information. Keep the exact same HDR format and shader shading.
 pipeline(L"earth.cso",false,true,earth_);pipeline(L"earth_space.cso",false,false,earthSpace_);
 pipeline(L"saturn.cso",false,false,saturn_);
 pipeline(L"uranus.cso",false,true,uranus_);pipeline(L"uranus_space.cso",false,false,uranusSpace_);
 pipeline(L"neptune.cso",false,true,neptune_);pipeline(L"neptune_space.cso",false,false,neptuneSpace_);
 pipeline(L"venus.cso",false,true,venus_);pipeline(L"venus_space.cso",false,false,venusSpace_);
 pipeline(L"jupiter.cso",false,true,jupiter_);pipeline(L"jupiter_space.cso",false,false,jupiterSpace_);
 pipeline(L"moon.cso",false,true,moon_);pipeline(L"moon_space.cso",false,false,moonSpace_);
 pipeline(L"sun.cso",false,false,sun_);
 pipeline(L"pluto.cso",false,true,pluto_);pipeline(L"pluto_space.cso",false,false,plutoSpace_);
 pipeline(L"mercury.cso",false,true,mercuryPlanet_);pipeline(L"mercury_space.cso",false,false,mercurySpace_);
 pipeline(L"mars.cso",false,true,mars_);pipeline(L"mars_space.cso",false,false,marsSpace_);
 pipeline(L"video.cso",false,false,videoPipeline_,true);
 pipeline(L"body_statistics.cso",false,true,bodyStatisticsPipeline_,true);
 pipeline(L"bloom_fold.cso",true,false,bloomSurface_,false,orbitSamples_,true);pipeline(L"bloom_backdrop.cso",false,false,bloomBackdrop_,false,orbitSamples_);
 pipeline(L"aurora.cso",false,false,aurora_);pipeline(L"space.cso",false,false,space_,false,orbitSamples_);pipeline(L"planet.cso",true,false,planet_,false,orbitSamples_);pipeline(L"planet.cso",true,true,atmosphere_,false,orbitSamples_);pipeline(L"smoke.cso",false,false,smoke_);pipeline(L"post.cso",false,false,post_,true);pipeline(L"bloom.cso",false,false,bloom_);
 pipeline(L"post.cso",false,false,postHdr_,true,1,false,DXGI_FORMAT_R16G16B16A16_FLOAT);
 pipeline(L"body_statistics.cso",false,true,bodyStatisticsHdr_,true,1,false,DXGI_FORMAT_R16G16B16A16_FLOAT);
 pipeline(L"forest_water.cso",false,false,forestWater_);
 const wchar_t* proceduralShaders[]={L"silk.cso",L"eclipse.cso",L"lattice.cso",L"drift.cso",L"contour.cso",L"plastic_metal.cso",L"ripples.cso"};
 for(unsigned i=0;i<proceduralWorlds_.size();i++)pipeline(proceduralShaders[i],false,false,proceduralWorlds_[i]);
 auto computePipeline=[&](const wchar_t* file,ComPtr<ID3D12PipelineState>& output){
  auto key=dir.wstring()+L"/compute/"+file;
  if(auto it=context_->pipelines.find(key);it!=context_->pipelines.end()){output=it->second;return;}
  auto code=shader(dir,file);D3D12_COMPUTE_PIPELINE_STATE_DESC cd{};cd.pRootSignature=root_.Get();cd.CS={code.data(),code.size()};check(device_->CreateComputePipelineState(&cd,IID_PPV_ARGS(&output)),"Create shared compute pipeline");context_->pipelines[key]=output;
 };
 computePipeline(L"fluid.cso",fluid_);computePipeline(L"smoke_coordinates.cso",coordinatePipeline_);computePipeline(L"smoke_detail.cso",detailPipeline_);
 D3D12_QUERY_HEAP_DESC qd{D3D12_QUERY_HEAP_TYPE_TIMESTAMP,QueryStride*2,0};check(device_->CreateQueryHeap(&qd,IID_PPV_ARGS(&query_)),"Create timing queries");auto hp=heap(D3D12_HEAP_TYPE_READBACK);auto bd=bufferDesc(QueryStride*2*sizeof(uint64_t));check(device_->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&queryReadback_)),"Create timing readback");queue_->GetTimestampFrequency(&frequency_);
 targets();
 D3D12_SHADER_RESOURCE_VIEW_DESC nullFine{};nullFine.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;nullFine.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;nullFine.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE3D;nullFine.Texture3D.MipLevels=1;device_->CreateShaderResourceView(nullptr,&nullFine,cpu(380));
 nullFine={};nullFine.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;nullFine.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;nullFine.Buffer.NumElements=1;nullFine.Buffer.StructureByteStride=8;device_->CreateShaderResourceView(nullptr,&nullFine,cpu(381));
 setupHandles.complete=true;
}
Renderer::~Renderer(){try{idle();}catch(...){}if(event_)CloseHandle(event_);if(latency_)CloseHandle(latency_);}
bool Renderer::tryPresent(){
 auto result=swap_->Present(0,DXGI_PRESENT_DO_NOT_WAIT);
 pendingPresent_=result==DXGI_ERROR_WAS_STILL_DRAWING;
 if(!pendingPresent_)check(result,"Present wallpaper without blocking");return !pendingPresent_;
}
bool Renderer::ready(){
 if(context_->removed())throw std::runtime_error("Graphics device removed");
 if(pendingPresent_){tryPresent();return false;}
 auto index=swap_->GetCurrentBackBufferIndex();if(frames_[index].fence&&fence_->GetCompletedValue()<frames_[index].fence)return false;
 return !latency_||WaitForSingleObject(latency_,0)==WAIT_OBJECT_0;
}
ID3D12Resource* Renderer::textureIdentity(unsigned slot)const{auto it=assetSlots_.find(slot);return it==assetSlots_.end()?nullptr:it->second;}
bool Renderer::cachedTexture(const std::wstring& key,ComPtr<ID3D12Resource>& texture,unsigned slot){
 auto found=context_->textures.find(key);if(found==context_->textures.end())return false;
 auto lease=found->second.lock();if(!lease)return false;
 texture=lease->resource;if(!textureLeases_.contains(texture.Get()))assetBytes_+=lease->bytes;textureLeases_[texture.Get()]=lease;assetSlots_[slot]=texture.Get();
 auto desc=texture->GetDesc();D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=desc.Format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=desc.MipLevels;device_->CreateShaderResourceView(texture.Get(),&srv,cpu(slot));return true;
}
void Renderer::retainTexture(const std::wstring& key,const ComPtr<ID3D12Resource>& texture,unsigned slot){
 auto lease=std::make_shared<SharedTexture>();lease->resource=texture;auto desc=texture->GetDesc();lease->bytes=device_->GetResourceAllocationInfo(0,1,&desc).SizeInBytes;
 context_->textures[key]=lease;textureLeases_[texture.Get()]=lease;assetSlots_[slot]=texture.Get();assetBytes_+=lease->bytes;
}
void Renderer::releaseAsset(ComPtr<ID3D12Resource>& texture){
 auto raw=texture.Get();if(!raw)return;
 for(auto it=assetSlots_.begin();it!=assetSlots_.end();)if(it->second==raw)it=assetSlots_.erase(it);else ++it;
 if(auto it=textureLeases_.find(raw);it!=textureLeases_.end()){assetBytes_-=it->second->bytes;textureLeases_.erase(it);}texture.Reset();context_->sharedTextureBytes();
}
void Renderer::wait(uint64_t value){
 if(!value)return;
 auto completed=fence_->GetCompletedValue();if(completed==UINT64_MAX)throw std::runtime_error("Graphics device removed while waiting for GPU");
 if(completed<value){check(fence_->SetEventOnCompletion(value,event_),"Wait for GPU fence");if(WaitForSingleObject(event_,10000)!=WAIT_OBJECT_0)throw std::runtime_error("GPU stopped responding");}
 if(fence_->GetCompletedValue()==UINT64_MAX)throw std::runtime_error("Graphics device removed while waiting for GPU");
}
void Renderer::idle(){if(!queue_||!fence_||context_->removed())return;check(queue_->Signal(fence_.Get(),++fenceValue_),"Signal GPU idle");wait(fenceValue_);}
void Renderer::validateDebugMessages(){
 idle();ComPtr<ID3D12InfoQueue> messages;if(FAILED(device_.As(&messages)))return;
 for(UINT64 i=0;i<messages->GetNumStoredMessagesAllowedByRetrievalFilter();i++){
  SIZE_T size{};check(messages->GetMessage(i,nullptr,&size),"Read GPU validation size");std::vector<char> storage(size);auto message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());check(messages->GetMessage(i,message,&size),"Read GPU validation message");
  if(message->Severity==D3D12_MESSAGE_SEVERITY_CORRUPTION||message->Severity==D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(std::string("D3D12 validation: ")+message->pDescription);
 }
 messages->ClearStoredMessages();
}
void Renderer::targets(){
 auto handle=rtv_->GetCPUDescriptorHandleForHeapStart();for(auto& b:buffers_){check(swap_->GetBuffer((UINT)(&b-buffers_.data()),IID_PPV_ARGS(&b)),"Get back buffer");device_->CreateRenderTargetView(b.Get(),nullptr,handle);handle.ptr+=rtvStride_;}
 D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=width_;d.Height=height_;d.DepthOrArraySize=1;d.MipLevels=1;d.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;d.SampleDesc.Count=1;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
 auto h=heap(D3D12_HEAP_TYPE_DEFAULT);
 check(device_->CreateCommittedResource(&h,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&hdr_)),"Create HDR scene target");hdr_->SetName(L"Linear HDR artwork");
 auto hdrView=rtv_->GetCPUDescriptorHandleForHeapStart();hdrView.ptr+=2*rtvStride_;device_->CreateRenderTargetView(hdr_.Get(),nullptr,hdrView);
 D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=d.Format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;device_->CreateShaderResourceView(hdr_.Get(),&srv,cpu(66));
 for(unsigned i=0;i<2;i++){
  d.Width=(width_+3)/4;d.Height=(height_+3)/4;
  check(device_->CreateCommittedResource(&h,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&bloomTargets_[i])),"Create bloom target");bloomTargets_[i]->SetName(i==0?L"Bloom vertical / downsample":L"Bloom horizontal");
  auto view=rtv_->GetCPUDescriptorHandleForHeapStart();view.ptr+=(3+i)*rtvStride_;device_->CreateRenderTargetView(bloomTargets_[i].Get(),nullptr,view);
  device_->CreateShaderResourceView(bloomTargets_[i].Get(),&srv,cpu(67+i));
 }
 allocationBytes_=uint64_t(width_)*height_*20;
}
void Renderer::makeDepthTarget(){
 if(depth_)return;
 D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=width_;d.Height=height_;d.DepthOrArraySize=1;d.MipLevels=1;d.Format=DXGI_FORMAT_D32_FLOAT;d.SampleDesc.Count=1;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
 auto h=heap(D3D12_HEAP_TYPE_DEFAULT);D3D12_CLEAR_VALUE clear{};clear.Format=d.Format;clear.DepthStencil.Depth=1;
 check(device_->CreateCommittedResource(&h,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_DEPTH_WRITE,&clear,IID_PPV_ARGS(&depth_)),"Create depth target");device_->CreateDepthStencilView(depth_.Get(),nullptr,dsv_->GetCPUDescriptorHandleForHeapStart());
 depthTargetBytes_=device_->GetResourceAllocationInfo(0,1,&d).SizeInBytes;
}
void Renderer::resize(unsigned w,unsigned h){if(!w||!h||(w==width_&&h==height_))return;idle();for(auto& b:buffers_)b.Reset();depth_.Reset();depthTargetBytes_=0;hdr_.Reset();orbitColor_.Reset();orbitDepth_.Reset();orbitTargetBytes_=0;for(auto& b:bloomTargets_)b.Reset();auto resizeOutput=[&](){check(swap_->ResizeBuffers(2,w,h,output_.format(),DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT),"Resize wallpaper");check(swap_->SetColorSpace1(output_.colorSpace()),"Restore output color space");};
 try{resizeOutput();}catch(...){if(context_->removed()||!output_.hdr)throw;output_.hdr=false;output_.reason=L"HDR presentation unavailable";resizeOutput();}
 width_=w;height_=h;pendingPresent_=false;targets();}
void Renderer::outputConstants(){float values[]={output_.hdr?1.f:0.f,output_.white/80.f,output_.peak/output_.white,0};list_->SetGraphicsRoot32BitConstants(5,4,values,0);}
bool Renderer::configureOutput(const OutputConfig& requested,bool retry){
 if(outputConfigured_&&requested==requestedOutput_&&!retry)return false;
 requestedOutput_=requested;outputConfigured_=true;OutputConfig next=requested;
 if(next.format()!=output_.format()){
  idle();pendingPresent_=false;
  for(auto& b:buffers_)b.Reset();
  auto apply=[&](const OutputConfig& c){
   for(auto& b:buffers_)b.Reset();
   check(swap_->ResizeBuffers(2,width_,height_,c.format(),DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT),"Change display output format");
   UINT support=0;check(swap_->CheckColorSpaceSupport(c.colorSpace(),&support),"Check display color space");
   if(!(support&DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT))throw std::runtime_error("HDR presentation unavailable");
   check(swap_->SetColorSpace1(c.colorSpace()),"Set display color space");
   auto view=rtv_->GetCPUDescriptorHandleForHeapStart();for(unsigned i=0;i<2;i++){
    check(swap_->GetBuffer(i,IID_PPV_ARGS(&buffers_[i])),"Get output buffer");device_->CreateRenderTargetView(buffers_[i].Get(),nullptr,view);view.ptr+=rtvStride_;
   }
  };
  next=applyOutputWithFallback(next,apply,[&]{return context_->removed();});
 }
 bool changed=output_!=next;output_=next;return changed;
}
void Renderer::makeOrbitTargets(){
 if(orbitSamples_==1||orbitColor_)return;
 D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=width_;d.Height=height_;d.DepthOrArraySize=1;d.MipLevels=1;d.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;d.SampleDesc.Count=orbitSamples_;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
 auto h=heap(D3D12_HEAP_TYPE_DEFAULT);ComPtr<ID3D12Resource> color,depth;
 check(device_->CreateCommittedResource(&h,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_RENDER_TARGET,nullptr,IID_PPV_ARGS(&color)),"Create multisample Orbit color");color->SetName(L"Orbit multisample HDR");
 uint64_t bytes=device_->GetResourceAllocationInfo(0,1,&d).SizeInBytes;
 d.Format=DXGI_FORMAT_D32_FLOAT;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
 D3D12_CLEAR_VALUE clear{};clear.Format=d.Format;clear.DepthStencil.Depth=1;
 check(device_->CreateCommittedResource(&h,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_DEPTH_WRITE,&clear,IID_PPV_ARGS(&depth)),"Create multisample Orbit depth");depth->SetName(L"Orbit multisample depth");
 bytes+=device_->GetResourceAllocationInfo(0,1,&d).SizeInBytes;
 auto rtv=rtv_->GetCPUDescriptorHandleForHeapStart();rtv.ptr+=5*rtvStride_;device_->CreateRenderTargetView(color.Get(),nullptr,rtv);
 auto dsv=dsv_->GetCPUDescriptorHandleForHeapStart();dsv.ptr+=device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);device_->CreateDepthStencilView(depth.Get(),nullptr,dsv);
 orbitColor_=std::move(color);orbitDepth_=std::move(depth);orbitTargetBytes_=bytes;
}
void Renderer::loadStars(bool foreground){
 if(starTexture_&&starForeground_==foreground)return;
 if(starTexture_){idle();releaseAsset(starTexture_);}
 loadDds(foreground?assetDirectory_/L"space"/L"runtime"/L"planet-sky.dds":assetDirectory_/L"orbit"/L"runtime"/L"milkyway-2020.dds",starTexture_,82);
 starForeground_=foreground;
 D3D12_SHADER_RESOURCE_VIEW_DESC empty{};empty.Format=DXGI_FORMAT_R8G8B8A8_UNORM;empty.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;empty.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;empty.Texture2D.MipLevels=1;
 device_->CreateShaderResourceView(nullptr,&empty,cpu(83));device_->CreateShaderResourceView(nullptr,&empty,cpu(84));
}
void Renderer::loadSaturn(){
 if(saturnColor_)return;
 loadStars(true);
 loadDds(assetDirectory_/L"saturn"/L"runtime"/L"saturn-color.dds",saturnColor_,405);
 D3D12_SHADER_RESOURCE_VIEW_DESC srv{};auto desc=starTexture_->GetDesc();srv.Format=desc.Format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=desc.MipLevels;
 device_->CreateShaderResourceView(starTexture_.Get(),&srv,cpu(406));
 srv.Format=DXGI_FORMAT_R8G8B8A8_UNORM;srv.Texture2D.MipLevels=1;device_->CreateShaderResourceView(nullptr,&srv,cpu(407));
}
void Renderer::loadJupiter(){
 if(jupiterColor_&&jupiterAtmosphere_)return;
 loadStars(true);
 // 402..404 follow Moon; no overlap with other scene tables.
 loadDds(assetDirectory_/L"jupiter"/L"runtime"/L"jupiter-color.dds",jupiterColor_,402);
 loadDds(assetDirectory_/L"jupiter"/L"runtime"/L"jupiter-atmosphere.dds",jupiterAtmosphere_,403);
 D3D12_SHADER_RESOURCE_VIEW_DESC empty{};empty.Format=DXGI_FORMAT_R8G8B8A8_UNORM;empty.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;empty.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;empty.Texture2D.MipLevels=1;
 device_->CreateShaderResourceView(nullptr,&empty,cpu(404));
}
void Renderer::loadMoon(){
 if(moonColor_&&moonNormal_&&moonEarth_)return;
 loadStars(true);
 // 399..401 follow Earth, without overlapping any Smoke tables.
 loadDds(assetDirectory_/L"moon"/L"runtime"/L"moon-color.dds",moonColor_,399);
 loadDds(assetDirectory_/L"moon"/L"runtime"/L"moon-normal.dds",moonNormal_,400);
 loadDds(assetDirectory_/L"moon"/L"runtime"/L"moon-earth.dds",moonEarth_,401);
}
void Renderer::loadForestWater(){
 if(forestReflection_)return;
 loadTexture(assetDirectory_/L"forest-water"/L"environment.png",forestReflection_,417,4096);
 D3D12_SHADER_RESOURCE_VIEW_DESC empty{};empty.Format=DXGI_FORMAT_R8G8B8A8_UNORM;empty.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;empty.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;empty.Texture2D.MipLevels=1;
 device_->CreateShaderResourceView(nullptr,&empty,cpu(418));device_->CreateShaderResourceView(nullptr,&empty,cpu(419));
}
void Renderer::loadSun(){
 if(sunImage_&&sunProminence_)return;
 // Dedicated three-SRV table following Pluto; no star texture is needed.
 loadDds(assetDirectory_/L"sun"/L"runtime"/L"sun-aia171.dds",sunImage_,414);
 loadDds(assetDirectory_/L"sun"/L"runtime"/L"sun-prominence.dds",sunProminence_,415);
 D3D12_SHADER_RESOURCE_VIEW_DESC empty{};empty.Format=DXGI_FORMAT_R8G8B8A8_UNORM;empty.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;empty.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;empty.Texture2D.MipLevels=1;
 device_->CreateShaderResourceView(nullptr,&empty,cpu(416));
}
void Renderer::loadPluto(){
 if(plutoColor_)return;
 loadStars(true);
 // Three contiguous SRVs match the shared root table; unused entries are null.
 loadDds(assetDirectory_/L"pluto"/L"runtime"/L"pluto-color.dds",plutoColor_,411);
 D3D12_SHADER_RESOURCE_VIEW_DESC empty{};empty.Format=DXGI_FORMAT_R8G8B8A8_UNORM;empty.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;empty.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;empty.Texture2D.MipLevels=1;
 device_->CreateShaderResourceView(nullptr,&empty,cpu(412));device_->CreateShaderResourceView(nullptr,&empty,cpu(413));
}
void Renderer::loadMercury(){
 if(mercuryColor_&&mercuryNormal_)return;
 loadStars(true);
 loadDds(assetDirectory_/L"mercury"/L"runtime"/L"mercury-color.dds",mercuryColor_,408);
 loadDds(assetDirectory_/L"mercury"/L"runtime"/L"mercury-normal.dds",mercuryNormal_,409);
 D3D12_SHADER_RESOURCE_VIEW_DESC empty{};empty.Format=DXGI_FORMAT_R8G8B8A8_UNORM;empty.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;empty.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;empty.Texture2D.MipLevels=1;
 device_->CreateShaderResourceView(nullptr,&empty,cpu(410));
}
void Renderer::loadMars(){
 if(marsColor_&&marsNormal_)return;
 // 393..395 are outside Smoke transport and tracer descriptor ranges.
 loadStars(true);
 loadDds(assetDirectory_/L"mars"/L"runtime"/L"mars-color.dds",marsColor_,393);
 loadDds(assetDirectory_/L"mars"/L"runtime"/L"mars-normal.dds",marsNormal_,394);
 D3D12_SHADER_RESOURCE_VIEW_DESC empty{};empty.Format=DXGI_FORMAT_R8G8B8A8_UNORM;empty.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;empty.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;empty.Texture2D.MipLevels=1;
 device_->CreateShaderResourceView(nullptr,&empty,cpu(395));
}
void Renderer::loadEarth(){
 if(earthColor_&&earthClouds_&&earthMaterial_)return;
 loadStars(true);
 // Dedicated three-SRV table beyond Mars and all Smoke descriptor ranges.
 loadDds(assetDirectory_/L"earth"/L"runtime"/L"earth-color.dds",earthColor_,396);
 loadDds(assetDirectory_/L"orbit"/L"runtime"/L"earth-clouds-8192.dds",earthClouds_,397);
 loadDds(assetDirectory_/L"earth"/L"runtime"/L"earth-material.dds",earthMaterial_,398);
}
void Renderer::loadPlanet(){
 loadStars();
 loadDds(assetDirectory_/L"orbit"/L"runtime"/L"earth-january.dds",planetTexture_,72);
 loadDds(assetDirectory_/L"orbit"/L"runtime"/L"earth-clouds-8192.dds",cloudTexture_,73);
 loadTexture(assetDirectory_/L"orbit"/L"earth-night-2016.jpg",nightTexture_,74,4096,true);
}
void Renderer::loadAurora(){
 if(auroraArtwork_)return;
 // The current artwork shader reads only t1. Keep the full descriptor table
 // valid without allocating the former procedural 3D-noise resource.
 D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
 srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
 srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
 srv.Texture2D.MipLevels=1;
 device_->CreateShaderResourceView(nullptr,&srv,cpu(69));
 device_->CreateShaderResourceView(nullptr,&srv,cpu(71));
 loadTexture(assetDirectory_/L"aurora"/L"approved-clean-v1.png",auroraArtwork_,70,4096);
}
void Renderer::loadDds(const std::filesystem::path& file,ComPtr<ID3D12Resource>& texture,unsigned slot){
 if(texture)return;
 auto cacheKey=file.lexically_normal().wstring()+L"/dds";if(cachedTexture(cacheKey,texture,slot))return;
 // File-backed source pages do not duplicate the entire DDS in private memory.
 // Keep the mapping alive until every bounded upload has completed.
 struct SourceMapping {
  HANDLE file=INVALID_HANDLE_VALUE,mapping=nullptr;const uint8_t* data=nullptr;
  ~SourceMapping(){if(data)UnmapViewOfFile(data);if(mapping)CloseHandle(mapping);if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);}
 } sourceFile;
 sourceFile.file=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(sourceFile.file==INVALID_HANDLE_VALUE)throw std::runtime_error("Bundled DDS texture missing");
 LARGE_INTEGER length{};if(!GetFileSizeEx(sourceFile.file,&length)||length.QuadPart<148||length.QuadPart>128*1024*1024)throw std::runtime_error("Invalid DDS file size");
 sourceFile.mapping=CreateFileMappingW(sourceFile.file,nullptr,PAGE_READONLY,0,0,nullptr);
 if(!sourceFile.mapping)throw std::runtime_error("Cannot map DDS texture");
 sourceFile.data=static_cast<const uint8_t*>(MapViewOfFile(sourceFile.mapping,FILE_MAP_READ,0,0,0));
 if(!sourceFile.data)throw std::runtime_error("Cannot read DDS texture");
 auto image=parseDds(std::span<const uint8_t>(sourceFile.data,static_cast<size_t>(length.QuadPart)));
 D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=image.width;desc.Height=image.height;desc.DepthOrArraySize=1;desc.MipLevels=static_cast<UINT16>(image.mips.size());desc.Format=image.format;desc.SampleDesc.Count=1;
 ComPtr<ID3D12Resource> ready;auto hp=heap(D3D12_HEAP_TYPE_DEFAULT);check(device_->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&ready)),"Allocate compressed texture");ready->SetName(file.filename().c_str());
 std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(image.mips.size());std::vector<UINT> rows(image.mips.size());UINT64 uploadBytes{};
 device_->GetCopyableFootprints(&desc,0,desc.MipLevels,0,footprints.data(),rows.data(),nullptr,&uploadBytes);
 // Preserve every compressed block and mip. Only staging capacity is reduced.
 uploadBytes=std::min<UINT64>(uploadBytes,4*1024*1024);
 auto uploadHeap=heap(D3D12_HEAP_TYPE_UPLOAD);auto buffer=bufferDesc(uploadBytes);ComPtr<ID3D12Resource> upload;
 check(device_->CreateCommittedResource(&uploadHeap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&upload)),"Allocate bounded compressed upload");
 for(size_t i=0;i<image.mips.size();i++)if(rows[i]!=image.mips[i].rows||image.mips[i].rowBytes>footprints[i].Footprint.RowPitch||footprints[i].Footprint.RowPitch>uploadBytes)throw std::runtime_error("Invalid compressed footprint");
 BYTE* mapped{};D3D12_RANGE none{};check(upload->Map(0,&none,reinterpret_cast<void**>(&mapped)),"Map compressed upload");
 for(UINT i=0;i<desc.MipLevels;i++){
  const auto& footprint=footprints[i].Footprint;
  unsigned capacity=static_cast<unsigned>(uploadBytes/footprint.RowPitch);
  for(unsigned row=0;row<rows[i];){
   unsigned count=std::min(capacity,rows[i]-row);
   for(unsigned chunkRow=0;chunkRow<count;chunkRow++)
    std::memcpy(mapped+size_t(chunkRow)*footprint.RowPitch,sourceFile.data+image.mips[i].offset+size_t(row+chunkRow)*image.mips[i].rowBytes,image.mips[i].rowBytes);
   D3D12_TEXTURE_COPY_LOCATION source{},dest{};source.pResource=upload.Get();source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;source.PlacedFootprint.Footprint=footprint;source.PlacedFootprint.Footprint.Height=std::min(count*4,footprint.Height-row*4);
   dest.pResource=ready.Get();dest.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dest.SubresourceIndex=i;
   begin();list_->CopyTextureRegion(&dest,0,row*4,0,&source,nullptr);submit();idle();
   // The fence must complete before this mapped staging region is overwritten.
   row+=count;
  }
 }
 upload->Unmap(0,nullptr);begin();transition(ready.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);submit();idle();
 D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=desc.Format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=desc.MipLevels;device_->CreateShaderResourceView(ready.Get(),&srv,cpu(slot));
 texture=std::move(ready);retainTexture(cacheKey,texture,slot);
}
void Renderer::loadTexture(const std::filesystem::path& file,ComPtr<ID3D12Resource>& texture,unsigned slot,unsigned maxWidth,bool grayscale){
 if(texture)return;
 auto cacheKey=file.lexically_normal().wstring()+L"/wic/"+std::to_wstring(maxWidth)+L"/"+std::to_wstring(grayscale);if(cachedTexture(cacheKey,texture,slot))return;
 ComPtr<IWICImagingFactory> wic;check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&wic)),"Create texture decoder");
 ComPtr<IWICBitmapDecoder> decoder;check(wic->CreateDecoderFromFilename(file.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder),"Load artwork texture");
 ComPtr<IWICBitmapFrameDecode> frame;check(decoder->GetFrame(0,&frame),"Read artwork texture");
 UINT originalW{},originalH{};check(frame->GetSize(&originalW,&originalH),"Read texture dimensions");UINT width=std::min(originalW,maxWidth),height=std::max(1u,UINT(uint64_t(originalH)*width/originalW));
 ComPtr<IWICBitmapScaler> scaler;check(wic->CreateBitmapScaler(&scaler),"Create texture scaler");check(scaler->Initialize(frame.Get(),width,height,WICBitmapInterpolationModeFant),"Scale artwork texture");
 ComPtr<IWICFormatConverter> converter;check(wic->CreateFormatConverter(&converter),"Create texture converter");check(converter->Initialize(scaler.Get(),grayscale?GUID_WICPixelFormat8bppGray:GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom),"Convert artwork texture");
 // Mipmaps preserve fine coverage at the limb without sparkling or a blur pass.
 // RGB filtering is in linear light; coverage and alpha remain linear data.
 const UINT channels=grayscale?1:4;
 std::vector<std::vector<BYTE>> levels(1);
 levels[0].resize(size_t(width)*height*channels);
 check(converter->CopyPixels(nullptr,width*channels,static_cast<UINT>(levels[0].size()),levels[0].data()),"Decode artwork pixels");
 std::array<float,256> linear{};
 for(unsigned i=0;i<256;i++){float c=i/255.f;linear[i]=c<=.04045f?c/12.92f:std::pow((c+.055f)/1.055f,2.4f);}
 UINT mipWidth=width,mipHeight=height;
 while(mipWidth>1||mipHeight>1){
  UINT nextWidth=std::max(1u,mipWidth/2),nextHeight=std::max(1u,mipHeight/2);
  std::vector<BYTE> next(size_t(nextWidth)*nextHeight*channels);
  const auto& previous=levels.back();
  for(UINT y=0;y<nextHeight;y++)for(UINT x=0;x<nextWidth;x++)for(UINT c=0;c<channels;c++){
   // Include the final row/column for odd-sized maps such as the night mask.
   UINT x0=x*mipWidth/nextWidth,x1=(x+1)*mipWidth/nextWidth;
   UINT y0=y*mipHeight/nextHeight,y1=(y+1)*mipHeight/nextHeight;
   float value=0;
   for(UINT sy=y0;sy<y1;sy++)for(UINT sx=x0;sx<x1;sx++){
    BYTE b=previous[(size_t(sy)*mipWidth+sx)*channels+c];value+=!grayscale&&c<3?linear[b]:b/255.f;
   }
   value/=float((x1-x0)*(y1-y0));
   if(!grayscale&&c<3)value=value<=.0031308f?value*12.92f:1.055f*std::pow(value,1/2.4f)-.055f;
   next[(size_t(y)*nextWidth+x)*channels+c]=static_cast<BYTE>(std::clamp(value*255.f+.5f,0.f,255.f));
  }
  levels.push_back(std::move(next));mipWidth=nextWidth;mipHeight=nextHeight;
 }
 D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=width;desc.Height=height;desc.DepthOrArraySize=1;desc.MipLevels=static_cast<UINT16>(levels.size());desc.Format=grayscale?DXGI_FORMAT_R8_UNORM:DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;desc.SampleDesc.Count=1;
 ComPtr<ID3D12Resource> ready;auto hp=heap(D3D12_HEAP_TYPE_DEFAULT);check(device_->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&ready)),"Allocate artwork texture");ready->SetName(file.filename().c_str());
 std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(levels.size());UINT64 bytes{};device_->GetCopyableFootprints(&desc,0,desc.MipLevels,0,footprints.data(),nullptr,nullptr,&bytes);auto uploadHeap=heap(D3D12_HEAP_TYPE_UPLOAD);auto buffer=bufferDesc(bytes);ComPtr<ID3D12Resource> upload;check(device_->CreateCommittedResource(&uploadHeap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&upload)),"Allocate artwork upload");
 BYTE* mapped{};D3D12_RANGE none{};check(upload->Map(0,&none,reinterpret_cast<void**>(&mapped)),"Map artwork upload");
 for(size_t i=0;i<levels.size();i++){
  const auto& fp=footprints[i];size_t rowBytes=size_t(fp.Footprint.Width)*channels;
  for(UINT y=0;y<fp.Footprint.Height;y++)std::memcpy(mapped+fp.Offset+size_t(y)*fp.Footprint.RowPitch,levels[i].data()+size_t(y)*rowBytes,rowBytes);
 }
 upload->Unmap(0,nullptr);
 begin();D3D12_TEXTURE_COPY_LOCATION source{},dest{};source.pResource=upload.Get();source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dest.pResource=ready.Get();dest.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
 for(UINT i=0;i<desc.MipLevels;i++){source.PlacedFootprint=footprints[i];dest.SubresourceIndex=i;list_->CopyTextureRegion(&dest,0,0,0,&source,nullptr);}
 transition(ready.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);submit();idle();
 D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=desc.Format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=desc.MipLevels;device_->CreateShaderResourceView(ready.Get(),&srv,cpu(slot));
 texture=std::move(ready);retainTexture(cacheKey,texture,slot);
}
D3D12_CPU_DESCRIPTOR_HANDLE Renderer::cpu(unsigned index)const {auto h=views_->GetCPUDescriptorHandleForHeapStart();h.ptr+=size_t(index)*descriptorStride_;return h;}
D3D12_GPU_DESCRIPTOR_HANDLE Renderer::gpu(unsigned index)const {auto h=views_->GetGPUDescriptorHandleForHeapStart();h.ptr+=uint64_t(index)*descriptorStride_;return h;}
void Renderer::setGrid(unsigned value){value=std::clamp(value,24u,wideDensity_?96u:192u);value=(value/4)*4;if(pressureMultigrid_&&(value>128||(value&(value-1))))throw std::runtime_error("Unsupported multigrid grid change");if(grid_==value)return;idle();releasePressureHierarchy();releaseFineAtlas();grid_=value;fluidReady_=false;detailVelocity_.resource.Reset();releaseFlowmapAudit();flowmapBase_.resource.Reset();flowmapTotal_.resource.Reset();for(auto& r:coordinates_)r.resource.Reset();for(auto& r:velocity_)r.resource.Reset();for(auto& r:density_)r.resource.Reset();for(auto& r:pressure_)r.resource.Reset();divergence_.resource.Reset();curl_.resource.Reset();}
void Renderer::releaseFlowmapAudit(){flowmapAudit_.Reset();flowmapAuditZero_.Reset();for(auto& r:flowmapAuditReadback_)r.Reset();flowmapAuditReady_={};}
void Renderer::makeFluid(){
 if(pressureMultigrid_&&!fluidMultigrid_){auto code=shader(assetDirectory_.parent_path()/L"shaders",L"fluid_multigrid.cso");D3D12_COMPUTE_PIPELINE_STATE_DESC d{};d.pRootSignature=root_.Get();d.CS={code.data(),code.size()};check(device_->CreateComputePipelineState(&d,IID_PPV_ARGS(&fluidMultigrid_)),"Create capture fluid pipeline");}

 idle();auto h=heap(D3D12_HEAP_TYPE_DEFAULT);fluidBytes_=0;
 auto make=[&](Volume& v,DXGI_FORMAT fmt,std::array<unsigned,3> extent){D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE3D;d.Width=extent[0];d.Height=extent[1];d.DepthOrArraySize=(UINT16)extent[2];d.MipLevels=1;d.Format=fmt;d.SampleDesc.Count=1;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;check(device_->CreateCommittedResource(&h,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&v.resource)),"Allocate fluid volume");fluidBytes_+=device_->GetResourceAllocationInfo(0,1,&d).SizeInBytes;};
 for(auto& r:velocity_)make(r,DXGI_FORMAT_R16G16B16A16_FLOAT,{grid_,grid_,grid_});for(auto& r:density_)make(r,DXGI_FORMAT_R16G16B16A16_FLOAT,densityDimensions());for(auto& r:pressure_)make(r,DXGI_FORMAT_R32_FLOAT,{grid_,grid_,grid_});make(divergence_,DXGI_FORMAT_R32_FLOAT,{grid_,grid_,grid_});make(curl_,DXGI_FORMAT_R16G16B16A16_FLOAT,densityDimensions());
 if(detailCoordinates_){
  make(detailVelocity_,DXGI_FORMAT_R32G32B32A32_FLOAT,densityDimensions());
  for(auto& r:coordinates_)make(r,DXGI_FORMAT_R32G32B32A32_FLOAT,{grid_*2,grid_,grid_});
  for(unsigned v=0;v<2;v++)for(unsigned c=0;c<2;c++){
   unsigned start=243+(v*2+c)*11;
   D3D12_UNORDERED_ACCESS_VIEW_DESC u{};u.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;u.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE3D;u.Texture3D.WSize=grid_;
   for(unsigned index=0;index<8;index++)device_->CreateUnorderedAccessView(index==1?coordinates_[1-c].resource.Get():nullptr,nullptr,&u,cpu(start+index));
   D3D12_SHADER_RESOURCE_VIEW_DESC s{};s.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE3D;s.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;s.Texture3D.MipLevels=1;s.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
   device_->CreateShaderResourceView(velocity_[v].resource.Get(),&s,cpu(start+8));
   s.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;device_->CreateShaderResourceView(coordinates_[c].resource.Get(),&s,cpu(start+9));device_->CreateShaderResourceView(nullptr,&s,cpu(start+10));
   device_->CreateShaderResourceView(coordinates_[c].resource.Get(),&s,cpu(307+(v*2+c)*3+1));
   device_->CreateShaderResourceView(detailVelocity_.resource.Get(),&s,cpu(307+(v*2+c)*3+2));
   s.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;device_->CreateShaderResourceView(velocity_[v].resource.Get(),&s,cpu(307+(v*2+c)*3));
  }
 }
 D3D12_SHADER_RESOURCE_VIEW_DESC coordinateView{};coordinateView.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;coordinateView.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE3D;coordinateView.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;coordinateView.Texture3D.MipLevels=1;

 device_->CreateShaderResourceView(nullptr,&coordinateView,cpu(289));
 D3D12_UNORDERED_ACCESS_VIEW_DESC detailUav{};detailUav.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;detailUav.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE3D;detailUav.Texture3D.WSize=densityDimensions()[2];
 for(unsigned i=0;i<8;i++)device_->CreateUnorderedAccessView(detailCoordinates_&&i==1?detailVelocity_.resource.Get():nullptr,nullptr,&detailUav,cpu(290+i));
 device_->CreateShaderResourceView(detailCoordinates_?detailVelocity_.resource.Get():nullptr,&coordinateView,cpu(298));
 detailUav.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
 for(unsigned i=0;i<8;i++)device_->CreateUnorderedAccessView(detailCoordinates_&&i==7?curl_.resource.Get():nullptr,nullptr,&detailUav,cpu(299+i));
 coordinateView.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;device_->CreateShaderResourceView(curl_.resource.Get(),&coordinateView,cpu(287));
 coordinateIndex_=0;
 for(unsigned v=0;v<2;v++)for(unsigned d=0;d<2;d++)for(unsigned p=0;p<2;p++){
  ID3D12Resource* resources[]={velocity_[v].resource.Get(),velocity_[1-v].resource.Get(),density_[d].resource.Get(),density_[1-d].resource.Get(),pressure_[p].resource.Get(),pressure_[1-p].resource.Get(),divergence_.resource.Get(),curl_.resource.Get()};
  unsigned start=(v*4+d*2+p)*8;for(unsigned i=0;i<8;i++){D3D12_UNORDERED_ACCESS_VIEW_DESC u{};u.Format=resources[i]->GetDesc().Format;u.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE3D;u.Texture3D.WSize=resources[i]->GetDesc().DepthOrArraySize;device_->CreateUnorderedAccessView(resources[i],nullptr,&u,cpu(start+i));}
 }
 for(unsigned i=0;i<2;i++){D3D12_SHADER_RESOURCE_VIEW_DESC s{};s.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;s.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE3D;s.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;s.Texture3D.MipLevels=1;device_->CreateShaderResourceView(density_[i].resource.Get(),&s,cpu(64+i));device_->CreateShaderResourceView(density_[i].resource.Get(),&s,cpu(76+i*3));device_->CreateShaderResourceView(curl_.resource.Get(),&s,cpu(77+i*3));device_->CreateShaderResourceView(nullptr,&s,cpu(78+i*3));}
 // Read-only fluid inputs have SRVs and null UAV aliases during sampling.
 for(unsigned v=0;v<2;v++)for(unsigned d=0;d<2;d++)for(unsigned p=0;p<2;p++){
  ID3D12Resource* resources[]={velocity_[v].resource.Get(),velocity_[1-v].resource.Get(),density_[d].resource.Get(),density_[1-d].resource.Get(),pressure_[p].resource.Get(),pressure_[1-p].resource.Get(),divergence_.resource.Get(),curl_.resource.Get()};
  unsigned combination=v*4+d*2+p;
  for(unsigned forward=0;forward<2;forward++)for(unsigned index=0;index<8;index++){
   D3D12_UNORDERED_ACCESS_VIEW_DESC u{};u.Format=resources[index]->GetDesc().Format;u.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE3D;u.Texture3D.WSize=resources[index]->GetDesc().DepthOrArraySize;
   bool readOnly=index==0||index==2||(forward&&index==3);
   device_->CreateUnorderedAccessView(readOnly?nullptr:resources[index],nullptr,&u,cpu(85+forward*64+combination*8+index));
  }
 }
 D3D12_SHADER_RESOURCE_VIEW_DESC fluidView{};fluidView.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;fluidView.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE3D;fluidView.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;fluidView.Texture3D.MipLevels=1;
 for(unsigned v=0;v<2;v++)for(unsigned d=0;d<2;d++)for(unsigned forward=0;forward<2;forward++){
  unsigned start=213+(v*4+d*2+forward)*3;
  device_->CreateShaderResourceView(velocity_[v].resource.Get(),&fluidView,cpu(start));
  device_->CreateShaderResourceView(density_[d].resource.Get(),&fluidView,cpu(start+1));
  device_->CreateShaderResourceView(forward?density_[1-d].resource.Get():nullptr,&fluidView,cpu(start+2));
 }
 for(unsigned index=237;index<240;index++)device_->CreateShaderResourceView(nullptr,&fluidView,cpu(index));
 if(flowmapAdaptive_){
  releaseFlowmapAudit();
  auto makeBuffer=[&](ComPtr<ID3D12Resource>& resource,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state,bool uav){
   auto properties=heap(type);auto desc=bufferDesc(16);if(uav)desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
   check(device_->CreateCommittedResource(&properties,D3D12_HEAP_FLAG_NONE,&desc,state,nullptr,IID_PPV_ARGS(&resource)),"Allocate flow-map audit");fluidBytes_+=device_->GetResourceAllocationInfo(0,1,&desc).SizeInBytes;
  };
  makeBuffer(flowmapAudit_,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST,true);
  makeBuffer(flowmapAuditZero_,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,false);
  for(auto& r:flowmapAuditReadback_)makeBuffer(r,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST,false);
  void* zero{};D3D12_RANGE none{};check(flowmapAuditZero_->Map(0,&none,&zero),"Map audit zero");memset(zero,0,16);flowmapAuditZero_->Unmap(0,nullptr);
  D3D12_UNORDERED_ACCESS_VIEW_DESC audit{};audit.Format=DXGI_FORMAT_R32_TYPELESS;audit.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;audit.Buffer.NumElements=4;audit.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
  device_->CreateUnorderedAccessView(flowmapAudit_.Get(),nullptr,&audit,cpu(344));
  D3D12_UNORDERED_ACCESS_VIEW_DESC texture{};texture.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;texture.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE3D;texture.Texture3D.WSize=densityDimensions()[2];
  for(unsigned i=1;i<8;i++)device_->CreateUnorderedAccessView(i==7?curl_.resource.Get():nullptr,nullptr,&texture,cpu(344+i));
 }
 if(flowmapLive_){
  make(flowmapBase_,DXGI_FORMAT_R16G16B16A16_FLOAT,densityDimensions());make(flowmapTotal_,DXGI_FORMAT_R16G16B16A16_FLOAT,densityDimensions());
  device_->CreateShaderResourceView(flowmapBase_.resource.Get(),&fluidView,cpu(319));device_->CreateShaderResourceView(flowmapTotal_.resource.Get(),&fluidView,cpu(320));
  D3D12_UNORDERED_ACCESS_VIEW_DESC u{};u.Format=fluidView.Format;u.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE3D;u.Texture3D.WSize=densityDimensions()[2];
  device_->CreateUnorderedAccessView(flowmapBase_.resource.Get(),nullptr,&u,cpu(321));device_->CreateUnorderedAccessView(flowmapTotal_.resource.Get(),nullptr,&u,cpu(322));
  for(unsigned c=0;c<2;c++)for(unsigned d=0;d<2;d++){
   unsigned slot=323+(c*2+d)*3;
   device_->CreateShaderResourceView(density_[d].resource.Get(),&fluidView,cpu(slot));
   auto coordView=fluidView;coordView.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;device_->CreateShaderResourceView(coordinates_[c].resource.Get(),&coordView,cpu(slot+1));device_->CreateShaderResourceView(nullptr,&fluidView,cpu(slot+2));
  }
  device_->CreateShaderResourceView(flowmapTotal_.resource.Get(),&fluidView,cpu(335));device_->CreateShaderResourceView(curl_.resource.Get(),&fluidView,cpu(336));device_->CreateShaderResourceView(nullptr,&fluidView,cpu(337));
  for(unsigned v=0;v<2;v++){
   device_->CreateShaderResourceView(velocity_[v].resource.Get(),&fluidView,cpu(338+v*3));device_->CreateShaderResourceView(flowmapTotal_.resource.Get(),&fluidView,cpu(339+v*3));device_->CreateShaderResourceView(nullptr,&fluidView,cpu(340+v*3));
  }
 }
 v_=d_=p_=0;fluidReady_=false;allocationBytes_=uint64_t(width_)*height_*20+uint64_t(grid_)*grid_*grid_*52;
 if(!referenceVolume_.empty()){
  auto extent=densityDimensions();size_t expected=size_t(extent[0])*extent[1]*extent[2]*8;
  std::ifstream input(referenceVolume_,std::ios::binary|std::ios::ate);
  if(!input||input.tellg()!=std::streamoff(expected))throw std::runtime_error("Reference volume must exactly match the selected RGBA16F density dimensions");
  std::vector<uint16_t> raw(expected/2);input.seekg(0);input.read(reinterpret_cast<char*>(raw.data()),std::streamsize(expected));
  if(!input)throw std::runtime_error("Cannot read reference volume");
  for(auto value:raw)if((value&0x8000)||(value&0x7c00)==0x7c00)throw std::runtime_error("Reference concentrations must be finite and nonnegative");
  auto desc=density_[0].resource->GetDesc();UINT64 total{};device_->GetCopyableFootprints(&desc,0,1,0,&referenceFootprint_,nullptr,nullptr,&total);
  auto hp=heap(D3D12_HEAP_TYPE_UPLOAD);auto bd=bufferDesc(total);referenceUpload_.Reset();
  check(device_->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&referenceUpload_)),"Create reference volume upload");
  BYTE* mapped{};D3D12_RANGE none{};check(referenceUpload_->Map(0,&none,reinterpret_cast<void**>(&mapped)),"Map reference volume");
  for(unsigned z=0;z<extent[2];z++)for(unsigned y=0;y<extent[1];y++)memcpy(mapped+referenceFootprint_.Offset+(size_t(z)*extent[1]+y)*referenceFootprint_.Footprint.RowPitch,reinterpret_cast<const BYTE*>(raw.data())+(size_t(z)*extent[1]+y)*extent[0]*8,size_t(extent[0])*8);
  referenceUpload_->Unmap(0,nullptr);
  fluidBytes_+=device_->GetResourceAllocationInfo(0,1,&bd).SizeInBytes;
 }
}
void Renderer::transition(ID3D12Resource* resource,D3D12_RESOURCE_STATES from,D3D12_RESOURCE_STATES to){D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={resource,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,from,to};list_->ResourceBarrier(1,&b);}
void Renderer::begin(){
 frameIndex_=swap_->GetCurrentBackBufferIndex();auto& f=frames_[frameIndex_];wait(f.fence);
 if(f.fence){
  uint64_t* times{};size_t base=frameIndex_*QueryStride;
  D3D12_RANGE range{base*sizeof(uint64_t),(base+QueryStride)*sizeof(uint64_t)};
  gpuStageMs_.fill(0);
  if(SUCCEEDED(queryReadback_->Map(0,&range,reinterpret_cast<void**>(&times)))){
   auto a=times[base],b=times[base+1];if(b>=a&&frequency_)gpuMs_=double(b-a)*1000/double(frequency_);
   for(unsigned slot=0;slot<stageCount_[frameIndex_];slot++){
    auto start=times[base+2+slot*2],end=times[base+3+slot*2];
    if(end>=start&&frequency_)gpuStageMs_[stageIds_[frameIndex_][slot]]+=double(end-start)*1000/double(frequency_);
   }
   D3D12_RANGE none{};queryReadback_->Unmap(0,&none);
  }
 }
 stageCount_[frameIndex_]=0;
 check(f.allocator->Reset(),"Reset frame allocator");check(list_->Reset(f.allocator.Get(),nullptr),"Reset graphics list");
 list_->EndQuery(query_.Get(),D3D12_QUERY_TYPE_TIMESTAMP,frameIndex_*QueryStride);
}
unsigned Renderer::beginGpuStage(unsigned stage){
 if(!stageProfiling_)return UINT_MAX;
 auto slot=stageCount_[frameIndex_]++;
 if(slot>=32||stage>=gpuStageMs_.size())throw std::runtime_error("GPU stage query capacity exceeded");
 stageIds_[frameIndex_][slot]=stage;
 list_->EndQuery(query_.Get(),D3D12_QUERY_TYPE_TIMESTAMP,frameIndex_*QueryStride+2+slot*2);return slot;
}
void Renderer::endGpuStage(unsigned slot){if(slot!=UINT_MAX)list_->EndQuery(query_.Get(),D3D12_QUERY_TYPE_TIMESTAMP,frameIndex_*QueryStride+3+slot*2);}
void Renderer::submit(){
 list_->EndQuery(query_.Get(),D3D12_QUERY_TYPE_TIMESTAMP,frameIndex_*QueryStride+1);
 list_->ResolveQueryData(query_.Get(),D3D12_QUERY_TYPE_TIMESTAMP,frameIndex_*QueryStride,2+stageCount_[frameIndex_]*2,queryReadback_.Get(),frameIndex_*QueryStride*sizeof(uint64_t));
 check(list_->Close(),"Close graphics list");ID3D12CommandList* lists[]={list_.Get()};queue_->ExecuteCommandLists(1,lists);check(queue_->Signal(fence_.Get(),++fenceValue_),"Submit graphics fence");frames_[frameIndex_].fence=fenceValue_;
}
void Renderer::advectCoordinates(FrameData& data,bool initialize){
 if(!detailCoordinates_)return;
 transition(velocity_[v_].resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 transition(coordinates_[coordinateIndex_].resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 data.pass=initialize?0.f:1.f;list_->SetPipelineState(coordinatePipeline_.Get());list_->SetComputeRoot32BitConstants(0,32,&data,0);
 unsigned start=243+(v_*2+coordinateIndex_)*11;list_->SetComputeRootDescriptorTable(1,gpu(start));list_->SetComputeRootDescriptorTable(2,gpu(start+8));
 if(!initialize&&data.spare[9]>1.5f){
  // Reuse the unused detail-velocity texture as a forward-map scratch field.
  // Its extent contains the packed pair of grid-sized coordinate charts.
  data.pass=2;list_->SetComputeRoot32BitConstants(0,32,&data,0);list_->SetComputeRootDescriptorTable(1,gpu(290));
  list_->Dispatch(grid_/2,grid_/4,grid_/4);
  transition(detailVelocity_.resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  data.pass=3;list_->SetComputeRoot32BitConstants(0,32,&data,0);list_->SetComputeRootDescriptorTable(1,gpu(start));list_->SetComputeRootDescriptorTable(2,gpu(307+(v_*2+coordinateIndex_)*3));
  list_->Dispatch(grid_/2,grid_/4,grid_/4);
  transition(detailVelocity_.resource.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 }else list_->Dispatch(grid_/2,grid_/4,grid_/4);
 D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;list_->ResourceBarrier(1,&barrier);
 transition(velocity_[v_].resource.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 transition(coordinates_[coordinateIndex_].resource.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 coordinateIndex_=1-coordinateIndex_;list_->SetPipelineState(pressureMultigrid_?fluidMultigrid_.Get():fluid_.Get());
}
void Renderer::buildDetailField(FrameData& data){
 if(!detailCoordinates_)return;
 transition(velocity_[v_].resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 transition(coordinates_[coordinateIndex_].resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 data.pass=0;list_->SetPipelineState(detailPipeline_.Get());list_->SetComputeRoot32BitConstants(0,32,&data,0);
 list_->SetComputeRootDescriptorTable(3,gpu(289));list_->SetComputeRootDescriptorTable(1,gpu(299));list_->SetComputeRootDescriptorTable(2,gpu(243+(v_*2+coordinateIndex_)*11+8));
 auto size=densityDimensions();list_->Dispatch(size[0]/4,size[1]/4,size[2]/4);
 D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;list_->ResourceBarrier(1,&barrier);
 transition(curl_.resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 data.pass=1;list_->SetComputeRoot32BitConstants(0,32,&data,0);list_->SetComputeRootDescriptorTable(1,gpu(290));list_->SetComputeRootDescriptorTable(3,gpu(287));
 list_->Dispatch(size[0]/4,size[1]/4,size[2]/4);list_->ResourceBarrier(1,&barrier);
 transition(curl_.resource.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 list_->SetComputeRootDescriptorTable(3,gpu(289));
 transition(velocity_[v_].resource.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 transition(coordinates_[coordinateIndex_].resource.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 list_->SetPipelineState(pressureMultigrid_?fluidMultigrid_.Get():fluid_.Get());
}
void Renderer::combineFlowmap(FrameData& data){
 data.spare[1]=flowmapAge_;data.pass=flowmapAdaptive_?5.f:4.f;
 if(flowmapAdaptive_){list_->CopyBufferRegion(flowmapAudit_.Get(),0,flowmapAuditZero_.Get(),0,16);transition(flowmapAudit_.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);}
 transition(coordinates_[coordinateIndex_].resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 transition(density_[d_].resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 transition(flowmapBase_.resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 list_->SetPipelineState(detailPipeline_.Get());list_->SetComputeRoot32BitConstants(0,32,&data,0);
 list_->SetComputeRootDescriptorTable(1,gpu(flowmapAdaptive_?344:299));list_->SetComputeRootDescriptorTable(2,gpu(323+(coordinateIndex_*2+d_)*3));list_->SetComputeRootDescriptorTable(3,gpu(319));
 auto size=densityDimensions();list_->Dispatch(size[0]/4,size[1]/4,size[2]/4);
 if(flowmapAdaptive_){
  transition(flowmapAudit_.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
  list_->CopyBufferRegion(flowmapAuditReadback_[frameIndex_].Get(),0,flowmapAudit_.Get(),0,16);
  transition(flowmapAudit_.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COPY_DEST);
  flowmapAuditReady_[frameIndex_]=true;flowmapAuditEpoch_[frameIndex_]=flowmapRenewals_;
 }
 transition(coordinates_[coordinateIndex_].resource.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 transition(density_[d_].resource.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 transition(flowmapBase_.resource.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 transition(curl_.resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
 transition(flowmapTotal_.resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_DEST);
 list_->CopyResource(flowmapTotal_.resource.Get(),curl_.resource.Get());
 transition(curl_.resource.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 transition(flowmapTotal_.resource.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 list_->SetPipelineState(pressureMultigrid_?fluidMultigrid_.Get():fluid_.Get());list_->SetComputeRootDescriptorTable(3,gpu(289));
}
void Renderer::simulate(FrameData& data){
 data.grid=float(grid_);data.spare[2]=float(densityGridSize());for(unsigned axis=0;axis<3;axis++)data.spare[5+axis]=float(densityDimensions()[axis]);list_->SetComputeRootSignature(root_.Get());list_->SetPipelineState(pressureMultigrid_?fluidMultigrid_.Get():fluid_.Get());
 auto dispatch=[&](float pass){
  unsigned timing=beginGpuStage(unsigned(std::max(pass,0.f)));
  bool clear=pass<=0,forward=pass==7;
  if(!clear){
   transition(velocity_[v_].resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   transition(density_[d_].resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   if(forward)transition(density_[1-d_].resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  }
  bool total=flowmapLive_&&(pass==1||pass==8);
  if(total)transition(flowmapTotal_.resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  data.pass=pass;
  FrameData computeData=data;
  // Pass 8 may skip lighting empty coarse bricks unless a fine-density atlas
  // still needs their lighting. Keep this compute-only flag out of scene data.
  if(pass==8)computeData.spare[11]=(fineAtlasEnabled_||data.spare[10]>.5f)?1.f:0.f;
  list_->SetComputeRoot32BitConstants(0,32,&computeData,0);
  list_->SetComputeRootDescriptorTable(1,gpu((clear?0:85+unsigned(forward)*64)+(v_*4+d_*2+p_)*8));
  list_->SetComputeRootDescriptorTable(2,gpu(total?338+v_*3:clear?237:213+(v_*4+d_*2+unsigned(forward))*3));
  auto extent=(pass<=0||pass>=7)?densityDimensions():std::array<unsigned,3>{grid_,grid_,grid_};list_->Dispatch(extent[0]/4,extent[1]/4,extent[2]/4);
  D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;list_->ResourceBarrier(1,&barrier);
  if(!clear){
   transition(velocity_[v_].resource.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   transition(density_[d_].resource.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   if(forward)transition(density_[1-d_].resource.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  }
  if(total)transition(flowmapTotal_.resource.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  endGpuStage(timing);
 };
 data.spare[8]=detailCoordinates_&&!flowmapLive_?1.f:0.f;
 list_->SetComputeRootDescriptorTable(3,gpu(289));
 bool initializeCoordinates=!fluidReady_;
 if(!fluidReady_){
  dispatch(0);fluidReady_=true;
  if(referenceUpload_){
   transition(density_[d_].resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_DEST);
   D3D12_TEXTURE_COPY_LOCATION src{},dst{};src.pResource=referenceUpload_.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=referenceFootprint_;
   dst.pResource=density_[d_].resource.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
   list_->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
   transition(density_[d_].resource.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  }
 }
 if(flowmapLive_){
  bool distorted=false;
  // begin() has already waited for this frame slot's fence; no extra GPU wait.
  if(flowmapAdaptive_&&flowmapAuditReady_[frameIndex_]&&flowmapAuditEpoch_[frameIndex_]==flowmapRenewals_){
   uint32_t* counts{};D3D12_RANGE range{0,8};check(flowmapAuditReadback_[frameIndex_]->Map(0,&range,reinterpret_cast<void**>(&counts)),"Read flow-map distortion");
   distorted=counts[1]>8&&uint64_t(counts[1])*100>counts[0];D3D12_RANGE none{};flowmapAuditReadback_[frameIndex_]->Unmap(0,&none);
  }
  auto copyVolume=[&](ID3D12Resource* from,ID3D12Resource* to){
   transition(from,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);transition(to,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_DEST);list_->CopyResource(to,from);transition(from,D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);transition(to,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  };
  if(initializeCoordinates){
   copyVolume(density_[0].resource.Get(),flowmapBase_.resource.Get());copyVolume(density_[0].resource.Get(),flowmapTotal_.resource.Get());flowmapAge_=0;flowmapRenewals_=0;flowmapAdaptiveRenewals_=0;
  }else if(flowmapAge_>=flowmapRenewal_-data.dt*.25f||(distorted&&flowmapAge_>=.05f)){
   transition(flowmapTotal_.resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);transition(flowmapBase_.resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_DEST);
   list_->CopyResource(flowmapBase_.resource.Get(),flowmapTotal_.resource.Get());
   transition(flowmapTotal_.resource.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);transition(flowmapBase_.resource.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   dispatch(-1);
   initializeCoordinates=true;flowmapAge_=0;++flowmapRenewals_;if(distorted)++flowmapAdaptiveRenewals_;
  }
  D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;list_->ResourceBarrier(1,&barrier);
  if(initializeCoordinates)advectCoordinates(data,true);
  advectCoordinates(data,false);flowmapAge_+=data.dt;
 }
 if(referenceUpload_&&data.spare[9]>1.5f){
  // Density[0] is immutable between renewals. Bake the previous output before
  // resetting the map, then advance normally; do not replay elapsed time.
  if(initializeCoordinates){flowmapAge_=0;flowmapRenewals_=0;}
  if(!initializeCoordinates&&flowmapRenewal_>0&&flowmapAge_>=flowmapRenewal_-data.dt*.25f){
   transition(density_[1].resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
   transition(density_[0].resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_DEST);
   list_->CopyResource(density_[0].resource.Get(),density_[1].resource.Get());
   transition(density_[1].resource.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   transition(density_[0].resource.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   initializeCoordinates=true;flowmapAge_=0;++flowmapRenewals_;
  }
  if(initializeCoordinates)advectCoordinates(data,true);
  advectCoordinates(data,false);
  flowmapAge_+=data.dt;
  transition(coordinates_[coordinateIndex_].resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  transition(density_[0].resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  list_->SetPipelineState(detailPipeline_.Get());list_->SetComputeRoot32BitConstants(0,32,&data,0);
  list_->SetComputeRootDescriptorTable(1,gpu(299));list_->SetComputeRootDescriptorTable(2,gpu(243+(v_*2+coordinateIndex_)*11+8));list_->SetComputeRootDescriptorTable(3,gpu(64));
  auto size=densityDimensions();list_->Dispatch(size[0]/4,size[1]/4,size[2]/4);
  transition(coordinates_[coordinateIndex_].resource.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  transition(density_[0].resource.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  transition(curl_.resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
  transition(density_[1].resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_DEST);
  list_->CopyResource(density_[1].resource.Get(),curl_.resource.Get());
  transition(curl_.resource.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  transition(density_[1].resource.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  d_=1;list_->SetPipelineState(pressureMultigrid_?fluidMultigrid_.Get():fluid_.Get());list_->SetComputeRootDescriptorTable(3,gpu(289));dispatch(8);return;
 }
 if(referenceUpload_&&data.spare[4]<.5f){dispatch(8);return;}
 unsigned auxiliaryTiming=beginGpuStage(10);
 if(!flowmapLive_){advectCoordinates(data,initializeCoordinates);buildDetailField(data);}
 endGpuStage(auxiliaryTiming);
 if(detailCoordinates_&&!flowmapLive_)transition(detailVelocity_.resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 list_->SetComputeRootDescriptorTable(3,gpu(detailCoordinates_&&!flowmapLive_?298:289));
 auto finishCoordinates=[&]{if(detailCoordinates_&&!flowmapLive_)transition(detailVelocity_.resource.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);};
 if(data.spare[4]<.5f)dispatch(1);dispatch(9);dispatch(7);
 unsigned copyTiming=beginGpuStage(12);
 transition(curl_.resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
 transition(density_[1-d_].resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_DEST);
 list_->CopyResource(density_[1-d_].resource.Get(),curl_.resource.Get());
 transition(curl_.resource.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 transition(density_[1-d_].resource.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 endGpuStage(copyTiming);
 d_=1-d_;
 if(flowmapLive_)combineFlowmap(data);
 if(data.spare[4]>.5f){dispatch(8);finishCoordinates();return;}
 v_=1-v_;dispatch(2);dispatch(3);v_=1-v_;dispatch(4);
 if(pressureMultigrid_)solvePressureHierarchy();else for(int i=0;i<12;i++){dispatch(5);p_=1-p_;}dispatch(6);v_=1-v_;dispatch(8);finishCoordinates();
}
void Renderer::makeSmokeFilaments(){
 if(smokeFilamentPipeline_)return;
 auto resourceDesc=bufferDesc((4096*2+128*32)*16);resourceDesc.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;auto hp=heap(D3D12_HEAP_TYPE_DEFAULT);
 check(device_->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&resourceDesc,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&smokeTracers_)),"Create persistent Smoke tracers");
 smokeTracerBytes_=device_->GetResourceAllocationInfo(0,1,&resourceDesc).SizeInBytes;
 D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;uav.Buffer.NumElements=12288;uav.Buffer.StructureByteStride=16;device_->CreateUnorderedAccessView(smokeTracers_.Get(),nullptr,&uav,cpu(391));
 D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;srv.Buffer.NumElements=12288;srv.Buffer.StructureByteStride=16;device_->CreateShaderResourceView(smokeTracers_.Get(),&srv,cpu(392));
 auto dir=assetDirectory_.parent_path()/L"shaders";
 auto compute=shader(dir,L"smoke_tracers.cso");D3D12_COMPUTE_PIPELINE_STATE_DESC cd{};cd.pRootSignature=root_.Get();cd.CS={compute.data(),compute.size()};check(device_->CreateComputePipelineState(&cd,IID_PPV_ARGS(&smokeTracerPipeline_)),"Create tracer advection pipeline");
 auto vs=shader(dir,L"smoke_filament_vs.cso"),ps=shader(dir,L"smoke_filament_ps.cso");
 D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root_.Get();
 pd.VS={vs.data(),vs.size()};pd.PS={ps.data(),ps.size()};pd.SampleMask=UINT_MAX;
 pd.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
 pd.NumRenderTargets=1;pd.RTVFormats[0]=DXGI_FORMAT_R16G16B16A16_FLOAT;
 pd.DSVFormat=DXGI_FORMAT_D32_FLOAT;pd.SampleDesc.Count=1;
 pd.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;pd.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;pd.RasterizerState.DepthClipEnable=TRUE;
 pd.DepthStencilState.DepthEnable=FALSE;pd.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ZERO;
 pd.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_ALWAYS;
 auto& blend=pd.BlendState.RenderTarget[0];blend.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;blend.BlendEnable=TRUE;
 blend.SrcBlend=D3D12_BLEND_ONE;blend.DestBlend=D3D12_BLEND_ONE;blend.BlendOp=D3D12_BLEND_OP_ADD;
 blend.SrcBlendAlpha=D3D12_BLEND_ZERO;blend.DestBlendAlpha=D3D12_BLEND_ONE;blend.BlendOpAlpha=D3D12_BLEND_OP_ADD;
 check(device_->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&smokeFilamentPipeline_)),"Create Smoke filament pipeline");
}
void Renderer::drawSmokeFilaments(FrameData data){
 transition(velocity_[v_].resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 transition(density_[d_].resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 if(data.dt>0||!smokeTracersReady_){
  data.pass=smokeTracersReady_?1.f:0.f;
  list_->SetComputeRootSignature(root_.Get());list_->SetComputeRoot32BitConstants(0,32,&data,0);
  list_->SetComputeRootDescriptorTable(1,gpu(384));list_->SetComputeRootDescriptorTable(2,gpu(213+(v_*4+d_*2)*3));
  list_->SetPipelineState(smokeTracerPipeline_.Get());list_->Dispatch(4096/64,1,1);
  smokeTracersReady_=true;
 }
 transition(smokeTracers_.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 data.pass=1;
 list_->SetGraphicsRoot32BitConstants(0,32,&data,0);
 list_->SetGraphicsRootDescriptorTable(3,gpu(392));
 list_->SetGraphicsRootDescriptorTable(2,gpu(213+(v_*4+d_*2)*3));
 list_->SetPipelineState(smokeFilamentPipeline_.Get());list_->DrawInstanced(6,4096,0,0);
 data.pass=2;list_->SetGraphicsRoot32BitConstants(0,32,&data,0);list_->DrawInstanced(62*6,128,0,0);
 transition(smokeTracers_.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 transition(velocity_[v_].resource.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 transition(density_[d_].resource.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}
void Renderer::setPaused(bool paused){if(video_&&(paused||videoRateKnown_))video_->setPaused(paused);}
double Renderer::videoPosition()const{return video_?video_->position():0;}
void Renderer::renderVideo(const Assignment& assignment,FrameData data,bool present){
 if(!video_||videoPath_!=assignment.video){
  idle();video_.reset();videoView_=nullptr;videoBytes_=0;
  video_=std::make_unique<VideoPlayer>(device_.Get(),queue_.Get(),assignment.video);videoPath_=assignment.video;videoRateKnown_=false;requestedVideoRate_=-1;acceptedVideoRate_=1;videoRateMessage_.clear();
 }
 if(!videoRateKnown_||requestedVideoRate_!=assignment.videoSettings.speed){
  try{if(video_->setPlaybackRate(assignment.videoSettings.speed)){videoRateKnown_=true;requestedVideoRate_=assignment.videoSettings.speed;acceptedVideoRate_=video_->playbackRate();videoRateMessage_.clear();}}
  catch(const VideoRateRejected&){videoRateKnown_=true;requestedVideoRate_=assignment.videoSettings.speed;acceptedVideoRate_=video_->playbackRate();videoRateMessage_=L"Selected video speed unavailable; previous speed retained.";}
 }
 if(videoRateKnown_)video_->setPaused(false);video_->update();auto source=video_->frame();
 if(source&&source!=videoView_){
  D3D12_SHADER_RESOURCE_VIEW_DESC view{};view.Format=DXGI_FORMAT_B8G8R8A8_UNORM;view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;view.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;view.Texture2D.MipLevels=1;
  device_->CreateShaderResourceView(source,&view,cpu(240));for(unsigned index=241;index<243;index++)device_->CreateShaderResourceView(nullptr,&view,cpu(index));
  auto desc=source->GetDesc();videoBytes_=device_->GetResourceAllocationInfo(0,1,&desc).SizeInBytes;videoView_=source;
 }
 begin();ID3D12DescriptorHeap* heaps[]={views_.Get()};list_->SetDescriptorHeaps(1,heaps);
 auto target=buffers_[frameIndex_].Get();transition(target,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);
 auto view=rtv_->GetCPUDescriptorHandleForHeapStart();view.ptr+=frameIndex_*rtvStride_;list_->OMSetRenderTargets(1,&view,FALSE,nullptr);
 const float black[]={0,0,0,1};list_->ClearRenderTargetView(view,black,0,nullptr);
 if(source){
  transition(source,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  D3D12_VIEWPORT viewport{0,0,float(width_),float(height_),0,1};D3D12_RECT scissor{0,0,LONG(width_),LONG(height_)};list_->RSSetViewports(1,&viewport);list_->RSSetScissorRects(1,&scissor);
  data.width=float(width_);data.height=float(height_);auto info=video_->info();data.spare[0]=float(info.width);data.spare[1]=float(info.height);if(std::abs(info.displayAspect-double(info.width)/info.height)>1e-6){data.spare[0]=float(info.displayAspect);data.spare[1]=1;}data.spare[2]=assignment.fit?1.f:0.f;data.spare[3]=float(assignment.videoSettings.positionX);data.spare[4]=float(assignment.videoSettings.positionY);
  list_->SetGraphicsRootSignature(root_.Get());list_->SetGraphicsRoot32BitConstants(0,32,&data,0);list_->SetGraphicsRootDescriptorTable(2,gpu(240));list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);list_->SetPipelineState(videoPipeline_.Get());list_->DrawInstanced(3,1,0,0);
  transition(source,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);
 }
 transition(target,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT);submit();video_->consumed();if(present){if(nonblocking_)tryPresent();else check(swap_->Present(1,0),"Present video wallpaper");}
}
void Renderer::render(const Assignment& assignment,FrameData data,bool present){
 if(assignment.scene!=Scene::Aurora&&auroraArtwork_){idle();releaseAsset(auroraArtwork_);}
 prepareBodyStatistics(assignment,int(data.palette));
 if(assignment.scene!=Scene::Video&&video_){idle();video_.reset();videoPath_.clear();videoView_=nullptr;videoBytes_=0;}

 if(lastScene_!=assignment.scene&&(lastScene_==Scene::Orbit||lastScene_==Scene::Mars||lastScene_==Scene::Earth||lastScene_==Scene::Moon||lastScene_==Scene::Jupiter||lastScene_==Scene::Saturn||lastScene_==Scene::Mercury||lastScene_==Scene::Neptune||lastScene_==Scene::Venus||lastScene_==Scene::Uranus||lastScene_==Scene::Pluto||lastScene_==Scene::Sun||lastScene_==Scene::ForestWater)){
  idle();
  auto releaseTexture=[&](ComPtr<ID3D12Resource>& texture){
   if(!texture)return;
   releaseAsset(texture);
  };
  if(lastScene_==Scene::Orbit){releaseTexture(planetTexture_);releaseTexture(cloudTexture_);releaseTexture(nightTexture_);}
  if(lastScene_==Scene::Mars){releaseTexture(marsColor_);releaseTexture(marsNormal_);}
  if(lastScene_==Scene::Saturn)releaseTexture(saturnColor_);
  if(lastScene_==Scene::Pluto)releaseTexture(plutoColor_);
  if(lastScene_==Scene::Sun){releaseTexture(sunImage_);releaseTexture(sunProminence_);}
  if(lastScene_==Scene::ForestWater)releaseTexture(forestReflection_);
  if(lastScene_==Scene::Mercury){releaseTexture(mercuryColor_);releaseTexture(mercuryNormal_);}
  if(lastScene_==Scene::Jupiter){releaseTexture(jupiterColor_);releaseTexture(jupiterAtmosphere_);}
  if(lastScene_==Scene::Moon){releaseTexture(moonColor_);releaseTexture(moonNormal_);releaseTexture(moonEarth_);}
  if(lastScene_==Scene::Earth){releaseTexture(earthColor_);releaseTexture(earthClouds_);releaseTexture(earthMaterial_);}
  if(assignment.scene!=Scene::Orbit&&assignment.scene!=Scene::Mars&&assignment.scene!=Scene::Earth&&assignment.scene!=Scene::Moon&&assignment.scene!=Scene::Jupiter&&assignment.scene!=Scene::Saturn&&assignment.scene!=Scene::Mercury&&assignment.scene!=Scene::Neptune&&assignment.scene!=Scene::Venus&&assignment.scene!=Scene::Uranus&&assignment.scene!=Scene::Pluto)releaseTexture(starTexture_);
 }
 // Only mesh geometry needs depth. With MSAA its dedicated depth target is
 // sufficient; allocate single-sample depth only for the hardware fallback.
 bool geometry=assignment.scene==Scene::Orbit||assignment.scene==Scene::Bloom;
 if(geometry&&orbitSamples_==1)makeDepthTarget();
 else if(depth_){idle();depth_.Reset();depthTargetBytes_=0;}
 if(assignment.scene==Scene::Neptune||assignment.scene==Scene::Venus||assignment.scene==Scene::Uranus)loadStars(true);
 if(assignment.scene==Scene::Saturn)loadSaturn();
 if(assignment.scene==Scene::Pluto)loadPluto();
 if(assignment.scene==Scene::Sun)loadSun();
 if(assignment.scene==Scene::ForestWater)loadForestWater();
 if(assignment.scene==Scene::Mercury)loadMercury();
 if(assignment.scene==Scene::Jupiter)loadJupiter();
 if(assignment.scene==Scene::Moon)loadMoon();
 if(assignment.scene==Scene::Earth)loadEarth();
 if(assignment.scene==Scene::Mars)loadMars();
 if(assignment.scene==Scene::Aurora)loadAurora();
 if(assignment.scene==Scene::Orbit&&(!planetTexture_||!cloudTexture_||!nightTexture_||!starTexture_))loadPlanet();
 if(geometry)makeOrbitTargets();
 else if(orbitColor_){idle();orbitColor_.Reset();orbitDepth_.Reset();orbitTargetBytes_=0;}
 if(assignment.scene==Scene::Smoke&&!velocity_[0].resource)makeFluid();
 if(assignment.scene==Scene::Smoke&&smokeFilamentsEnabled_)makeSmokeFilaments();
 if(lastScene_==Scene::Smoke&&assignment.scene!=Scene::Smoke){idle();smokeTracersReady_=false;releasePressureHierarchy();releaseFineAtlas();detailVelocity_.resource.Reset();releaseFlowmapAudit();flowmapBase_.resource.Reset();flowmapTotal_.resource.Reset();for(auto& r:coordinates_)r.resource.Reset();for(auto& r:velocity_)r.resource.Reset();for(auto& r:density_)r.resource.Reset();for(auto& r:pressure_)r.resource.Reset();divergence_.resource.Reset();curl_.resource.Reset();fluidReady_=false;allocationBytes_=uint64_t(width_)*height_*20;}
 lastScene_=assignment.scene;if(assignment.scene==Scene::Video){renderVideo(assignment,data,present);return;}data.width=float(width_);data.height=float(height_);begin();ID3D12DescriptorHeap* heaps[]={views_.Get()};list_->SetDescriptorHeaps(1,heaps);
 if(assignment.scene==Scene::Smoke){
  // Smoke advances a simulation rather than sampling time in its scene shader.
  // Apply the same Movement contract as the analytic Worlds, including zero.
  data.dt*=data.movement;data.time*=data.movement;
  if(data.movement<=0)data.reaction=0;
  data.grid=float(grid_);data.spare[2]=float(densityGridSize());for(unsigned axis=0;axis<3;axis++)data.spare[5+axis]=float(densityDimensions()[axis]);
  if(data.dt>0||!fluidReady_)simulate(data);
  if(fineAtlasEnabled_&&(data.dt>0||!fineAtlasReady_))stepFineAtlas(data);
 }
 unsigned renderTiming=beginGpuStage(11);
 bool multisample=geometry&&orbitSamples_>1;
 bool usesDepth=geometry;
 auto target=buffers_[frameIndex_].Get();if(!multisample)transition(hdr_.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
 auto rtv=rtv_->GetCPUDescriptorHandleForHeapStart();rtv.ptr+=(multisample?5:2)*rtvStride_;auto dsv=dsv_->GetCPUDescriptorHandleForHeapStart();if(multisample)dsv.ptr+=device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);list_->OMSetRenderTargets(1,&rtv,FALSE,usesDepth?&dsv:nullptr);
 const float clear[]={.002f,.003f,.006f,1};list_->ClearRenderTargetView(rtv,clear,0,nullptr);if(usesDepth)list_->ClearDepthStencilView(dsv,D3D12_CLEAR_FLAG_DEPTH,1,0,0,nullptr);
 D3D12_VIEWPORT vp{0,0,float(width_),float(height_),0,1};D3D12_RECT sc{0,0,(LONG)width_,(LONG)height_};list_->RSSetViewports(1,&vp);list_->RSSetScissorRects(1,&sc);list_->SetGraphicsRootSignature(root_.Get());list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
 auto draw=[&](ID3D12PipelineState* state,float pass,unsigned count,unsigned instances=1){data.pass=pass;list_->SetPipelineState(state);list_->SetGraphicsRoot32BitConstants(0,32,&data,0);list_->DrawInstanced(count,instances,0,0);};
 if(assignment.scene==Scene::Aurora){list_->SetGraphicsRootDescriptorTable(2,gpu(69));draw(aurora_.Get(),0,3);}
 else if(assignment.scene==Scene::Bloom){draw(bloomBackdrop_.Get(),0,3);draw(bloomSurface_.Get(),0,512*192*6);}
 else if(assignment.scene==Scene::ForestWater){list_->SetGraphicsRootDescriptorTable(2,gpu(417));draw(forestWater_.Get(),0,3);}
 else if(isProceduralWorld(assignment.scene)){draw(proceduralWorlds_[size_t(assignment.scene)-size_t(Scene::Silk)].Get(),0,3);}
 else if(assignment.scene==Scene::Uranus){list_->SetGraphicsRootDescriptorTable(2,gpu(82));draw(uranusSpace_.Get(),0,3);draw(uranus_.Get(),0,3);}
 else if(assignment.scene==Scene::Neptune){list_->SetGraphicsRootDescriptorTable(2,gpu(82));draw(neptuneSpace_.Get(),0,3);draw(neptune_.Get(),0,3);}
 else if(assignment.scene==Scene::Venus){list_->SetGraphicsRootDescriptorTable(2,gpu(82));draw(venusSpace_.Get(),0,3);draw(venus_.Get(),0,3);}
 else if(assignment.scene==Scene::Sun){list_->SetGraphicsRootDescriptorTable(2,gpu(414));draw(sun_.Get(),0,3);}
 else if(assignment.scene==Scene::Pluto){list_->SetGraphicsRootDescriptorTable(2,gpu(82));draw(plutoSpace_.Get(),0,3);list_->SetGraphicsRootDescriptorTable(2,gpu(411));draw(pluto_.Get(),0,3);}
 else if(assignment.scene==Scene::Mercury){list_->SetGraphicsRootDescriptorTable(2,gpu(82));draw(mercurySpace_.Get(),0,3);list_->SetGraphicsRootDescriptorTable(2,gpu(408));draw(mercuryPlanet_.Get(),0,3);}
 else if(assignment.scene==Scene::Saturn){list_->SetGraphicsRootDescriptorTable(2,gpu(405));draw(saturn_.Get(),0,3);}
 else if(assignment.scene==Scene::Jupiter){list_->SetGraphicsRootDescriptorTable(2,gpu(82));draw(jupiterSpace_.Get(),0,3);list_->SetGraphicsRootDescriptorTable(2,gpu(402));draw(jupiter_.Get(),0,3);}
 else if(assignment.scene==Scene::Moon){list_->SetGraphicsRootDescriptorTable(2,gpu(82));draw(moonSpace_.Get(),0,3);list_->SetGraphicsRootDescriptorTable(2,gpu(399));draw(moon_.Get(),0,3);}
 else if(assignment.scene==Scene::Earth){list_->SetGraphicsRootDescriptorTable(2,gpu(82));draw(earthSpace_.Get(),0,3);list_->SetGraphicsRootDescriptorTable(2,gpu(396));draw(earth_.Get(),0,3);}
 else if(assignment.scene==Scene::Mars){list_->SetGraphicsRootDescriptorTable(2,gpu(82));draw(marsSpace_.Get(),0,3);list_->SetGraphicsRootDescriptorTable(2,gpu(393));draw(mars_.Get(),0,3);}
 else if(assignment.scene==Scene::Orbit){list_->SetGraphicsRootDescriptorTable(2,gpu(82));draw(space_.Get(),0,3);list_->SetGraphicsRootDescriptorTable(2,gpu(72));draw(planet_.Get(),0,256*128*6);draw(planet_.Get(),3,24*12*6,400);draw(planet_.Get(),5,8*4*6,1800);draw(planet_.Get(),6,48*24*6,18);draw(planet_.Get(),4,96*48*6,3);draw(atmosphere_.Get(),7,128*64*6);draw(atmosphere_.Get(),2,256*128*6);draw(atmosphere_.Get(),1,256*128*6);}
 else if(assignment.scene==Scene::Smoke){auto visible=flowmapLive_?flowmapTotal_.resource.Get():density_[d_].resource.Get();transition(visible,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);transition(curl_.resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);list_->SetGraphicsRootDescriptorTable(2,gpu(flowmapLive_?335:76+d_*3));list_->SetGraphicsRootDescriptorTable(4,gpu(fineAtlasEnabled_?376+fineAtlasIndex_*2:380));if(fineAtlasEnabled_){transition(fineAtlases_[fineAtlasIndex_].Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);data.spare[8]=256;data.spare[9]=fineAtlasFront_?16.f:8.f;}
 list_->SetGraphicsRootDescriptorTable(3,gpu(fineLightingEnabled_?382:380));
 if(fineLightingEnabled_){transition(fineForward_.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);data.spare[11]=1;}
 draw(smoke_.Get(),0,3);if(fineLightingEnabled_)transition(fineForward_.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);if(fineAtlasEnabled_)transition(fineAtlases_[fineAtlasIndex_].Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);transition(curl_.resource.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);transition(visible,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);}
 if(assignment.scene==Scene::Smoke&&smokeFilamentsEnabled_)drawSmokeFilaments(data);
 endGpuStage(renderTiming);
 unsigned postTiming=beginGpuStage(13);
 if(multisample){
  transition(orbitColor_.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
  transition(hdr_.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RESOLVE_DEST);
  list_->ResolveSubresource(hdr_.Get(),0,orbitColor_.Get(),0,DXGI_FORMAT_R16G16B16A16_FLOAT);
  transition(orbitColor_.Get(),D3D12_RESOURCE_STATE_RESOLVE_SOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
  transition(hdr_.Get(),D3D12_RESOURCE_STATE_RESOLVE_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
 }else transition(hdr_.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
 D3D12_VIEWPORT bloomViewport{0,0,float((width_+3)/4),float((height_+3)/4),0,1};list_->RSSetViewports(1,&bloomViewport);
 for(unsigned pass=0;pass<3;pass++){
  unsigned destination=pass==1?1:0;unsigned source=pass==0?66:pass==1?67:68;
  transition(bloomTargets_[destination].Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
  auto view=rtv_->GetCPUDescriptorHandleForHeapStart();view.ptr+=(3+destination)*rtvStride_;list_->OMSetRenderTargets(1,&view,FALSE,nullptr);
  list_->SetGraphicsRootDescriptorTable(2,gpu(source));draw(bloom_.Get(),float(pass),3);
  transition(bloomTargets_[destination].Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
 }
 list_->RSSetViewports(1,&vp);
 transition(target,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);
 rtv=rtv_->GetCPUDescriptorHandleForHeapStart();rtv.ptr+=size_t(frameIndex_)*rtvStride_;list_->OMSetRenderTargets(1,&rtv,FALSE,nullptr);
 outputConstants();list_->SetGraphicsRootDescriptorTable(2,gpu(66));draw(output_.hdr?postHdr_.Get():post_.Get(),assignment.scene==Scene::Orbit?1.f:0.f,3);
 drawBodyStatistics(assignment);
 endGpuStage(postTiming);
 transition(target,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT);submit();if(present){if(nonblocking_)tryPresent();else check(swap_->Present(1,0),"Present wallpaper");}
}
void Renderer::captureFluidFields(const std::filesystem::path& directory){
 if(!fluidReady_)throw std::runtime_error("Fluid diagnostics require an initialized Smoke scene");
 idle();
 auto dump=[&](ID3D12Resource* resource,const wchar_t* name,unsigned bytesPerCell){
  auto desc=resource->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT64 total{};
  device_->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,&total);
  auto hp=heap(D3D12_HEAP_TYPE_READBACK);auto bd=bufferDesc(total);ComPtr<ID3D12Resource> readback;
  check(device_->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)),"Create fluid diagnostic readback");
  begin();transition(resource,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
  D3D12_TEXTURE_COPY_LOCATION dst{},src{};dst.pResource=readback.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=footprint;src.pResource=resource;src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  list_->CopyTextureRegion(&dst,0,0,0,&src,nullptr);transition(resource,D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);submit();idle();
  BYTE* mapped{};D3D12_RANGE range{0,SIZE_T(total)};check(readback->Map(0,&range,reinterpret_cast<void**>(&mapped)),"Map fluid diagnostic");
  std::ofstream output(directory/name,std::ios::binary);
  for(unsigned z=0;z<desc.DepthOrArraySize;z++)for(unsigned y=0;y<desc.Height;y++)
   output.write(reinterpret_cast<const char*>(mapped+footprint.Offset+(size_t(z)*desc.Height+y)*footprint.Footprint.RowPitch),std::streamsize(desc.Width*bytesPerCell));
  D3D12_RANGE none{};readback->Unmap(0,&none);
  if(!output)throw std::runtime_error("Cannot write fluid diagnostic field");
 };
 if(detailCoordinates_)dump(detailVelocity_.resource.Get(),L"detail-velocity-rgba32f.bin",16);
 if(detailCoordinates_)dump(coordinates_[coordinateIndex_].resource.Get(),L"coordinates-rgba32f.bin",16);
 dump(flowmapLive_?flowmapTotal_.resource.Get():density_[d_].resource.Get(),L"density-rgba16f.bin",8);
 dump(velocity_[v_].resource.Get(),L"velocity-rgba16f.bin",8);
 dump(divergence_.resource.Get(),L"divergence-before-r32f.bin",4);
 dump(pressure_[p_].resource.Get(),L"pressure-r32f.bin",4);
}
void Renderer::capture(const std::filesystem::path& file){
 idle();auto resource=buffers_[frameIndex_].Get();auto desc=resource->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT rows{};UINT64 rowBytes{},total{};device_->GetCopyableFootprints(&desc,0,1,0,&footprint,&rows,&rowBytes,&total);
 auto hp=heap(D3D12_HEAP_TYPE_READBACK);auto bd=bufferDesc(total);ComPtr<ID3D12Resource> readback;check(device_->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)),"Create capture buffer");
 begin();transition(resource,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_SOURCE);D3D12_TEXTURE_COPY_LOCATION dst{},src{};dst.pResource=readback.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=footprint;src.pResource=resource;src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;list_->CopyTextureRegion(&dst,0,0,0,&src,nullptr);transition(resource,D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PRESENT);submit();idle();
 if(desc.Format==DXGI_FORMAT_R16G16B16A16_FLOAT){
  BYTE* pixels{};D3D12_RANGE range{0,SIZE_T(total)};check(readback->Map(0,&range,reinterpret_cast<void**>(&pixels)),"Map HDR readback");
  auto raw=file;raw.replace_extension(L".rgba16f");std::ofstream out(raw,std::ios::binary);
  for(unsigned y=0;y<height_;y++)out.write(reinterpret_cast<char*>(pixels+y*footprint.Footprint.RowPitch),size_t(width_)*8);
  D3D12_RANGE none{};readback->Unmap(0,&none);out.close();if(!out)throw std::runtime_error("Write HDR readback");
  auto meta=raw;meta+=L".json";std::ofstream metadata(meta);metadata<<nlohmann::json({{"width",width_},{"height",height_},{"layout","tightly packed little-endian RGBA16F"},{"colorSpace","linear scRGB (Rec.709), 1.0 = 80 nits"},{"outputMode","diagnostic FP16"},{"referenceWhiteNits",output_.white},{"peakNits",output_.peak},{"physicalHdrVerified",false}}).dump(2);return;
 }
 ComPtr<IWICImagingFactory> wic;check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&wic)),"Create image encoder");ComPtr<IWICStream> stream;check(wic->CreateStream(&stream),"Create image stream");check(stream->InitializeFromFilename(file.c_str(),GENERIC_WRITE),"Open capture file");ComPtr<IWICBitmapEncoder> encoder;check(wic->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder),"Create PNG encoder");check(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache),"Initialize PNG encoder");ComPtr<IWICBitmapFrameEncode> frame;check(encoder->CreateNewFrame(&frame,nullptr),"Create PNG frame");check(frame->Initialize(nullptr),"Initialize PNG frame");check(frame->SetSize(width_,height_),"Set capture size");WICPixelFormatGUID format=GUID_WICPixelFormat32bppRGBA;check(frame->SetPixelFormat(&format),"Set capture format");
 BYTE* data{};D3D12_RANGE range{0,(SIZE_T)total};check(readback->Map(0,&range,reinterpret_cast<void**>(&data)),"Map captured pixels");
 // WIC may negotiate BGRA even when RGBA was requested. The GPU texture is RGBA.
 std::vector<BYTE> converted;
 if(format==GUID_WICPixelFormat32bppBGRA){converted.assign(data,data+total);for(unsigned y=0;y<height_;y++)for(unsigned x=0;x<width_;x++)std::swap(converted[y*footprint.Footprint.RowPitch+x*4],converted[y*footprint.Footprint.RowPitch+x*4+2]);}
 if(format!=GUID_WICPixelFormat32bppRGBA&&format!=GUID_WICPixelFormat32bppBGRA){D3D12_RANGE none{};readback->Unmap(0,&none);throw std::runtime_error("Unsupported capture pixel format");}
 HRESULT written=frame->WritePixels(height_,footprint.Footprint.RowPitch,(UINT)total,converted.empty()?data:converted.data());D3D12_RANGE none{};readback->Unmap(0,&none);check(written,"Encode captured pixels");check(frame->Commit(),"Commit PNG frame");check(encoder->Commit(),"Commit PNG");
}
}





