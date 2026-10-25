#include "video.h"
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
namespace worlds {
using Microsoft::WRL::ComPtr;
namespace {
void checked(HRESULT value,const char* operation){if(FAILED(value)){std::ostringstream text;text<<operation<<" (0x"<<std::hex<<unsigned(value)<<")";throw std::runtime_error(text.str());}}
struct MediaRuntime {MediaRuntime(){checked(MFStartup(MF_VERSION),"Initialize Windows media playback");}~MediaRuntime(){MFShutdown();}};
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
VideoInfo inspectVideo(const std::filesystem::path& file){
 if(!std::filesystem::is_regular_file(file))throw std::runtime_error("Choose a local SDR H.264 MP4 video.");
 {FILE* input{};if(_wfopen_s(&input,file.c_str(),L"rb")||!input)throw std::runtime_error("Cannot open video");unsigned char header[12]{};size_t size=fread(header,1,sizeof(header),input);fclose(input);if(size!=12||memcmp(header+4,"ftyp",4))throw std::runtime_error("Only SDR H.264 MP4 videos are supported.");}
 MediaRuntime runtime;ComPtr<IMFSourceReader> reader;checked(MFCreateSourceReaderFromURL(file.c_str(),nullptr,&reader),"Open MP4 metadata");
 ComPtr<IMFMediaType> type;checked(reader->GetNativeMediaType(DWORD(MF_SOURCE_READER_FIRST_VIDEO_STREAM),0,&type),"Read video format");GUID subtype{};checked(type->GetGUID(MF_MT_SUBTYPE,&subtype),"Read video codec");
 if(subtype!=MFVideoFormat_H264)throw std::runtime_error("Only SDR H.264 MP4 videos are supported; no codec pack is required.");
 UINT32 transfer{};if(SUCCEEDED(type->GetUINT32(MF_MT_TRANSFER_FUNCTION,&transfer))&&(transfer==MFVideoTransFunc_2084||transfer==MFVideoTransFunc_HLG))throw std::runtime_error("HDR videos are not supported yet. Choose an SDR H.264 MP4.");
 VideoInfo info;checked(MFGetAttributeSize(type.Get(),MF_MT_FRAME_SIZE,&info.width,&info.height),"Read video dimensions");
 if(!info.width||!info.height||info.width>7680||info.height>4320)throw std::runtime_error("Video dimensions exceed the supported limit.");
 PROPVARIANT duration{};if(SUCCEEDED(reader->GetPresentationAttribute(DWORD(MF_SOURCE_READER_MEDIASOURCE),MF_PD_DURATION,&duration))){if(duration.vt==VT_UI8)info.duration=duration.uhVal.QuadPart/10000000.0;PropVariantClear(&duration);}
 return info;
}
std::vector<unsigned char> videoPreview(const std::filesystem::path& file){
 auto info=inspectVideo(file);MediaRuntime runtime;
 ComPtr<IMFAttributes> attributes;checked(MFCreateAttributes(&attributes,1),"Create thumbnail decoder");checked(attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING,TRUE),"Enable thumbnail conversion");
 ComPtr<IMFSourceReader> reader;checked(MFCreateSourceReaderFromURL(file.c_str(),attributes.Get(),&reader),"Open video preview");
 checked(reader->SetStreamSelection(DWORD(MF_SOURCE_READER_ALL_STREAMS),FALSE),"Disable preview audio");checked(reader->SetStreamSelection(DWORD(MF_SOURCE_READER_FIRST_VIDEO_STREAM),TRUE),"Select preview video");
 ComPtr<IMFMediaType> output;checked(MFCreateMediaType(&output),"Create thumbnail format");checked(output->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video),"Set preview video type");checked(output->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_RGB32),"Set preview pixels");checked(reader->SetCurrentMediaType(DWORD(MF_SOURCE_READER_FIRST_VIDEO_STREAM),nullptr,output.Get()),"Decode video preview");
 ComPtr<IMFSample> sample;for(unsigned attempt=0;attempt<64&&!sample;attempt++){DWORD flags{};checked(reader->ReadSample(DWORD(MF_SOURCE_READER_FIRST_VIDEO_STREAM),0,nullptr,&flags,nullptr,&sample),"Read video preview frame");if(flags&MF_SOURCE_READERF_ENDOFSTREAM)break;}
 if(!sample)throw std::runtime_error("This video has no readable frames.");
 ComPtr<IMFMediaBuffer> buffer;checked(sample->ConvertToContiguousBuffer(&buffer),"Read preview buffer");ComPtr<IMF2DBuffer> image;
 std::vector<BYTE> pixels(size_t(info.width)*info.height*4);
 BYTE* source{};LONG stride=LONG(info.width*4);DWORD length{};bool twoDimensional=SUCCEEDED(buffer.As(&image));
 if(twoDimensional)checked(image->Lock2D(&source,&stride),"Lock preview image");else{
  checked(reader->GetCurrentMediaType(DWORD(MF_SOURCE_READER_FIRST_VIDEO_STREAM),&output),"Read preview stride");UINT32 stored{};if(SUCCEEDED(output->GetUINT32(MF_MT_DEFAULT_STRIDE,&stored)))stride=LONG(stored);
  checked(buffer->Lock(&source,nullptr,&length),"Lock preview pixels");if(uint64_t(std::abs(stride))*info.height>length){buffer->Unlock();throw std::runtime_error("Invalid decoded preview buffer");}if(stride<0)source+=size_t(-stride)*(info.height-1);
 }
 if(std::abs(int64_t(stride))<int64_t(info.width)*4){if(twoDimensional)image->Unlock2D();else buffer->Unlock();throw std::runtime_error("Invalid decoded preview stride");}
 for(unsigned y=0;y<info.height;y++){auto row=pixels.data()+size_t(y)*info.width*4;memcpy(row,source+ptrdiff_t(y)*stride,size_t(info.width)*4);for(unsigned x=0;x<info.width;x++)row[x*4+3]=255;}
 if(twoDimensional)image->Unlock2D();else buffer->Unlock();
 ComPtr<IWICImagingFactory> factory;checked(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)),"Create thumbnail encoder");ComPtr<IWICBitmap> bitmap;checked(factory->CreateBitmapFromMemory(info.width,info.height,GUID_WICPixelFormat32bppBGRA,info.width*4,UINT(pixels.size()),pixels.data(),&bitmap),"Create thumbnail bitmap");
 double scale=std::min(540.0/info.width,280.0/info.height);UINT width=std::max(1u,UINT(info.width*scale)),height=std::max(1u,UINT(info.height*scale));ComPtr<IWICBitmapScaler> scaler;checked(factory->CreateBitmapScaler(&scaler),"Create thumbnail scaler");checked(scaler->Initialize(bitmap.Get(),width,height,WICBitmapInterpolationModeFant),"Scale thumbnail");
 ComPtr<IStream> stream;checked(CreateStreamOnHGlobal(nullptr,TRUE,&stream),"Create thumbnail stream");ComPtr<IWICBitmapEncoder> encoder;checked(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder),"Create PNG thumbnail");checked(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache),"Initialize thumbnail encoder");ComPtr<IWICBitmapFrameEncode> frame;checked(encoder->CreateNewFrame(&frame,nullptr),"Create thumbnail frame");checked(frame->Initialize(nullptr),"Initialize thumbnail frame");checked(frame->SetSize(width,height),"Size thumbnail");WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;checked(frame->SetPixelFormat(&format),"Set thumbnail pixel format");checked(frame->WriteSource(scaler.Get(),nullptr),"Encode thumbnail");checked(frame->Commit(),"Finish thumbnail frame");checked(encoder->Commit(),"Finish thumbnail");
 STATSTG stat{};checked(stream->Stat(&stat,STATFLAG_NONAME),"Read thumbnail length");HGLOBAL memory{};checked(GetHGlobalFromStream(stream.Get(),&memory),"Read thumbnail data");auto bytes=static_cast<BYTE*>(GlobalLock(memory));if(!bytes)throw std::runtime_error("Cannot read thumbnail memory");std::vector<BYTE> encoded(bytes,bytes+size_t(stat.cbSize.QuadPart));GlobalUnlock(memory);return encoded;
}
std::filesystem::path copyVideoToLibrary(const std::filesystem::path& library,const std::filesystem::path& source,std::stop_token stop){
 if(stop.stop_requested())throw std::runtime_error("Video import cancelled");
 inspectVideo(source);std::filesystem::create_directories(library);GUID guid{};checked(CoCreateGuid(&guid),"Create video library identifier");wchar_t name[40]{};StringFromGUID2(guid,name,40);
 auto stage=library/(std::wstring(name)+L".pending"),destination=library/name;std::filesystem::create_directory(stage);
 try{
  auto progress=[](LARGE_INTEGER,LARGE_INTEGER,LARGE_INTEGER,LARGE_INTEGER,DWORD,DWORD,HANDLE,HANDLE,LPVOID context)->DWORD{return static_cast<std::stop_token*>(context)->stop_requested()?PROGRESS_CANCEL:PROGRESS_CONTINUE;};
  if(!CopyFileExW(source.c_str(),(stage/L"video.mp4").c_str(),progress,&stop,nullptr,COPY_FILE_FAIL_IF_EXISTS))throw std::runtime_error("Video import cancelled or could not be copied. Check available disk space.");
  inspectVideo(stage/L"video.mp4");if(stop.stop_requested())throw std::runtime_error("Video import cancelled");std::filesystem::rename(stage,destination);
 }catch(...){std::error_code error;std::filesystem::remove(stage/L"video.mp4",error);std::filesystem::remove(stage,error);throw;}
 return destination/L"video.mp4";
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
  if(!width||!height||width>7680||height>4320)throw std::runtime_error("Decoded video dimensions are unsupported");metadata.width=width;metadata.height=height;
  D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;desc.MiscFlags=D3D11_RESOURCE_MISC_SHARED_NTHANDLE|D3D11_RESOURCE_MISC_SHARED;
  checked(decodeDevice->CreateTexture2D(&desc,nullptr,&output11),"Allocate GPU video output");ComPtr<IDXGIResource1> shared;checked(output11.As(&shared),"Query shared video surface");
  HANDLE handle{};checked(shared->CreateSharedHandle(nullptr,DXGI_SHARED_RESOURCE_READ|DXGI_SHARED_RESOURCE_WRITE,nullptr,&handle),"Share video texture");HRESULT opened=device->OpenSharedHandle(handle,IID_PPV_ARGS(&output12));CloseHandle(handle);checked(opened,"Open video texture in D3D12");
 }
};
VideoPlayer::VideoPlayer(ID3D12Device* device,ID3D12CommandQueue* queue,const std::filesystem::path& file):impl_(std::make_unique<Impl>()){impl_->open(device,queue,file);}
VideoPlayer::~VideoPlayer()=default;
void VideoPlayer::setPaused(bool value){auto& p=*impl_;if(p.paused==value)return;checked(value?p.engine->Pause():p.engine->Play(),"Change video playback state");p.paused=value;}
bool VideoPlayer::update(){
 auto& p=*impl_;checked(p.events->failure.load(),"Windows could not play this video");if(p.paused)return false;
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
}
