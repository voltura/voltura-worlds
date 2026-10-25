#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <DirectXPackedVector.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using Microsoft::WRL::ComPtr;
using DirectX::PackedVector::XMConvertFloatToHalf;
using DirectX::PackedVector::XMConvertHalfToFloat;
using V=std::array<float,4>;
struct Page{UINT slot;float blend;};
static void check(HRESULT h){if(FAILED(h))throw std::runtime_error("Atlas GPU call failed: "+std::to_string(unsigned(h)));}
static void require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
static V coarseAt(V p){V v{};for(int c=0;c<3;c++)v[c]=.2f+.2f*std::clamp(p[c],.125f,.875f);v[3]=v[0]+v[1]+v[2];return v;}
static V correction(float x,float y,float z){float s=.025f*std::sin((x+.5f)*.6f)*std::cos((y+.5f)*.4f)+.001f*z;return {s,s*.5f,-s*.25f,s*1.25f};}
int wmain(int argc,wchar_t** argv){try{
 const bool cached=argc>3&&std::wstring(argv[argc-1])==L"--cached";
 if(cached)--argc;
 require(argc==3||(argc==4&&std::wstring(argv[3])==L"--half")||(argc==6&&std::wstring(argv[3])==L"--half"&&std::wstring(argv[4])==L"--velocity"),"Supply atlas probe and transport shaders, optional --half [--velocity RGBA16F-file]");
 const bool half=argc>=4, capturedVelocity=argc==6;
 const DXGI_FORMAT fieldFormat=half?DXGI_FORMAT_R16G16B16A16_FLOAT:DXGI_FORMAT_R32G32B32A32_FLOAT;
 const size_t texelBytes=half?8:16;
 const float tolerance=half?.001f:.0005f;
 ComPtr<ID3D12Debug> debug;check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
 ComPtr<IDXGIFactory6> factory;check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
 ComPtr<IDXGIAdapter1> adapter;check(factory->EnumAdapterByGpuPreference(0,DXGI_GPU_PREFERENCE_UNSPECIFIED,IID_PPV_ARGS(&adapter)));
 ComPtr<ID3D12Device> device;check(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
 D3D12_COMMAND_QUEUE_DESC qd{};ComPtr<ID3D12CommandQueue> queue;check(device->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue)));
 ComPtr<ID3D12CommandAllocator> allocator;check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
 ComPtr<ID3D12GraphicsCommandList> list;check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));
 auto buffer=[&](UINT64 bytes,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state,D3D12_RESOURCE_FLAGS flags=D3D12_RESOURCE_FLAG_NONE){
  D3D12_HEAP_PROPERTIES hp{};hp.Type=type;D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes;d.Height=1;d.DepthOrArraySize=d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;d.Flags=flags;
  ComPtr<ID3D12Resource> r;check(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,state,nullptr,IID_PPV_ARGS(&r)));return r;};
 auto write=[&](ID3D12Resource* r,const void* data,size_t bytes){void* p{};D3D12_RANGE noRead{};check(r->Map(0,&noRead,&p));std::memcpy(p,data,bytes);r->Unmap(0,nullptr);};
 auto barrier=[&](ID3D12Resource* r,D3D12_RESOURCE_STATES from,D3D12_RESOURCE_STATES to){D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={r,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,from,to};list->ResourceBarrier(1,&b);};
 std::vector<ComPtr<ID3D12Resource>> uploads;
 auto texture=[&](UINT n,const std::vector<V>& values,D3D12_RESOURCE_FLAGS flags=D3D12_RESOURCE_FLAG_NONE){
  D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE3D;d.Width=d.Height=n;d.DepthOrArraySize=UINT16(n);d.MipLevels=1;d.Format=fieldFormat;d.SampleDesc.Count=1;
  d.Flags=flags;ComPtr<ID3D12Resource> r;check(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&r)));
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT64 bytes{};device->GetCopyableFootprints(&d,0,1,0,&fp,nullptr,nullptr,&bytes);
  auto upload=buffer(bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);BYTE* p{};D3D12_RANGE none{};check(upload->Map(0,&none,reinterpret_cast<void**>(&p)));
  for(UINT z=0;z<n;z++)for(UINT y=0;y<n;y++){
   auto row=p+fp.Offset+(size_t(z)*n+y)*fp.Footprint.RowPitch;
   if(half){for(UINT x=0;x<n;x++)for(int c=0;c<4;c++){auto h=XMConvertFloatToHalf(values[(size_t(z)*n+y)*n+x][c]);std::memcpy(row+x*8+c*2,&h,2);}}
   else std::memcpy(row,values.data()+(size_t(z)*n+y)*n,n*sizeof(V));
  }upload->Unmap(0,nullptr);
  D3D12_TEXTURE_COPY_LOCATION src{},dst{};src.pResource=upload.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=fp;dst.pResource=r.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);barrier(r.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);uploads.push_back(upload);return r;};
 std::vector<V> coarseData(64),atlasData(8000,V{999,999,999,999});
 for(int z=0;z<4;z++)for(int y=0;y<4;y++)for(int x=0;x<4;x++)coarseData[(z*4+y)*4+x]=coarseAt({(x+.5f)/4,(y+.5f)/4,(z+.5f)/4,0});
 for(int z=0;z<16;z++)for(int y=0;y<16;y++)for(int x=0;x<16;x++){
  int page=x/8+2*(y/8+2*(z/8)),slot=7-page;
  int ax=(slot%2)*10+x%8+1,ay=((slot/2)%2)*10+y%8+1,az=(slot/4)*10+z%8+1;
  V v=coarseAt({(x+.5f)/16,(y+.5f)/16,(z+.5f)/16,0}),d=correction(x,y,z);for(int c=0;c<4;c++)v[c]+=d[c];atlasData[(az*20+ay)*20+ax]=v;
 }
 auto coarse=texture(4,coarseData),atlas=texture(20,atlasData);
 UINT velocityGrid=4;std::vector<V> velocityData(64,V{.0625f,0,0,0});
 if(capturedVelocity){
  std::ifstream input(std::filesystem::path(argv[5]),std::ios::binary|std::ios::ate);require(bool(input),"Missing captured velocity");auto bytes=input.tellg();require(bytes>0&&bytes<=192ll*192*192*8,"Invalid velocity size");
  size_t cells=size_t(bytes)/8;velocityGrid=UINT(std::round(std::cbrt(double(cells))));require(velocityGrid>=4&&size_t(velocityGrid)*velocityGrid*velocityGrid*8==size_t(bytes),"Velocity is not a cubic RGBA16F field");
  input.seekg(0);std::vector<uint16_t> packed(cells*4);input.read(reinterpret_cast<char*>(packed.data()),bytes);require(bool(input),"Truncated velocity input");velocityData.resize(cells);
  for(size_t i=0;i<cells;i++)for(int c=0;c<4;c++){float v=XMConvertHalfToFloat(packed[i*4+c]);require(std::isfinite(v),"Nonfinite captured velocity");velocityData[i][c]=v;}
 }
 auto sampleVelocity=[&](V p){V result{};int base[3];float f[3];for(int c=0;c<3;c++){float v=p[c]*velocityGrid-.5f;base[c]=int(std::floor(v));f[c]=v-base[c];}
  for(int z=0;z<2;z++)for(int y=0;y<2;y++)for(int x=0;x<2;x++){UINT ix=UINT(std::clamp(base[0]+x,0,int(velocityGrid)-1)),iy=UINT(std::clamp(base[1]+y,0,int(velocityGrid)-1)),iz=UINT(std::clamp(base[2]+z,0,int(velocityGrid)-1));float w=(x?f[0]:1-f[0])*(y?f[1]:1-f[1])*(z?f[2]:1-f[2]);auto v=velocityData[(size_t(iz)*velocityGrid+iy)*velocityGrid+ix];for(int c=0;c<3;c++)result[c]+=w*v[c];}return result;};
 auto velocity=texture(velocityGrid,velocityData);
 auto reconstructVelocity=[&](V p){if(!capturedVelocity)return sampleVelocity(p);V result{};for(int c=0;c<3;c++){V shifted=p;shifted[c]+=.5f/velocityGrid;result[c]=sampleVelocity(shifted)[c];}return result;};
 auto nextAtlas=texture(20,atlasData,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
 ComPtr<ID3D12Resource> forwardAtlas;
 if(cached){auto desc=nextAtlas->GetDesc();desc.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;check(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&forwardAtlas)));}
 std::vector<V> points;
 for(int z=0;z<=16;z++)for(int y=0;y<=16;y++)for(int x=0;x<=16;x++)points.push_back({x/16.f,y/16.f,z/16.f,0});
 for(int axis=0;axis<3;axis++)for(float offset:{-.000001f,0.f,.000001f}){V p{.5f,.5f,.5f,0};p[axis]+=offset;points.push_back(p);}
 UINT count=UINT(points.size());auto positions=buffer(count*sizeof(V),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);write(positions.Get(),points.data(),count*sizeof(V));
 auto pageBuffer=buffer(8*sizeof(Page),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
 auto output=buffer(count*sizeof(V)*3,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
 auto readback=buffer(count*sizeof(V)*3,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
 auto destinations=buffer(8*sizeof(UINT),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
 UINT inverse[8];for(UINT i=0;i<8;i++)inverse[i]=7-i;write(destinations.Get(),inverse,sizeof(inverse));
 D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=14;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
 ComPtr<ID3D12DescriptorHeap> heap;check(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)));UINT increment=device->GetDescriptorHandleIncrementSize(hd.Type);
 auto cpu=[&](UINT i){auto h=heap->GetCPUDescriptorHandleForHeapStart();h.ptr+=size_t(i)*increment;return h;};
 D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE3D;srv.Format=fieldFormat;srv.Texture3D.MipLevels=1;
 device->CreateShaderResourceView(coarse.Get(),&srv,cpu(0));device->CreateShaderResourceView(atlas.Get(),&srv,cpu(1));
 srv={};srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;srv.Buffer.NumElements=8;srv.Buffer.StructureByteStride=sizeof(Page);device->CreateShaderResourceView(pageBuffer.Get(),&srv,cpu(2));
 srv.Buffer.NumElements=count;srv.Buffer.StructureByteStride=sizeof(V);device->CreateShaderResourceView(positions.Get(),&srv,cpu(3));
 D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;uav.Buffer.NumElements=count*3;uav.Buffer.StructureByteStride=sizeof(V);device->CreateUnorderedAccessView(output.Get(),nullptr,&uav,cpu(4));
 srv={};srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE3D;srv.Format=fieldFormat;srv.Texture3D.MipLevels=1;device->CreateShaderResourceView(velocity.Get(),&srv,cpu(8));
 device->CreateShaderResourceView(coarse.Get(),&srv,cpu(5));device->CreateShaderResourceView(atlas.Get(),&srv,cpu(6));
 srv={};srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;srv.Buffer.NumElements=8;srv.Buffer.StructureByteStride=sizeof(UINT);device->CreateShaderResourceView(destinations.Get(),&srv,cpu(9));
 srv.Buffer.StructureByteStride=sizeof(Page);device->CreateShaderResourceView(pageBuffer.Get(),&srv,cpu(7));
 uav={};uav.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE3D;uav.Format=fieldFormat;uav.Texture3D.WSize=20;device->CreateUnorderedAccessView(nextAtlas.Get(),nullptr,&uav,cpu(10));
 if(cached){
  srv={};srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE3D;srv.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;srv.Texture3D.MipLevels=1;device->CreateShaderResourceView(forwardAtlas.Get(),&srv,cpu(11));device->CreateShaderResourceView(nullptr,&srv,cpu(13));
  uav.Format=srv.Format;device->CreateUnorderedAccessView(forwardAtlas.Get(),nullptr,&uav,cpu(12));
 }
 D3D12_DESCRIPTOR_RANGE ranges[3]={{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,4,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,5,0,0}};
 D3D12_ROOT_PARAMETER params[4]{};params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[0].Constants={0,0,4};
 for(int i=0;i<2;i++){params[i+1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[i+1].DescriptorTable={1,&ranges[i]};}
 D3D12_STATIC_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;
 D3D12_ROOT_SIGNATURE_DESC rd{3,params,1,&sampler,D3D12_ROOT_SIGNATURE_FLAG_NONE};ComPtr<ID3DBlob> signature,errors;check(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&signature,&errors));ComPtr<ID3D12RootSignature> root;check(device->CreateRootSignature(0,signature->GetBufferPointer(),signature->GetBufferSize(),IID_PPV_ARGS(&root)));
 std::ifstream file(std::filesystem::path(argv[1]),std::ios::binary);std::vector<char> shader((std::istreambuf_iterator<char>(file)),{});require(!shader.empty(),"Missing shader");
 D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.CS={shader.data(),shader.size()};ComPtr<ID3D12PipelineState> pipeline;check(device->CreateComputePipelineState(&pd,IID_PPV_ARGS(&pipeline)));
 ComPtr<ID3D12Fence> fence;check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));
 HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);require(event!=nullptr,"Fence event");float maxError=0;
 for(UINT mode=0;mode<4;mode++){
  if(mode){check(allocator->Reset());check(list->Reset(allocator.Get(),nullptr));barrier(output.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);}
  Page pages[8];for(UINT p=0;p<8;p++)pages[p]={mode==0?UINT_MAX:7-p,mode==2?(p%3)*.5f:mode==3?0.f:1.f};if(mode==2)pages[5].slot=UINT_MAX;
  write(pageBuffer.Get(),pages,sizeof(pages));list->SetPipelineState(pipeline.Get());list->SetComputeRootSignature(root.Get());ID3D12DescriptorHeap* heaps[]={heap.Get()};list->SetDescriptorHeaps(1,heaps);
  UINT constants[]={16,8,2,count};list->SetComputeRoot32BitConstants(0,4,constants,0);auto gpu=heap->GetGPUDescriptorHandleForHeapStart();list->SetComputeRootDescriptorTable(1,gpu);gpu.ptr+=4ull*increment;list->SetComputeRootDescriptorTable(2,gpu);list->Dispatch((count+63)/64,1,1);
  barrier(output.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);list->CopyResource(readback.Get(),output.Get());check(list->Close());ID3D12CommandList* lists[]={list.Get()};queue->ExecuteCommandLists(1,lists);check(queue->Signal(fence.Get(),mode+1));check(fence->SetEventOnCompletion(mode+1,event));require(WaitForSingleObject(event,10000)==WAIT_OBJECT_0,"Atlas GPU timeout");
  V* result{};D3D12_RANGE range{0,count*sizeof(V)*3};check(readback->Map(0,&range,reinterpret_cast<void**>(&result)));
  for(UINT i=0;i<count;i++){
   V expected=coarseAt(points[i]);int base[3];float f[3];for(int a=0;a<3;a++){float s=points[i][a]*16-.5f;base[a]=int(std::floor(s));f[a]=s-base[a];}
   for(int z=0;z<2;z++)for(int y=0;y<2;y++)for(int x=0;x<2;x++){
    int ix=std::clamp(base[0]+x,0,15),iy=std::clamp(base[1]+y,0,15),iz=std::clamp(base[2]+z,0,15);auto page=pages[ix/8+2*(iy/8+2*(iz/8))];if(page.slot==UINT_MAX)continue;
    float w=(x?f[0]:1-f[0])*(y?f[1]:1-f[1])*(z?f[2]:1-f[2])*page.blend;auto d=correction(ix,iy,iz);for(int c=0;c<4;c++)expected[c]+=w*d[c];
   }
   for(int c=0;c<4;c++){require(std::isfinite(result[i][c]),"Nonfinite atlas sample");float e=std::abs(result[i][c]-expected[c]);maxError=std::max(maxError,e);require(e<tolerance,"Atlas differs from independent field oracle");}
   if(mode==0||mode==3)require(std::memcmp(&result[i],&result[i+count],sizeof(V))==0,"Empty/faded atlas changed coarse density");
   // Validate the direct cell-center path against the independent analytic
   // field, including partially faded and absent pages and domain clamping.
   int cell[3];V center{};for(int axis=0;axis<3;axis++){cell[axis]=std::clamp(int(std::floor(points[i][axis]*16)),0,15);center[axis]=(cell[axis]+.5f)/16;}
   V centerExpected=coarseAt(center);auto page=pages[cell[0]/8+2*(cell[1]/8+2*(cell[2]/8))];
   if(page.slot!=UINT_MAX){auto delta=correction(cell[0],cell[1],cell[2]);for(int c=0;c<4;c++)centerExpected[c]+=delta[c]*page.blend;}
   for(int c=0;c<4;c++)require(std::isfinite(result[i+count*2][c])&&std::abs(result[i+count*2][c]-centerExpected[c])<tolerance,"Fine cell center differs from independent field oracle");
  }
  D3D12_RANGE empty{};readback->Unmap(0,&empty);
 }
 // Execute transport into a distinct atlas, with a known one-cell displacement.
 // Source/destination storage must never alias while a step reads prior data.
 ranges[0].NumDescriptors=5;params[0].Constants.Num32BitValues=16;
 if(cached){params[3].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[3].DescriptorTable={1,&ranges[2]};rd.NumParameters=4;}
 signature.Reset();errors.Reset();check(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&signature,&errors));
 ComPtr<ID3D12RootSignature> stepRoot;check(device->CreateRootSignature(0,signature->GetBufferPointer(),signature->GetBufferSize(),IID_PPV_ARGS(&stepRoot)));
 std::ifstream stepFile(std::filesystem::path(argv[2]),std::ios::binary);std::vector<char> stepShader((std::istreambuf_iterator<char>(stepFile)),{});require(!stepShader.empty(),"Missing transport shader");
 pd.pRootSignature=stepRoot.Get();pd.CS={stepShader.data(),stepShader.size()};ComPtr<ID3D12PipelineState> stepPipeline;check(device->CreateComputePipelineState(&pd,IID_PPV_ARGS(&stepPipeline)));
 auto textureDesc=nextAtlas->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT64 textureBytes{};device->GetCopyableFootprints(&textureDesc,0,1,0,&fp,nullptr,nullptr,&textureBytes);
 auto textureReadback=buffer(textureBytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
 Page allPages[8];for(UINT p=0;p<8;p++)allPages[p]={7-p,1};write(pageBuffer.Get(),allPages,sizeof(allPages));
 double fractionalError[2]{};
 for(UINT step=0;step<(capturedVelocity?5u:9u);step++){
  check(allocator->Reset());check(list->Reset(allocator.Get(),stepPipeline.Get()));
  barrier(nextAtlas.Get(),step?D3D12_RESOURCE_STATE_COPY_SOURCE:D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  list->SetComputeRootSignature(stepRoot.Get());ID3D12DescriptorHeap* heaps[]={heap.Get()};list->SetDescriptorHeaps(1,heaps);
  struct Constants{UINT n,interior,axis,slots;float dt;UINT initialize,halfStorage,staggeredVelocityGrid;float decay;UINT reserved[7];} constants{16,8,2,8,(step==0||step==3)?(capturedVelocity?1.f/60:1.f):(step>=5?.5f:(step==4&&!capturedVelocity?1.f:0.f)),step==1?1u:0u,half?1u:0u,capturedVelocity?velocityGrid:0,step==3?.22f:0.f,{(step==4||step==6)?1u:0u,0,0}};
  if(step>=7){constants.dt=1.f/60;float sourceFlow=step==7?1.f:0.f;std::memcpy(&constants.reserved[2],&sourceFlow,4);constants.reserved[4]=1;}
  auto gpuAt=[&](UINT i){auto h=heap->GetGPUDescriptorHandleForHeapStart();h.ptr+=size_t(i)*increment;return h;};
  list->SetComputeRootDescriptorTable(1,gpuAt(5));
  if(cached){
   if(!constants.initialize){
    barrier(forwardAtlas.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    constants.reserved[5]=1;list->SetComputeRoot32BitConstants(0,16,&constants,0);list->SetComputeRootDescriptorTable(2,gpuAt(12));list->SetComputeRootDescriptorTable(3,gpuAt(13));list->Dispatch(64,1,1);
    barrier(forwardAtlas.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   }
   constants.reserved[5]=2;list->SetComputeRootDescriptorTable(3,gpuAt(constants.initialize?13:11));
  }
  list->SetComputeRoot32BitConstants(0,16,&constants,0);list->SetComputeRootDescriptorTable(2,gpuAt(10));list->Dispatch(64,1,1);
  barrier(nextAtlas.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
  D3D12_TEXTURE_COPY_LOCATION from{},to{};from.pResource=nextAtlas.Get();from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;to.pResource=textureReadback.Get();to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;to.PlacedFootprint=fp;list->CopyTextureRegion(&to,0,0,0,&from,nullptr);
  check(list->Close());ID3D12CommandList* submitted[]={list.Get()};queue->ExecuteCommandLists(1,submitted);check(queue->Signal(fence.Get(),5+step));check(fence->SetEventOnCompletion(5+step,event));require(WaitForSingleObject(event,10000)==WAIT_OBJECT_0,"Atlas transport timeout");
  BYTE* data{};D3D12_RANGE range{0,SIZE_T(textureBytes)};check(textureReadback->Map(0,&range,reinterpret_cast<void**>(&data)));
  double added[3]{};
  for(int z=0;z<16;z++)for(int y=0;y<16;y++)for(int x=0;x<16;x++){
   int page=x/8+2*(y/8+2*(z/8)),slot=7-page,ax=(slot%2)*10+x%8+1,ay=((slot/2)%2)*10+y%8+1,az=(slot/4)*10+z%8+1;
   V actual;auto texel=data+fp.Offset+(size_t(az)*20+ay)*fp.Footprint.RowPitch+ax*texelBytes;
   if(half){for(int c=0;c<4;c++){uint16_t h;std::memcpy(&h,texel+c*2,2);actual[c]=XMConvertHalfToFloat(h);}}
   else std::memcpy(actual.data(),texel,sizeof(V));
   V position{(x+.5f)/16,(y+.5f)/16,(z+.5f)/16,0},departure=position;
   if(step==0||step==3||step>=4){auto v=reconstructVelocity(position);V mid=position;for(int c=0;c<3;c++)mid[c]-=v[c]*constants.dt*.5f;v=reconstructVelocity(mid);for(int c=0;c<3;c++)departure[c]-=v[c]*constants.dt;}
   V expected{};bool inside=true;for(int c=0;c<3;c++)inside&=departure[c]>=0&&departure[c]<=1;
   if(inside){expected=coarseAt(departure);if(step!=1){
    int base[3];float f[3];for(int c=0;c<3;c++){float s=departure[c]*16-.5f;base[c]=int(std::floor(s));f[c]=s-base[c];}
    for(int dz=0;dz<2;dz++)for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++){
     auto d=correction(std::clamp(base[0]+dx,0,15),std::clamp(base[1]+dy,0,15),std::clamp(base[2]+dz,0,15));float w=(dx?f[0]:1-f[0])*(dy?f[1]:1-f[1])*(dz?f[2]:1-f[2]);for(int c=0;c<4;c++)expected[c]+=w*d[c];
    }
   }}
   if(step==5||step==6){expected=coarseAt(departure);auto detail=correction(x-.5f,float(y),float(z));for(int c=0;c<4;c++)expected[c]+=detail[c];if(x>=3&&x<=12&&y>=2&&y<=13&&z>=2&&z<=13)for(int c=0;c<3;c++){double error=double(actual[c])-expected[c];fractionalError[step-5]+=error*error;}}
   if(step==3)for(float& value:expected)value*=std::exp(-constants.dt*constants.decay);
   if(step==7)for(int c=0;c<3;c++){
    require(actual[c]>=expected[c]-tolerance&&actual[c]<=expected[c]+.026f,"Source injection changed unrelated density excessively");
    if(y<6)added[c]+=actual[c]-expected[c];
    if(y>=8)require(std::abs(actual[c]-expected[c])<tolerance,"Source leaked far above emitters");
   }
   for(int c=0;c<4;c++)if(!(std::isfinite(actual[c])&&actual[c]>=0&&((step>=5&&step<=7)||std::abs(actual[c]-expected[c])<tolerance))){
    std::cerr<<"step="<<step<<" cell="<<x<<","<<y<<","<<z<<" channel="<<c<<" actual="<<actual[c]<<" expected="<<expected[c]<<'\n';
    throw std::runtime_error("Fine-density transport mismatch");
   }
   require(std::abs(actual[3]-actual[0]-actual[1]-actual[2])<(half?.001f:.00001f),"Transport lost pigment consistency");
  }
  if(step==7)for(double mass:added)require(mass>.002,"Fine atlas failed to receive an emitter pigment");
  D3D12_RANGE empty{};textureReadback->Unmap(0,&empty);
 }
 if(!capturedVelocity){std::cout<<"Fractional translation squared error: first="<<fractionalError[0]<<" corrected="<<fractionalError[1]<<char(10);require(fractionalError[1]<fractionalError[0]*.8,"Correction failed to reduce fractional transport error");}
 CloseHandle(event);ComPtr<ID3D12InfoQueue> messages;check(device.As(&messages));
 for(UINT64 i=0;i<messages->GetNumStoredMessagesAllowedByRetrievalFilter();i++){SIZE_T bytes{};messages->GetMessage(i,nullptr,&bytes);std::vector<char> data(bytes);auto m=reinterpret_cast<D3D12_MESSAGE*>(data.data());check(messages->GetMessage(i,m,&bytes));if(m->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(m->pDescription);}
 std::cout<<"PASS: "<<(half?"RGBA16F":"RGBA32F")<<", "<<count*4<<" native atlas samples, shuffled tiles, poisoned borders, missing/faded pages, exact coarse fallback; maximum error "<<maxError<<"; 20480 transported/initialized/stationary/decayed/corrected fine cells and pigment consistency. Velocity: "<<(capturedVelocity?"captured Smoke field":"constant fixture")<<". D3D12 debug validation passed.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
