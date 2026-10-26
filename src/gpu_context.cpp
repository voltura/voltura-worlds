#include "gpu_context.h"
#include <d3d12sdklayers.h>
#include <mutex>
#include <stdexcept>
#include <tuple>
#include <vector>
namespace worlds {
std::shared_ptr<GpuContext> GpuContext::acquire(LUID preferred,bool debug,bool allowFallback){
 static std::mutex mutex;static std::map<std::tuple<LONG,DWORD,bool>,std::weak_ptr<GpuContext>> contexts;
 std::lock_guard lock(mutex);
 static bool debugEnabled=false;
 if(debug&&!debugEnabled){debugEnabled=true;Microsoft::WRL::ComPtr<ID3D12Debug> layer;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&layer))))layer->EnableDebugLayer();}
 Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
 if(FAILED(CreateDXGIFactory2(debug?DXGI_CREATE_FACTORY_DEBUG:0,IID_PPV_ARGS(&factory))))throw std::runtime_error("Cannot create DXGI factory");
 std::vector<Microsoft::WRL::ComPtr<IDXGIAdapter1>> adapters;
 Microsoft::WRL::ComPtr<IDXGIAdapter1> preferredAdapter;
 if((preferred.HighPart||preferred.LowPart)&&SUCCEEDED(factory->EnumAdapterByLuid(preferred,IID_PPV_ARGS(&preferredAdapter))))adapters.push_back(preferredAdapter);
 for(UINT i=0;allowFallback;i++){Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;if(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_UNSPECIFIED,IID_PPV_ARGS(&adapter))==DXGI_ERROR_NOT_FOUND)break;adapters.push_back(adapter);}
 for(auto& adapter:adapters){
  DXGI_ADAPTER_DESC1 desc{};if(!adapter||FAILED(adapter->GetDesc1(&desc))||(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE))continue;
  auto key=std::tuple{desc.AdapterLuid.HighPart,desc.AdapterLuid.LowPart,debug};
  if(auto existing=contexts[key].lock();existing&&!existing->removed())return existing;
  auto context=std::make_shared<GpuContext>();context->factory=factory;
  if(FAILED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&context->device))))continue;
  D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
  if(FAILED(context->device->CreateCommandQueue(&q,IID_PPV_ARGS(&context->queue))))throw std::runtime_error("Cannot create shared graphics queue");
  context->queue->SetName(L"Worlds adapter graphics");context->luid=desc.AdapterLuid;context->adapterName=desc.Description;contexts[key]=context;return context;
 }
 throw std::runtime_error("A hardware Direct3D 12 GPU is required");
}
}
