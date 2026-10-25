#include "renderer.h"
#include "smoke_detail_cache.h"
#include <fstream>
#include <cstring>
namespace worlds {
void Renderer::releaseFineAtlas(){
 for(auto& r:fineAtlases_)r.Reset();fineCoarseHistory_.Reset();fineForward_.Reset();finePages_.Reset();fineDestinations_.Reset();fineAtlasReady_=false;fineAtlasIndex_=0;fineAtlasBytes_=0;
}
void Renderer::makeFineAtlas(){
 if(wideDensity_||flowmapLive_||!referenceVolume_.empty())throw std::runtime_error("Fine atlas capture requires ordinary cubic live Smoke");
 auto make=[&](D3D12_RESOURCE_DESC desc,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state,ComPtr<ID3D12Resource>& r){D3D12_HEAP_PROPERTIES heap{};heap.Type=type;check(device_->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,state,nullptr,IID_PPV_ARGS(&r)),"Create fine atlas resource");fineAtlasBytes_+=device_->GetResourceAllocationInfo(0,1,&desc).SizeInBytes;};
 const unsigned atlasAxis=fineAtlasFront_?16:8,slots=atlasAxis*atlasAxis*atlasAxis,edge=atlasAxis*10;
 D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE3D;d.Width=d.Height=d.DepthOrArraySize=UINT16(edge);d.MipLevels=1;d.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;d.SampleDesc.Count=1;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
 for(auto& r:fineAtlases_)make(d,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,r);
 d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;make(d,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,fineForward_);d.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
 d.Width=d.Height=d.DepthOrArraySize=UINT16(densityGridSize());d.Flags=D3D12_RESOURCE_FLAG_NONE;make(d,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,fineCoarseHistory_);
 auto upload=[&](const void* bytes,size_t count,ComPtr<ID3D12Resource>& r){D3D12_RESOURCE_DESC b{};b.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;b.Width=count;b.Height=1;b.DepthOrArraySize=b.MipLevels=1;b.SampleDesc.Count=1;b.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;make(b,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,r);void* p{};D3D12_RANGE none{};check(r->Map(0,&none,&p),"Map fine atlas table");std::memcpy(p,bytes,count);r->Unmap(0,nullptr);};
 struct Page{uint32_t slot;float blend;};std::vector<Page> pages(32*32*32,Page{UINT_MAX,0});std::vector<uint32_t> destinations(slots,UINT_MAX);SmokeDetailCache cache(32*32*32,slots);
 // Fixed capture regions; front mode covers more of the visible composition.
 unsigned x0=fineAtlasFront_?8:12,x1=fineAtlasFront_?24:20;
 unsigned y0=fineAtlasFront_?4:12,y1=fineAtlasFront_?28:20;
 unsigned z0=fineAtlasFront_?8:12,z1=fineAtlasFront_?18:20;
 for(unsigned z=z0;z<z1;z++)for(unsigned y=y0;y<y1;y++)for(unsigned x=x0;x<x1;x++){unsigned page=x+32*(y+32*z);auto handle=cache.acquire(page).value();pages[page]={handle.slot,1};destinations[handle.slot]=page;}
 upload(pages.data(),pages.size()*sizeof(Page),finePages_);upload(destinations.data(),destinations.size()*4,fineDestinations_);
 D3D12_SHADER_RESOURCE_VIEW_DESC texture{};texture.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;texture.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;texture.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE3D;texture.Texture3D.MipLevels=1;
 auto structured=[&](ID3D12Resource* r,unsigned index,unsigned count,unsigned stride){D3D12_SHADER_RESOURCE_VIEW_DESC s{};s.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;s.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;s.Buffer.NumElements=count;s.Buffer.StructureByteStride=stride;device_->CreateShaderResourceView(r,&s,cpu(index));};
 for(unsigned a=0;a<2;a++)for(unsigned v=0;v<2;v++){
  unsigned base=352+(a*2+v)*6;device_->CreateShaderResourceView(fineCoarseHistory_.Get(),&texture,cpu(base));device_->CreateShaderResourceView(fineAtlases_[a].Get(),&texture,cpu(base+1));structured(finePages_.Get(),base+2,32*32*32,8);device_->CreateShaderResourceView(velocity_[v].resource.Get(),&texture,cpu(base+3));structured(fineDestinations_.Get(),base+4,slots,4);
  D3D12_UNORDERED_ACCESS_VIEW_DESC u{};u.Format=texture.Format;u.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE3D;u.Texture3D.WSize=edge;device_->CreateUnorderedAccessView(fineAtlases_[1-a].Get(),nullptr,&u,cpu(base+5));
 }
 for(unsigned a=0;a<2;a++){device_->CreateShaderResourceView(fineAtlases_[a].Get(),&texture,cpu(376+a*2));structured(finePages_.Get(),377+a*2,32*32*32,8);}
 auto forwardView=texture;forwardView.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;
 device_->CreateShaderResourceView(fineForward_.Get(),&forwardView,cpu(382));
 device_->CreateShaderResourceView(nullptr,&forwardView,cpu(384));
 D3D12_UNORDERED_ACCESS_VIEW_DESC forwardUav{};forwardUav.Format=forwardView.Format;forwardUav.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE3D;forwardUav.Texture3D.WSize=edge;
 device_->CreateUnorderedAccessView(fineForward_.Get(),nullptr,&forwardUav,cpu(383));
 if(!fineRoot_){
  D3D12_DESCRIPTOR_RANGE ranges[3]={{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,5,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,5,0,0}};D3D12_ROOT_PARAMETER p[4]{};p[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;p[0].Constants={0,0,16};for(unsigned i=0;i<3;i++){p[i+1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;p[i+1].DescriptorTable={1,&ranges[i]};}
  D3D12_STATIC_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;
  D3D12_ROOT_SIGNATURE_DESC rd{4,p,1,&sampler,D3D12_ROOT_SIGNATURE_FLAG_NONE};ComPtr<ID3DBlob> blob,error;check(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error),"Serialize fine root");check(device_->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&fineRoot_)),"Create fine root");
  std::ifstream file(assetDirectory_.parent_path()/(fineCurlStrength_>0?L"shaders/smoke_atlas_cached_curl.cso":L"shaders/smoke_atlas_cached.cso"),std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});if(bytes.empty())throw std::runtime_error("Missing fine-density transport shader");D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};pd.pRootSignature=fineRoot_.Get();pd.CS={bytes.data(),bytes.size()};check(device_->CreateComputePipelineState(&pd,IID_PPV_ARGS(&finePipeline_)),"Create fine transport pipeline");
 }
 if(fineLightingEnabled_&&!fineLightPipeline_){
  std::ifstream file(assetDirectory_.parent_path()/L"shaders/smoke_atlas_light.cso",std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});if(bytes.empty())throw std::runtime_error("Missing fine-lighting shader");
  D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};pd.pRootSignature=fineRoot_.Get();pd.CS={bytes.data(),bytes.size()};check(device_->CreateComputePipelineState(&pd,IID_PPV_ARGS(&fineLightPipeline_)),"Create fine-lighting pipeline");
 }
}
void Renderer::stepFineAtlas(const FrameData& frame){
 if(!fineAtlases_[0])makeFineAtlas();
 auto copyCoarse=[&]{auto source=density_[d_].resource.Get();transition(source,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);transition(fineCoarseHistory_.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST);list_->CopyResource(fineCoarseHistory_.Get(),source);transition(fineCoarseHistory_.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);transition(source,D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);};
 if(!fineAtlasReady_)copyCoarse();
 unsigned next=1-fineAtlasIndex_,base=352+(fineAtlasIndex_*2+v_)*6;
 transition(fineAtlases_[next].Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);transition(velocity_[v_].resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 list_->SetComputeRootSignature(fineRoot_.Get());list_->SetPipelineState(finePipeline_.Get());struct Constants{UINT grid,interior,axis,slots;float dt;UINT initialize,halfStorage,velocityGrid;float decay;UINT corrected;float time,flow,audio;UINT inject,reserved[1];float fineStrength;} c{256,8,fineAtlasFront_?16u:8u,fineAtlasFront_?4096u:512u,frame.dt,fineAtlasReady_?0u:1u,1,grid_,.22f,1,frame.time,frame.flow,frame.energy*frame.reaction,1,{0},fineCurlStrength_*frame.turbulence*frame.flow};list_->SetComputeRootDescriptorTable(1,gpu(base));
 if(fineAtlasReady_){
  transition(fineForward_.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  c.reserved[0]=1;list_->SetComputeRoot32BitConstants(0,16,&c,0);list_->SetComputeRootDescriptorTable(2,gpu(383));list_->SetComputeRootDescriptorTable(3,gpu(384));list_->Dispatch(fineAtlasFront_?32768:4096,1,1);
  transition(fineForward_.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 }
 c.reserved[0]=2;list_->SetComputeRoot32BitConstants(0,16,&c,0);list_->SetComputeRootDescriptorTable(2,gpu(base+5));list_->SetComputeRootDescriptorTable(3,gpu(fineAtlasReady_?382:384));list_->Dispatch(fineAtlasFront_?32768:4096,1,1);
 transition(velocity_[v_].resource.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);transition(fineAtlases_[next].Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);fineAtlasIndex_=next;fineAtlasReady_=true;copyCoarse();
 if(fineLightingEnabled_){
  // Predictions are dead after correction. Reuse their FP32 storage for light.
  transition(fineForward_.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  c.decay=frame.density;list_->SetPipelineState(fineLightPipeline_.Get());list_->SetComputeRoot32BitConstants(0,16,&c,0);
  list_->SetComputeRootDescriptorTable(1,gpu(352+(next*2+v_)*6));list_->SetComputeRootDescriptorTable(2,gpu(383));list_->SetComputeRootDescriptorTable(3,gpu(384));list_->Dispatch(fineAtlasFront_?32768:4096,1,1);
  transition(fineForward_.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 }
}
}
