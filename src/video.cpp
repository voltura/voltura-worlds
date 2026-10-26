#include "video.h"
#include "video_format.h"
#include <d3d11_4.h>
#include <d3d10.h>
#include <dxgi1_6.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfmediaengine.h>
#include <mferror.h>
#include <wrl/client.h>
#include <atomic>
#include <stdexcept>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <wincodec.h>
#include <fstream>
#include <thread>
#include <algorithm>
#include <cmath>
namespace worlds {
using Microsoft::WRL::ComPtr;
namespace {
void checked(HRESULT value,const char* operation){if(FAILED(value)){std::ostringstream text;text<<operation<<" (0x"<<std::hex<<unsigned(value)<<")";throw std::runtime_error(text.str());}}
struct MediaRuntime {MediaRuntime(){if(FAILED(MFStartup(MF_VERSION)))throw VideoError(VideoErrorKind::Unavailable,"Native Windows media playback is unavailable. Check Windows media features.");}~MediaRuntime(){MFShutdown();}};
class Events final:public IMFMediaEngineNotify {
 std::atomic<ULONG> refs_{1};
public:
 std::atomic<HRESULT> failure{S_OK};
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(id==__uuidof(IUnknown)||id==__uuidof(IMFMediaEngineNotify)){*out=static_cast<IMFMediaEngineNotify*>(this);AddRef();return S_OK;}return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef()override{return ++refs_;}
 ULONG STDMETHODCALLTYPE Release()override{auto remaining=--refs_;if(!remaining)delete this;return remaining;}
 HRESULT STDMETHODCALLTYPE EventNotify(DWORD event,DWORD_PTR first,DWORD second)override{
  if(event==MF_MEDIA_ENGINE_EVENT_NOTIFYSTABLESTATE)SetEvent(reinterpret_cast<HANDLE>(first));
  if(event==MF_MEDIA_ENGINE_EVENT_ERROR)failure=second?static_cast<HRESULT>(second):E_FAIL;
  return S_OK;
 }
};
}
namespace {
void geometry(unsigned width,unsigned height){if(!width||!height||std::max(width,height)>7680||std::min(width,height)>4320)throw VideoError(VideoErrorKind::Geometry,"Video dimensions exceed the supported 8K landscape/portrait limit.");}
void cancellation(std::stop_token stop){if(stop.stop_requested())throw VideoError(VideoErrorKind::Cancelled,"Video import cancelled.");}
void mediaCheck(HRESULT value,const char* operation){
 if(SUCCEEDED(value))return;
 if(value==MF_E_TOPO_CODEC_NOT_FOUND||value==MF_E_INVALIDMEDIATYPE)throw VideoError(VideoErrorKind::Codec,"Windows cannot decode this video. Try an SDR H.264 MP4.");
 if(value==MF_E_UNSUPPORTED_BYTESTREAM_TYPE)throw VideoError(VideoErrorKind::Container,"Windows cannot open this video container. Try an SDR H.264 MP4.");
 if(value==MF_E_INVALID_FILE_FORMAT)throw VideoError(VideoErrorKind::Damaged,"This video is damaged or has an invalid format.");
 checked(value,operation);
}
}
namespace {
unsigned encodedColourTransfer(const std::filesystem::path& path,const std::string& container){
 std::ifstream in(path,std::ios::binary);uint64_t length=std::filesystem::file_size(path);unsigned budget=100000;
 auto bytes=[&](uint64_t at,size_t count){std::vector<BYTE> b;if(at>length||count>length-at||count>65536)return b;b.resize(count);in.clear();in.seekg(std::streamoff(at));if(!in.read(reinterpret_cast<char*>(b.data()),count))b.clear();return b;};
 auto be=[](std::span<const BYTE> b){uint64_t value=0;for(auto c:b)value=(value<<8)|c;return value;};
 if(container=="mp4"||container=="m4v"||container=="mov"){
  auto scan=[&](auto&& self,uint64_t begin,uint64_t end,unsigned depth)->unsigned{
   if(depth>12)return 0;
   while(begin+8<=end&&budget>0){--budget;auto h=bytes(begin,8);if(h.size()!=8)return 0;auto size=be(std::span(h).first(4));uint64_t header=8;if(size==1){auto more=bytes(begin+8,8);if(more.size()!=8)return 0;size=be(more);header=16;}if(!size)size=end-begin;if(size<header||size>end-begin)return 0;
    std::string tag(reinterpret_cast<const char*>(h.data()+4),4);uint64_t payload=begin+header,stop=begin+size;
    if(tag=="encv")throw VideoError(VideoErrorKind::Container,"Protected video is not supported.");
    if(tag=="colr"){auto c=bytes(payload,size_t(std::min<uint64_t>(size-header,16)));if(c.size()>=10&&(!memcmp(c.data(),"nclx",4)||!memcmp(c.data(),"nclc",4))){unsigned transfer=unsigned(be(std::span(c).subspan(6,2)));if(transfer==16||transfer==18)return transfer;}}
    if(tag=="avcC"){auto c=bytes(payload,size_t(size-header));auto transfer=h264SequenceTransfer(c);if(transfer==16||transfer==18)return transfer;}
    uint64_t skip=0;bool nested=tag=="moov"||tag=="trak"||tag=="mdia"||tag=="minf"||tag=="stbl";
    if(tag=="stsd"){nested=true;skip=8;}
    if(tag=="avc1"||tag=="avc3"||tag=="hvc1"||tag=="hev1"||tag=="vp09"||tag=="av01"){nested=true;skip=78;}
    if(nested&&skip<stop-payload){auto t=self(self,payload+skip,stop,depth+1);if(t)return t;}
    begin=stop;
   }return 0;
  };return scan(scan,0,length,0);
 }
 if(container=="mkv"||container=="webm"){
  auto scan=[&](auto&& self,uint64_t begin,uint64_t end,unsigned depth)->unsigned{
   if(depth>12)return 0;
   while(begin<end&&budget>0){--budget;auto h=bytes(begin,size_t(std::min<uint64_t>(end-begin,16)));size_t p=0;
    auto vint=[&](bool id)->uint64_t{if(p>=h.size()||!h[p])return UINT64_MAX;unsigned n=1;while(n<=8&&!(h[p]&(0x80>>(n-1))))++n;if(n>8||n>h.size()-p)return UINT64_MAX;uint64_t value=h[p++]&(id?255:0xff>>n);for(unsigned i=1;i<n;i++)value=(value<<8)|h[p++];return value;};
    auto id=vint(true),count=vint(false);if(id==UINT64_MAX||count==UINT64_MAX||p>h.size())return 0;auto payload=begin+p;auto stop=count>end-payload?end:payload+count;
    if(id==0x55ba&&count<=8){auto value=bytes(payload,size_t(count));unsigned t=unsigned(be(value));if(t==16||t==18)return t;}
    if(id==0x63a2&&count<=65536){auto value=bytes(payload,size_t(count));auto t=h264SequenceTransfer(value);if(t==16||t==18)return t;}
    if(id==0x18538067||id==0x1654ae6b||id==0xae||id==0xe0||id==0x55b0){auto t=self(self,payload,stop,depth+1);if(t)return t;}
    if(stop<=begin)return 0;begin=stop;
   }return 0;
  };return scan(scan,0,length,0);
 }return 0;
}
}
VideoInfo inspectVideo(const std::filesystem::path& file){
 if(!std::filesystem::is_regular_file(file))throw VideoError(VideoErrorKind::Container,"Choose a local video file.");
 std::ifstream input(file,std::ios::binary);std::vector<unsigned char> header(65536);input.read(reinterpret_cast<char*>(header.data()),header.size());header.resize(size_t(input.gcount()));
 VideoInfo info;info.container=videoContainer(header);if(info.container.empty())throw VideoError(VideoErrorKind::Container,"Unrecognized video container. Try an SDR H.264 MP4.");
 auto encodedTransfer=encodedColourTransfer(file,info.container);if(encodedTransfer==16||encodedTransfer==18)throw VideoError(VideoErrorKind::Hdr,"HDR video is not supported. Choose an SDR video.");
 MediaRuntime runtime;ComPtr<IMFSourceReader> reader;mediaCheck(MFCreateSourceReaderFromURL(file.c_str(),nullptr,&reader),"Read video metadata");
 ComPtr<IMFMediaType> type;auto result=reader->GetNativeMediaType(DWORD(MF_SOURCE_READER_FIRST_VIDEO_STREAM),0,&type);
 if(result==MF_E_INVALIDSTREAMNUMBER)throw VideoError(VideoErrorKind::Container,"This file has no video stream.");mediaCheck(result,"Read video stream");
 GUID subtype{};mediaCheck(type->GetGUID(MF_MT_SUBTYPE,&subtype),"Read video codec");
 if(subtype==MFVideoFormat_H264)info.codec="h264";else if(subtype==MFVideoFormat_HEVC)info.codec="hevc";
 else {char fourcc[5]{};for(unsigned i=0;i<4;i++){auto c=char((subtype.Data1>>(i*8))&255);fourcc[i]=c>=32&&c<=126?c:'?';}info.codec=fourcc;}
 if(subtype==MFVideoFormat_H264)for(auto attribute:{MF_MT_MPEG_SEQUENCE_HEADER,MF_MT_USER_DATA}){UINT32 count{};if(SUCCEEDED(type->GetBlobSize(attribute,&count))&&count>0&&count<=65536){std::vector<BYTE> blob(count);if(SUCCEEDED(type->GetBlob(attribute,blob.data(),count,nullptr))){unsigned t=h264SequenceTransfer(blob);if(t==16||t==18)throw VideoError(VideoErrorKind::Hdr,"HDR video is not supported. Choose an SDR video.");}}}
 UINT32 transfer{};if(SUCCEEDED(type->GetUINT32(MF_MT_TRANSFER_FUNCTION,&transfer))&&(transfer==MFVideoTransFunc_2084||transfer==MFVideoTransFunc_HLG))throw VideoError(VideoErrorKind::Hdr,"HDR video is not supported. Choose an SDR video; desktop HDR settings are unchanged.");
 mediaCheck(MFGetAttributeSize(type.Get(),MF_MT_FRAME_SIZE,&info.width,&info.height),"Read video dimensions");geometry(info.width,info.height);info.encodedWidth=info.width;info.encodedHeight=info.height;
 type->GetUINT32(MF_MT_VIDEO_ROTATION,&info.rotation);if(info.rotation!=0&&info.rotation!=90&&info.rotation!=180&&info.rotation!=270)throw VideoError(VideoErrorKind::Geometry,"Unsupported video rotation metadata.");
 MFGetAttributeRatio(type.Get(),MF_MT_PIXEL_ASPECT_RATIO,&info.pixelAspectX,&info.pixelAspectY);if(!info.pixelAspectX||!info.pixelAspectY||info.pixelAspectX>65536||info.pixelAspectY>65536)throw VideoError(VideoErrorKind::Geometry,"Invalid video pixel aspect ratio.");
 info.displayAspect=double(info.width)*info.pixelAspectX/(double(info.height)*info.pixelAspectY);if(info.rotation==90||info.rotation==270)info.displayAspect=1/info.displayAspect;
 PROPVARIANT duration{};if(SUCCEEDED(reader->GetPresentationAttribute(DWORD(MF_SOURCE_READER_MEDIASOURCE),MF_PD_DURATION,&duration))){if(duration.vt==VT_UI8)info.duration=duration.uhVal.QuadPart/10000000.0;PropVariantClear(&duration);}
 return info;
}
struct VideoPlayer::Impl {
 MediaRuntime runtime;
 ComPtr<ID3D12Device> device;ComPtr<ID3D12CommandQueue> queue;
 ComPtr<ID3D11Device> decodeDevice;ComPtr<ID3D11DeviceContext4> context;
 ComPtr<IMFDXGIDeviceManager> manager;ComPtr<IMFMediaEngine> engine;ComPtr<Events> events;
 ComPtr<ID3D11Texture2D> output11;ComPtr<ID3D12Resource> output12;
 ComPtr<ID3D11Fence> fence11;ComPtr<ID3D12Fence> fence12;
 VideoInfo metadata;UINT64 serial{},lastRead{};bool paused=true,hasFrame=false;
 ~Impl(){
  if(engine)engine->Shutdown();if(context)context->Flush();
  if(fence12&&lastRead&&fence12->GetCompletedValue()<lastRead){
   HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
   if(event){if(SUCCEEDED(fence12->SetEventOnCompletion(lastRead,event)))WaitForSingleObject(event,10000);CloseHandle(event);}
  }
 }
 void open(ID3D12Device* renderDevice,ID3D12CommandQueue* renderQueue,const std::filesystem::path& file){
  metadata=inspectVideo(file);device=renderDevice;queue=renderQueue;
  ComPtr<IDXGIFactory4> factory;checked(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"Create video adapter factory");ComPtr<IDXGIAdapter> adapter;
  checked(factory->EnumAdapterByLuid(device->GetAdapterLuid(),IID_PPV_ARGS(&adapter)),"Find matching video adapter");
  ComPtr<ID3D11DeviceContext> immediate;D3D_FEATURE_LEVEL level{};
  checked(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT|D3D11_CREATE_DEVICE_VIDEO_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&decodeDevice,&level,&immediate),"Create video GPU device");
  checked(immediate.As(&context),"Get video synchronization context");ComPtr<ID3D10Multithread> protection;checked(immediate.As(&protection),"Protect media GPU context");protection->SetMultithreadProtected(TRUE);
  UINT token{};checked(MFCreateDXGIDeviceManager(&token,&manager),"Create media GPU manager");checked(manager->ResetDevice(decodeDevice.Get(),token),"Set media GPU device");
  ComPtr<ID3D11Device5> modern;checked(decodeDevice.As(&modern),"Get shared fence device");checked(modern->CreateFence(0,D3D11_FENCE_FLAG_SHARED,IID_PPV_ARGS(&fence11)),"Create video shared fence");
  HANDLE handle{};checked(fence11->CreateSharedHandle(nullptr,GENERIC_ALL,nullptr,&handle),"Share video fence");HRESULT opened=device->OpenSharedHandle(handle,IID_PPV_ARGS(&fence12));CloseHandle(handle);checked(opened,"Open video fence in D3D12");
  ComPtr<IMFAttributes> attributes;checked(MFCreateAttributes(&attributes,3),"Create media engine attributes");events.Attach(new Events);
  checked(attributes->SetUnknown(MF_MEDIA_ENGINE_CALLBACK,events.Get()),"Set video events");checked(attributes->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER,manager.Get()),"Set video acceleration");checked(attributes->SetUINT32(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT,DXGI_FORMAT_B8G8R8A8_UNORM),"Set video output format");
  ComPtr<IMFMediaEngineClassFactory> mediaFactory;checked(CoCreateInstance(CLSID_MFMediaEngineClassFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&mediaFactory)),"Create Windows media engine");
  checked(mediaFactory->CreateInstance(MF_MEDIA_ENGINE_REAL_TIME_MODE,attributes.Get(),&engine),"Create frame-server media engine");
  checked(engine->SetMuted(TRUE),"Mute background video");checked(engine->SetLoop(TRUE),"Loop background video");checked(engine->SetAutoPlay(FALSE),"Disable video autoplay");
  BSTR source=SysAllocString(file.c_str());if(!source)throw std::bad_alloc();HRESULT loaded=engine->SetSource(source);SysFreeString(source);checked(loaded,"Set background video");checked(engine->Load(),"Load background video");
 }
 void makeOutput(){
  DWORD width{},height{};checked(engine->GetNativeVideoSize(&width,&height),"Read decoded video size");
  ComPtr<IMFMediaEngineEx> extended;if(SUCCEEDED(engine.As(&extended))){
   PROPVARIANT transfer{};if(SUCCEEDED(extended->GetStreamAttribute(0,MF_MT_TRANSFER_FUNCTION,&transfer))){auto t=transfer.vt==VT_UI4?transfer.ulVal:0;PropVariantClear(&transfer);if(t==MFVideoTransFunc_2084||t==MFVideoTransFunc_HLG)throw VideoError(VideoErrorKind::Hdr,"HDR video is not supported. Choose an SDR video.");}
  }
  geometry(width,height);metadata.width=width;metadata.height=height;
  DWORD aspectX{},aspectY{};if(SUCCEEDED(engine->GetVideoAspectRatio(&aspectX,&aspectY))&&aspectX&&aspectY)metadata.displayAspect=double(aspectX)/aspectY;
  D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;desc.MiscFlags=D3D11_RESOURCE_MISC_SHARED_NTHANDLE|D3D11_RESOURCE_MISC_SHARED;
  checked(decodeDevice->CreateTexture2D(&desc,nullptr,&output11),"Allocate GPU video output");ComPtr<IDXGIResource1> shared;checked(output11.As(&shared),"Query shared video surface");
  HANDLE handle{};checked(shared->CreateSharedHandle(nullptr,DXGI_SHARED_RESOURCE_READ|DXGI_SHARED_RESOURCE_WRITE,nullptr,&handle),"Share video texture");HRESULT opened=device->OpenSharedHandle(handle,IID_PPV_ARGS(&output12));CloseHandle(handle);checked(opened,"Open video texture in D3D12");
 }
};
VideoPlayer::VideoPlayer(ID3D12Device* device,ID3D12CommandQueue* queue,const std::filesystem::path& file):impl_(std::make_unique<Impl>()){impl_->open(device,queue,file);}
VideoPlayer::~VideoPlayer()=default;
bool VideoPlayer::setPlaybackRate(double value){
 VideoSettings settings;settings.speed=value;if(!validVideoSettings(settings))throw std::runtime_error("Invalid video playback speed");
 auto& p=*impl_;mediaCheck(p.events->failure.load(),"Windows could not load this video");if(p.engine->GetReadyState()<MF_MEDIA_ENGINE_READY_HAVE_METADATA)return false;
 value=canonicalVideoSpeed(value);if(std::abs(p.engine->GetPlaybackRate()-value)<1e-6)return true;
 auto previous=p.engine->GetPlaybackRate();auto result=p.engine->SetDefaultPlaybackRate(value);if(SUCCEEDED(result))result=p.engine->SetPlaybackRate(value);
 if(FAILED(result)||std::abs(p.engine->GetPlaybackRate()-value)>1e-6){p.engine->SetDefaultPlaybackRate(previous);p.engine->SetPlaybackRate(previous);throw VideoRateRejected("This video cannot play at the selected speed. The previous speed is retained.");}return true;
}
double VideoPlayer::playbackRate()const{return impl_->engine->GetPlaybackRate();}
void VideoPlayer::setPaused(bool value){auto& p=*impl_;if(p.paused==value)return;checked(value?p.engine->Pause():p.engine->Play(),"Change video playback state");p.paused=value;}
bool VideoPlayer::update(){
 auto& p=*impl_;mediaCheck(p.events->failure.load(),"Windows could not play this video");if(p.paused)return false;
 LONGLONG timestamp{};HRESULT tick=p.engine->OnVideoStreamTick(&timestamp);if(tick==S_FALSE)return false;checked(tick,"Query video frame");
 if(!p.output11)p.makeOutput();if(p.lastRead)checked(p.context->Wait(p.fence11.Get(),p.lastRead),"Wait for D3D12 video sampling");
 RECT destination{0,0,LONG(p.metadata.width),LONG(p.metadata.height)};MFARGB black{};
 checked(p.engine->TransferVideoFrame(p.output11.Get(),nullptr,&destination,&black),"Transfer decoded GPU frame");
 checked(p.context->Signal(p.fence11.Get(),++p.serial),"Signal decoded video frame");p.context->Flush();checked(p.queue->Wait(p.fence12.Get(),p.serial),"Wait for video GPU transfer");p.hasFrame=true;return true;
}
ID3D12Resource* VideoPlayer::frame()const{return impl_->hasFrame?impl_->output12.Get():nullptr;}
void VideoPlayer::consumed(){auto& p=*impl_;if(p.hasFrame){p.lastRead=++p.serial;checked(p.queue->Signal(p.fence12.Get(),p.lastRead),"Release video frame to decoder");}}
double VideoPlayer::position()const{return impl_->engine->GetCurrentTime();}
VideoInfo VideoPlayer::info()const{return impl_->metadata;}
namespace {
std::vector<unsigned char> encodePreview(const VideoInfo& info,const std::vector<BYTE>& pixels){
 ComPtr<IWICImagingFactory> factory;checked(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)),"Create thumbnail encoder");ComPtr<IWICBitmap> bitmap;
 checked(factory->CreateBitmapFromMemory(info.width,info.height,GUID_WICPixelFormat32bppBGRA,info.width*4,UINT(pixels.size()),const_cast<BYTE*>(pixels.data()),&bitmap),"Create thumbnail bitmap");
 double aspect=info.displayAspect>0?info.displayAspect:double(info.width)/info.height;UINT width=UINT(std::max(1.,std::min(540.,280.*aspect))),height=UINT(std::max(1.,width/aspect));
 ComPtr<IWICBitmapScaler> scaler;checked(factory->CreateBitmapScaler(&scaler),"Create thumbnail scaler");checked(scaler->Initialize(bitmap.Get(),width,height,WICBitmapInterpolationModeFant),"Scale thumbnail");
 ComPtr<IStream> stream;checked(CreateStreamOnHGlobal(nullptr,TRUE,&stream),"Create thumbnail stream");ComPtr<IWICBitmapEncoder> encoder;checked(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder),"Create PNG thumbnail");checked(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache),"Initialize thumbnail encoder");ComPtr<IWICBitmapFrameEncode> frame;checked(encoder->CreateNewFrame(&frame,nullptr),"Create thumbnail frame");checked(frame->Initialize(nullptr),"Initialize thumbnail frame");checked(frame->SetSize(width,height),"Size thumbnail");WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;checked(frame->SetPixelFormat(&format),"Set thumbnail pixels");checked(frame->WriteSource(scaler.Get(),nullptr),"Encode thumbnail");checked(frame->Commit(),"Finish thumbnail frame");checked(encoder->Commit(),"Finish thumbnail");
 STATSTG stat{};checked(stream->Stat(&stat,STATFLAG_NONAME),"Read thumbnail length");HGLOBAL memory{};checked(GetHGlobalFromStream(stream.Get(),&memory),"Read thumbnail data");auto bytes=static_cast<BYTE*>(GlobalLock(memory));if(!bytes)throw std::runtime_error("Cannot read thumbnail memory");std::vector<BYTE> encoded(bytes,bytes+size_t(stat.cbSize.QuadPart));GlobalUnlock(memory);return encoded;
}
}
VideoPreflight preflightVideo(const std::filesystem::path& file,std::stop_token stop,LUID preferred,unsigned timeoutMs,double speed){
 cancellation(stop);auto deadline=GetTickCount64()+std::min(timeoutMs,15000u);
 ComPtr<IDXGIFactory6> factory;checked(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"Create video validation factory");
 ComPtr<IDXGIAdapter1> adapter;ComPtr<ID3D12Device> device;
 if(preferred.HighPart||preferred.LowPart){checked(factory->EnumAdapterByLuid(preferred,IID_PPV_ARGS(&adapter)),"Find destination video adapter");checked(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)),"Validate video on destination adapter");}
 if(!device)for(UINT i=0;;i++){if(FAILED(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_UNSPECIFIED,IID_PPV_ARGS(&adapter))))break;DXGI_ADAPTER_DESC1 desc{};adapter->GetDesc1(&desc);if(!(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)&&SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))))break;}
 if(!device)throw VideoError(VideoErrorKind::Unavailable,"Native Windows video playback is unavailable on this GPU.");
 D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;ComPtr<ID3D12CommandQueue> queue;checked(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)),"Create independent video validation queue");
 VideoPlayer player(device.Get(),queue.Get(),file);player.setPaused(false);
 while(!player.frame()){cancellation(stop);if(GetTickCount64()>=deadline)throw VideoError(VideoErrorKind::Timeout,"Video validation timed out before a readable frame. Try an SDR H.264 MP4.");player.update();if(!player.frame())std::this_thread::sleep_for(std::chrono::milliseconds(10));}
 while(!player.setPlaybackRate(speed)){cancellation(stop);if(GetTickCount64()>=deadline)throw VideoError(VideoErrorKind::Timeout,"Video speed validation timed out.");std::this_thread::sleep_for(std::chrono::milliseconds(5));}
 player.setPaused(true);cancellation(stop);auto info=player.info();auto source=player.frame();auto desc=source->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT64 size{};device->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,&size);
 D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_READBACK;D3D12_RESOURCE_DESC bd{};bd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;bd.Width=size;bd.Height=1;bd.DepthOrArraySize=bd.MipLevels=1;bd.SampleDesc.Count=1;bd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
 ComPtr<ID3D12Resource> readback;checked(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)),"Create validation readback");ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;checked(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)),"Create validation allocator");checked(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)),"Create validation list");
 D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={source,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_SOURCE};list->ResourceBarrier(1,&barrier);D3D12_TEXTURE_COPY_LOCATION from{},to{};from.pResource=source;from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;to.pResource=readback.Get();to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;to.PlacedFootprint=footprint;list->CopyTextureRegion(&to,0,0,0,&from,nullptr);std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter);list->ResourceBarrier(1,&barrier);checked(list->Close(),"Close validation list");ComPtr<ID3D12Fence> fence;checked(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"Create validation fence");
 struct Flight {
  ComPtr<ID3D12Resource> resource;ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> commands;ComPtr<ID3D12Fence> fence;ComPtr<ID3D12CommandQueue> queue;ComPtr<ID3D12Device> device;
  ~Flight(){if(fence->GetCompletedValue()>=1||FAILED(device->GetDeviceRemovedReason()))return;if(FAILED(queue->Signal(fence.Get(),1)))return;HANDLE done=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(done){if(SUCCEEDED(fence->SetEventOnCompletion(1,done)))WaitForSingleObject(done,10000);CloseHandle(done);}}
 } flight{readback,allocator,list,fence,queue,device};
 ID3D12CommandList* lists[]={list.Get()};queue->ExecuteCommandLists(1,lists);player.consumed();
 checked(queue->Signal(fence.Get(),1),"Signal validation readback");
 while(fence->GetCompletedValue()<1){cancellation(stop);if(GetTickCount64()>=deadline)throw VideoError(VideoErrorKind::Timeout,"Video GPU validation timed out.");std::this_thread::sleep_for(std::chrono::milliseconds(5));}
 std::vector<BYTE> pixels(size_t(info.width)*info.height*4);BYTE* bytes{};D3D12_RANGE range{0,SIZE_T(size)};checked(readback->Map(0,&range,reinterpret_cast<void**>(&bytes)),"Read validation pixels");for(unsigned y=0;y<info.height;y++)std::memcpy(pixels.data()+size_t(y)*info.width*4,bytes+footprint.Offset+size_t(y)*footprint.Footprint.RowPitch,size_t(info.width)*4);D3D12_RANGE empty{};readback->Unmap(0,&empty);cancellation(stop);
 return {info,encodePreview(info,pixels)};
}
std::vector<unsigned char> videoPreview(const std::filesystem::path& file,std::stop_token stop,LUID adapter){return preflightVideo(file,stop,adapter).preview;}
std::filesystem::path copyVideoToLibrary(const std::filesystem::path& library,const std::filesystem::path& source,std::stop_token stop,LUID adapter){
 cancellation(stop);auto info=inspectVideo(source);std::filesystem::create_directories(library);GUID guid{};checked(CoCreateGuid(&guid),"Create video identifier");wchar_t name[40]{};StringFromGUID2(guid,name,40);
 auto stage=library/(std::wstring(name)+L".pending"),destination=library/name;auto filename=std::filesystem::path("video."+info.container);std::filesystem::create_directory(stage);
 try{auto progress=[](LARGE_INTEGER,LARGE_INTEGER,LARGE_INTEGER,LARGE_INTEGER,DWORD,DWORD,HANDLE,HANDLE,LPVOID token)->DWORD{return static_cast<std::stop_token*>(token)->stop_requested()?PROGRESS_CANCEL:PROGRESS_CONTINUE;};
  if(!CopyFileExW(source.c_str(),(stage/filename).c_str(),progress,&stop,nullptr,COPY_FILE_FAIL_IF_EXISTS)){cancellation(stop);throw VideoError(VideoErrorKind::Copy,"Video could not be copied. Check available disk space and file access.");}
  preflightVideo(stage/filename,stop,adapter);cancellation(stop);std::filesystem::rename(stage,destination);
 }catch(...){std::error_code error;std::filesystem::remove(stage/filename,error);std::filesystem::remove(stage,error);throw;}
 return destination/filename;
}
}
