#pragma once
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <memory>
#include <map>
#include <string>
namespace worlds {
struct SharedTexture {Microsoft::WRL::ComPtr<ID3D12Resource> resource;uint64_t bytes=0;};
// Mutated only on the owning adapter worker. Registry acquisition is synchronized.
struct GpuContext {
 Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
 Microsoft::WRL::ComPtr<ID3D12Device> device;
 Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
 Microsoft::WRL::ComPtr<ID3D12RootSignature> root;
 std::map<std::wstring,Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelines;
 std::map<std::wstring,std::weak_ptr<SharedTexture>> textures;
 std::wstring adapterName;LUID luid{};
 static std::shared_ptr<GpuContext> acquire(LUID preferred={},bool debug=false,bool allowFallback=true);
 bool removed()const{return FAILED(device->GetDeviceRemovedReason());}
 uint64_t sharedTextureBytes(){uint64_t bytes=0;for(auto it=textures.begin();it!=textures.end();){if(auto t=it->second.lock()){bytes+=t->bytes;++it;}else it=textures.erase(it);}return bytes;}
};
}
