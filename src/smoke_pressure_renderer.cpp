#include "renderer.h"
#include <fstream>
#include <functional>
namespace worlds {
void Renderer::makePressureHierarchy(){
 if(pressureHierarchy_)return;
 if(grid_>128||(grid_&(grid_-1)))throw std::runtime_error("Unsupported multigrid capture grid");
 pressureLevels_.clear();
 unsigned total=0;for(unsigned n=grid_;;n/=2){pressureLevels_.push_back({n,total});total+=3*n*n*n;if(n==4)break;}
 D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC bd{};bd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;bd.Width=UINT64(total)*4;bd.Height=1;bd.DepthOrArraySize=bd.MipLevels=1;bd.SampleDesc.Count=1;bd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;bd.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
 check(device_->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&pressureHierarchy_)),"Create pressure hierarchy");pressureHierarchyBytes_=device_->GetResourceAllocationInfo(0,1,&bd).SizeInBytes;
 D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Format=DXGI_FORMAT_R32_FLOAT;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE3D;srv.Texture3D.MipLevels=1;device_->CreateShaderResourceView(divergence_.resource.Get(),&srv,cpu(393));
 D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.Format=DXGI_FORMAT_R32_FLOAT;uav.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE3D;uav.Texture3D.WSize=grid_;for(unsigned i=0;i<2;i++)device_->CreateUnorderedAccessView(pressure_[i].resource.Get(),nullptr,&uav,cpu(394+i));
 if(pressureRoot_)return;
 D3D12_DESCRIPTOR_RANGE ranges[2]{};ranges[0]={D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0};ranges[1]={D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,1,0,0};
 D3D12_ROOT_PARAMETER params[4]{};params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[0].Constants={0,0,9};params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;params[1].Descriptor={0,0};for(int i=0;i<2;i++){params[2+i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[2+i].DescriptorTable={1,&ranges[i]};}
 D3D12_ROOT_SIGNATURE_DESC rd{};rd.NumParameters=4;rd.pParameters=params;ComPtr<ID3DBlob> blob,error;check(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error),"Serialize pressure root");check(device_->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&pressureRoot_)),"Create pressure root");
 auto pipeline=[&](const wchar_t* filename,ComPtr<ID3D12PipelineState>& result){
  std::ifstream f(assetDirectory_.parent_path()/L"shaders"/filename,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Missing pressure shader");std::vector<char> bytes(size_t(f.tellg()));f.seekg(0);f.read(bytes.data(),bytes.size());if(!f)throw std::runtime_error("Cannot read pressure shader");
  D3D12_COMPUTE_PIPELINE_STATE_DESC d{};d.pRootSignature=pressureRoot_.Get();d.CS={bytes.data(),bytes.size()};check(device_->CreateComputePipelineState(&d,IID_PPV_ARGS(&result)),"Create pressure pipeline");
 };
 pipeline(L"smoke_pressure_probe.cso",pressureCyclePipeline_);pipeline(L"smoke_pressure_bridge.cso",pressureBridgePipeline_);
}
void Renderer::solvePressureHierarchy(){
 makePressureHierarchy();
 transition(divergence_.resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 list_->SetComputeRootSignature(pressureRoot_.Get());list_->SetComputeRootUnorderedAccessView(1,pressureHierarchy_->GetGPUVirtualAddress());list_->SetComputeRootDescriptorTable(2,gpu(393));list_->SetComputeRootDescriptorTable(3,gpu(394+p_));
 auto barrier=[&]{D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;list_->ResourceBarrier(1,&b);};
 auto dispatch=[&](size_t level,unsigned op){
  auto& l=pressureLevels_[level];auto& c=pressureLevels_[std::min(level+1,pressureLevels_.size()-1)];unsigned out=op==0?l.base+(1-l.side)*l.cells():l.offset();
  std::array<unsigned,9> constants{l.n,l.offset(),out,l.base+2*l.cells(),op,0,c.n,c.base,c.offset()};
  list_->SetPipelineState(op>=3?pressureBridgePipeline_.Get():pressureCyclePipeline_.Get());list_->SetComputeRoot32BitConstants(0,9,constants.data(),0);unsigned n=op==1?c.n:l.n;list_->Dispatch((n+3)/4,(n+3)/4,(n+3)/4);barrier();if(op==0)l.side=1-l.side;
 };
 pressureLevels_[0].side=0;dispatch(0,3);
 std::function<void(size_t)> cycle=[&](size_t l){
  if(l+1==pressureLevels_.size()){for(int i=0;i<128;i++)dispatch(l,0);return;}
  for(int i=0;i<4;i++)dispatch(l,0);pressureLevels_[l+1].side=0;dispatch(l,1);cycle(l+1);dispatch(l,2);for(int i=0;i<4;i++)dispatch(l,0);
 };
 for(int i=0;i<6;i++)cycle(0);dispatch(0,4);
 transition(divergence_.resource.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 list_->SetComputeRootSignature(root_.Get());list_->SetPipelineState(pressureMultigrid_?fluidMultigrid_.Get():fluid_.Get());list_->SetComputeRootDescriptorTable(3,gpu(289));
}
}
