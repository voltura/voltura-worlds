#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <array>
#include <algorithm>
#include <string>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>
using Microsoft::WRL::ComPtr;
static void check(HRESULT h){if(FAILED(h))throw std::runtime_error("Pressure GPU HRESULT "+std::to_string(unsigned(h)));}
static void require(bool b,const char* text){if(!b)throw std::runtime_error(text);}
int wmain(int argc,wchar_t** argv){try{
 const bool self=argc==4&&std::wstring(argv[2])==L"--selftest";
 require(self||argc==7,"Usage: probe shader.cso input-r32f output-r32f grid periodic cycles OR shader.cso --selftest periodic");
 unsigned n=self?32:std::stoul(argv[4]),periodic=std::stoul(argv[self?3:5]),cycles=self?6:std::stoul(argv[6]);
 require(n>=4&&n<=128&&(n&(n-1))==0&&periodic<=1&&cycles>=1&&cycles<=12,"Invalid probe dimensions/mode/cycles");
 struct Level{unsigned n,base,side=0;unsigned cells()const{return n*n*n;}unsigned p()const{return base+side*cells();}};
 std::vector<Level> levels;unsigned total=0;
 for(unsigned size=n;;size/=2){levels.push_back({size,total});total+=3*size*size*size;if(size==4)break;}
 std::vector<float> initial(total);std::vector<double> reference(size_t(n)*n*n);
 if(self){
  auto coordinate=[&](int i){return periodic?(i+int(n))%int(n):std::clamp(i,0,int(n)-1);};
  auto pressure=[&](int x,int y,int z){x=coordinate(x);y=coordinate(y);z=coordinate(z);return .08*std::cos((periodic?2:1)*3.141592653589793*(x+.5)/n)+.02*std::cos((periodic?16:8)*3.141592653589793*(y+.5)/n)+.005*((x+y+z)%2?-1:1);};
  for(unsigned z=0;z<n;z++)for(unsigned y=0;y<n;y++)for(unsigned x=0;x<n;x++){
   size_t i=(size_t(z)*n+y)*n+x;reference[i]=pressure(x,y,z);
   initial[2*n*n*n+i]=float(pressure(x+1,y,z)+pressure(int(x)-1,y,z)+pressure(x,y+1,z)+pressure(x,int(y)-1,z)+pressure(x,y,z+1)+pressure(x,y,int(z)-1)-6*reference[i]);
  }
 }else{
  std::ifstream input(std::filesystem::path(argv[2]),std::ios::binary|std::ios::ate);
  require(bool(input)&&input.tellg()==std::streamoff(size_t(n)*n*n*4),"Invalid RHS file size");input.seekg(0);input.read(reinterpret_cast<char*>(initial.data()+2*n*n*n),size_t(n)*n*n*4);require(bool(input),"RHS read failed");
 }
 for(float f:initial)require(std::isfinite(f),"Nonfinite RHS");
 ComPtr<ID3D12Debug> debug;check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
 ComPtr<IDXGIFactory6> factory;check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));ComPtr<IDXGIAdapter1> adapter;check(factory->EnumAdapterByGpuPreference(0,DXGI_GPU_PREFERENCE_UNSPECIFIED,IID_PPV_ARGS(&adapter)));
 DXGI_ADAPTER_DESC1 adapterDescription{};check(adapter->GetDesc1(&adapterDescription));std::wcout<<L"Adapter: "<<adapterDescription.Description<<L"\n";
 ComPtr<ID3D12Device> device;check(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
 ComPtr<ID3D12InfoQueue> info;check(device.As(&info));
 D3D12_COMMAND_QUEUE_DESC qd{};ComPtr<ID3D12CommandQueue> queue;check(device->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue)));
 ComPtr<ID3D12CommandAllocator> alloc;check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&alloc)));
 ComPtr<ID3D12GraphicsCommandList> list;check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,alloc.Get(),nullptr,IID_PPV_ARGS(&list)));
 auto buffer=[&](UINT64 bytes,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state,D3D12_RESOURCE_FLAGS flags){
  D3D12_HEAP_PROPERTIES hp{};hp.Type=type;D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes;d.Height=1;d.DepthOrArraySize=d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;d.Flags=flags;
  ComPtr<ID3D12Resource> result;check(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,state,nullptr,IID_PPV_ARGS(&result)));return result;
 };
 const UINT64 bytes=UINT64(total)*4;
 auto fields=buffer(bytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
 auto upload=buffer(bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_FLAG_NONE);
 auto readback=buffer(bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_FLAG_NONE);
 void* mapped{};D3D12_RANGE empty{};check(upload->Map(0,&empty,&mapped));std::memcpy(mapped,initial.data(),size_t(bytes));upload->Unmap(0,nullptr);
 list->CopyBufferRegion(fields.Get(),0,upload.Get(),0,bytes);
 auto transition=[&](D3D12_RESOURCE_STATES from,D3D12_RESOURCE_STATES to){D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={fields.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,from,to};list->ResourceBarrier(1,&b);};
 transition(D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 D3D12_ROOT_PARAMETER parameters[2]{};parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;parameters[0].Constants={0,0,9};parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;parameters[1].Descriptor={0,0};
 D3D12_ROOT_SIGNATURE_DESC rd{};rd.NumParameters=2;rd.pParameters=parameters;ComPtr<ID3DBlob> blob,error;check(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error));ComPtr<ID3D12RootSignature> root;check(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root)));
 std::ifstream code(std::filesystem::path(argv[1]),std::ios::binary|std::ios::ate);require(bool(code),"Missing shader");std::vector<char> shader(size_t(code.tellg()));code.seekg(0);code.read(shader.data(),shader.size());require(bool(code),"Shader read failed");
 D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.CS={shader.data(),shader.size()};ComPtr<ID3D12PipelineState> pipeline;check(device->CreateComputePipelineState(&pd,IID_PPV_ARGS(&pipeline)));
 list->SetComputeRootSignature(root.Get());list->SetPipelineState(pipeline.Get());list->SetComputeRootUnorderedAccessView(1,fields->GetGPUVirtualAddress());
 auto dispatch=[&](size_t level,unsigned op){
  auto& l=levels[level];auto& c=levels[std::min(level+1,levels.size()-1)];
  unsigned out=op==0?l.base+(1-l.side)*l.cells():l.p();
  std::array<unsigned,9> constants{l.n,l.p(),out,l.base+2*l.cells(),op,periodic,c.n,c.base,c.p()};
  list->SetComputeRoot32BitConstants(0,9,constants.data(),0);unsigned size=op==1?c.n:l.n;list->Dispatch((size+3)/4,(size+3)/4,(size+3)/4);
  D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;b.UAV.pResource=fields.Get();list->ResourceBarrier(1,&b);
  if(op==0)l.side=1-l.side;
 };
 std::function<void(size_t)> cycle=[&](size_t level){
  if(level+1==levels.size()){for(int i=0;i<128;i++)dispatch(level,0);return;}
  for(int i=0;i<4;i++)dispatch(level,0);
  levels[level+1].side=0;dispatch(level,1);cycle(level+1);dispatch(level,2);
  for(int i=0;i<4;i++)dispatch(level,0);
 };
 for(unsigned i=0;i<cycles;i++)cycle(0);
 transition(D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);list->CopyBufferRegion(readback.Get(),0,fields.Get(),0,bytes);check(list->Close());ID3D12CommandList* lists[]={list.Get()};queue->ExecuteCommandLists(1,lists);
 ComPtr<ID3D12Fence> fence;check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);require(event!=nullptr,"Cannot create fence event");check(queue->Signal(fence.Get(),1));check(fence->SetEventOnCompletion(1,event));DWORD wait=WaitForSingleObject(event,60000);CloseHandle(event);require(wait==WAIT_OBJECT_0,"GPU wait did not complete");check(device->GetDeviceRemovedReason());
 for(UINT64 i=0;i<info->GetNumStoredMessages();i++){SIZE_T size=0;check(info->GetMessage(i,nullptr,&size));std::vector<char> storage(size);auto msg=reinterpret_cast<D3D12_MESSAGE*>(storage.data());check(info->GetMessage(i,msg,&size));require(msg->Severity>D3D12_MESSAGE_SEVERITY_ERROR,msg->pDescription);}
 D3D12_RANGE range{0,SIZE_T(bytes)};check(readback->Map(0,&range,&mapped));auto values=static_cast<float*>(mapped)+levels[0].p();bool valid=true;
 if(self){
  double mean=0,refMean=0;for(size_t i=0;i<reference.size();i++){mean+=values[i];refMean+=reference[i];}
  mean/=reference.size();refMean/=reference.size();double numerator=0,denominator=0;
  for(size_t i=0;i<reference.size();i++){double expected=reference[i]-refMean,e=(values[i]-mean)-expected;numerator+=e*e;denominator+=expected*expected;}
  double relative=std::sqrt(numerator/denominator);valid=std::isfinite(relative)&&relative<.001;std::cout<<"Manufactured pressure relative error="<<relative<<"\n";
 }else{
  std::ofstream output(std::filesystem::path(argv[3]),std::ios::binary);output.write(reinterpret_cast<const char*>(values),size_t(n)*n*n*4);valid=bool(output);
 }
 readback->Unmap(0,&empty);require(valid,"Pressure validation or output write failed");
 std::cout<<"PASS native compute/debug; hierarchy bytes="<<bytes<<" cycles="<<cycles<<"\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
