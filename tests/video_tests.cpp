#include "video.h"
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <objbase.h>
#include <iostream>
#include <set>
#include <cmath>
#include <sstream>
#include <vector>
using namespace worlds;using Microsoft::WRL::ComPtr;
static void require(bool value,const char* error){if(!value)throw std::runtime_error(error);}
static void checked(HRESULT value){if(FAILED(value)){std::ostringstream error;error<<"Video test GPU error 0x"<<std::hex<<unsigned(value);throw std::runtime_error(error.str());}}
static uint64_t pixels(ID3D12Device* device,ID3D12CommandQueue* queue,VideoPlayer& player){
 auto source=player.frame();auto desc=source->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT64 size{};device->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,&size);
 D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_READBACK;D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=size;buffer.Height=1;buffer.DepthOrArraySize=buffer.MipLevels=1;buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
 ComPtr<ID3D12Resource> readback;checked(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)));
 ComPtr<ID3D12CommandAllocator> allocator;checked(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));ComPtr<ID3D12GraphicsCommandList> list;checked(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));
 D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={source,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_SOURCE};list->ResourceBarrier(1,&barrier);
 D3D12_TEXTURE_COPY_LOCATION from{},to{};from.pResource=source;from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;to.pResource=readback.Get();to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;to.PlacedFootprint=footprint;list->CopyTextureRegion(&to,0,0,0,&from,nullptr);
 std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter);list->ResourceBarrier(1,&barrier);checked(list->Close());ID3D12CommandList* submitted[]={list.Get()};queue->ExecuteCommandLists(1,submitted);player.consumed();
 ComPtr<ID3D12Fence> fence;checked(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));checked(queue->Signal(fence.Get(),1));HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);require(event!=nullptr,"Fence event failed");checked(fence->SetEventOnCompletion(1,event));DWORD wait=WaitForSingleObject(event,5000);CloseHandle(event);require(wait==WAIT_OBJECT_0,"GPU video readback timed out");
 BYTE* data{};D3D12_RANGE range{0,SIZE_T(size)};checked(readback->Map(0,&range,reinterpret_cast<void**>(&data)));uint64_t hash=1469598103934665603ull,sum=0;
 for(UINT y=0;y<desc.Height;y++)for(UINT x=0;x<desc.Width;x++)for(UINT c=0;c<3;c++){BYTE value=data[footprint.Offset+size_t(y)*footprint.Footprint.RowPitch+x*4+c];sum+=value;hash=(hash^value)*1099511628211ull;}
 D3D12_RANGE empty{};readback->Unmap(0,&empty);require(sum>desc.Width*desc.Height*30,"Decoded image is black");return hash;
}
int wmain(int argc,wchar_t** argv){try{
 require(argc==3,"Provide H.264 and unsupported-codec MP4 fixtures");checked(CoInitializeEx(nullptr,COINIT_MULTITHREADED));
 auto info=inspectVideo(argv[1]);require(info.width==320&&info.height==180,"Video dimensions incorrect");auto additional=inspectVideo(argv[2]);require(additional.width==320,"Additional native MP4 codec metadata failed");
 std::stop_source cancelled;cancelled.request_stop();bool copyCancelled=false;
 auto cancelledLibrary=std::filesystem::temp_directory_path()/(L"Voltura-cancelled-video-"+std::to_wstring(GetCurrentProcessId()));
 try{copyVideoToLibrary(cancelledLibrary,argv[1],cancelled.get_token());}catch(...){copyCancelled=true;}
 require(copyCancelled&&!std::filesystem::exists(cancelledLibrary),"Cancelled video copy must not create a library");
 ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
 ComPtr<IDXGIFactory6> factory;checked(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));ComPtr<IDXGIAdapter1> adapter;checked(factory->EnumAdapterByGpuPreference(0,DXGI_GPU_PREFERENCE_UNSPECIFIED,IID_PPV_ARGS(&adapter)));ComPtr<ID3D12Device> device;checked(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
 D3D12_COMMAND_QUEUE_DESC desc{};desc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;ComPtr<ID3D12CommandQueue> queue;checked(device->CreateCommandQueue(&desc,IID_PPV_ARGS(&queue)));
 unsigned frames=0;bool paused=false,looped=false;double previous=0;std::set<uint64_t> hashes;
 {VideoPlayer player(device.Get(),queue.Get(),argv[1]);player.setPaused(false);auto deadline=GetTickCount64()+15000;
  while(GetTickCount64()<deadline&&(frames<90||!looped)){
   if(player.update()){
    require(player.frame()!=nullptr,"Frame signal without texture");if(frames%10==0)hashes.insert(pixels(device.Get(),queue.Get(),player));else player.consumed();frames++;
    double now=player.position();if(now+.4<previous)looped=true;previous=now;
    if(frames>=30&&!paused){player.setPaused(true);Sleep(100);double before=player.position();for(int i=0;i<20;i++){Sleep(10);require(!player.update(),"Paused video produced a frame");}require(std::abs(player.position()-before)<.04,"Paused clock continued advancing");player.setPaused(false);paused=true;}
   }
   Sleep(8);
  }
  player.setPaused(true);
 }
 require(frames>=90&&hashes.size()>=5,"Video did not produce varied GPU frames");require(looped&&paused,"Loop or pause/resume did not complete");
 ComPtr<ID3D12InfoQueue> messages;if(SUCCEEDED(device.As(&messages)))for(UINT64 i=0;i<messages->GetNumStoredMessagesAllowedByRetrievalFilter();i++){SIZE_T bytes{};messages->GetMessage(i,nullptr,&bytes);std::vector<char> storage(bytes);auto message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());messages->GetMessage(i,message,&bytes);if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);}
 std::cout<<"PASS: H.264 validation, additional codec inspection, "<<frames<<" GPU frames, "<<hashes.size()<<" distinct readbacks, loop boundary and pause/resume. D3D12 shared-resource checks; hardware decoder selection not measured.\n";CoUninitialize();return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
