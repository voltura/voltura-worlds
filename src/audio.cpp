#include "audio.h"
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>
#include <ksmedia.h>
#include <complex>
#include <cmath>
#include <algorithm>
namespace worlds {
using Microsoft::WRL::ComPtr;
AudioCapture::~AudioCapture(){configure(false,{});}
AudioFeatures AudioCapture::features()const {std::lock_guard lock(mutex_);return latest_;}
void AudioCapture::configure(bool enabled,const std::wstring& device){if(enabled==enabled_&&device==device_)return;thread_.request_stop();if(thread_.joinable())thread_.join();enabled_=enabled;device_=device;{std::lock_guard lock(mutex_);latest_={};}if(enabled)thread_=std::jthread([this,device](std::stop_token s){run(s,device);});}
std::vector<std::pair<std::wstring,std::wstring>> AudioCapture::devices(){
 std::vector<std::pair<std::wstring,std::wstring>> result;ComPtr<IMMDeviceEnumerator> e;if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&e))))return result;
 ComPtr<IMMDeviceCollection> collection;if(FAILED(e->EnumAudioEndpoints(eRender,DEVICE_STATE_ACTIVE,&collection)))return result;UINT count{};collection->GetCount(&count);
 for(UINT i=0;i<count;i++){ComPtr<IMMDevice>d;collection->Item(i,&d);LPWSTR id{};if(FAILED(d->GetId(&id)))continue;std::wstring name=id;ComPtr<IPropertyStore> props;if(SUCCEEDED(d->OpenPropertyStore(STGM_READ,&props))){PROPVARIANT v{};if(SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName,&v))&&v.vt==VT_LPWSTR)name=v.pwszVal;PropVariantClear(&v);}result.emplace_back(id,name);CoTaskMemFree(id);}return result;
}
static AudioFeatures analyze(const std::array<float,1024>& samples,unsigned cursor,float rate,float& previous){
 std::array<std::complex<float>,1024> bins;float power=0;
 for(unsigned i=0;i<1024;i++){float v=samples[(cursor+i)%1024];power+=v*v;bins[i]=v*(.5f-.5f*std::cos(6.2831853f*float(i)/1023));}
 for(unsigned i=1,j=0;i<1024;i++){unsigned bit=512;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(bins[i],bins[j]);}
 for(unsigned len=2;len<=1024;len*=2){float angle=-6.2831853f/len;std::complex<float> step(std::cos(angle),std::sin(angle));for(unsigned i=0;i<1024;i+=len){std::complex<float>w=1;for(unsigned j=0;j<len/2;j++){auto u=bins[i+j],v=bins[i+j+len/2]*w;bins[i+j]=u+v;bins[i+j+len/2]=u-v;w*=step;}}}
 auto band=[&](float lo,float hi){float energy=0;int count=0;for(unsigned i=1;i<512;i++){float hz=i*rate/1024;if(hz>=lo&&hz<hi){energy+=std::norm(bins[i])/(1024.f*1024.f);count++;}}return std::clamp((10*std::log10(energy/std::max(count,1)+1e-10f)+65)/55,0.f,1.f);};
 float rms=std::sqrt(power/1024);float energy=std::clamp((20*std::log10(rms+1e-7f)+55)/50,0.f,1.f);float transient=std::clamp((energy-previous)*5,0.f,1.f);previous=energy;return {band(30,250),band(250,3500),band(3500,16000),energy,transient};
}
void AudioCapture::run(std::stop_token stop,std::wstring requested){
 CoInitializeEx(nullptr,COINIT_MULTITHREADED);
 while(!stop.stop_requested()){
  ComPtr<IMMDeviceEnumerator> enumerator;ComPtr<IMMDevice> device;ComPtr<IAudioClient> client;ComPtr<IAudioCaptureClient> capture;WAVEFORMATEX* format=nullptr;HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);bool started=false;
  auto cleanup=[&]{if(started)client->Stop();if(format)CoTaskMemFree(format);if(event)CloseHandle(event);std::lock_guard lock(mutex_);latest_={};};
  HRESULT hr=CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator));
  if(SUCCEEDED(hr))hr=requested.empty()?enumerator->GetDefaultAudioEndpoint(eRender,eConsole,&device):enumerator->GetDevice(requested.c_str(),&device);
  if(SUCCEEDED(hr))hr=device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,&client);
  if(SUCCEEDED(hr))hr=client->GetMixFormat(&format);
  if(SUCCEEDED(hr))hr=client->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK|AUDCLNT_STREAMFLAGS_EVENTCALLBACK,0,0,format,nullptr);
  if(SUCCEEDED(hr))hr=client->SetEventHandle(event);
  if(SUCCEEDED(hr))hr=client->GetService(IID_PPV_ARGS(&capture));
  if(SUCCEEDED(hr)){hr=client->Start();started=SUCCEEDED(hr);}
  if(SUCCEEDED(hr)){
   LPWSTR id{};device->GetId(&id);std::wstring active=id?id:L"";CoTaskMemFree(id);
   bool floating=format->wFormatTag==WAVE_FORMAT_IEEE_FLOAT;
   if(format->wFormatTag==WAVE_FORMAT_EXTENSIBLE&&format->cbSize>=22)floating=reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format)->SubFormat==KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
   std::array<float,1024> samples{};unsigned cursor=0,pending=0;float previous=0;auto last=std::chrono::steady_clock::now(),deviceCheck=last;
   while(!stop.stop_requested()&&SUCCEEDED(hr)){
    WaitForSingleObject(event,100);auto now=std::chrono::steady_clock::now();
    if(requested.empty()&&now-deviceCheck>std::chrono::seconds(1)){ComPtr<IMMDevice> current;if(FAILED(enumerator->GetDefaultAudioEndpoint(eRender,eConsole,&current)))break;LPWSTR next{};current->GetId(&next);bool changed=!next||active!=next;CoTaskMemFree(next);if(changed)break;deviceCheck=now;}
    UINT packet{};hr=capture->GetNextPacketSize(&packet);while(SUCCEEDED(hr)&&packet){BYTE* bytes{};UINT count{};DWORD flags{};hr=capture->GetBuffer(&bytes,&count,&flags,nullptr,nullptr);if(FAILED(hr))break;
     for(UINT f=0;f<count;f++){float sample=0;if(!(flags&AUDCLNT_BUFFERFLAGS_SILENT)){for(unsigned c=0;c<format->nChannels;c++){auto data=bytes+size_t(f)*format->nBlockAlign+c*(format->wBitsPerSample/8);float value=0;if(floating&&format->wBitsPerSample==32)memcpy(&value,data,4);else if(format->wBitsPerSample==16){int16_t v;memcpy(&v,data,2);value=v/32768.f;}else if(format->wBitsPerSample==32){int32_t v;memcpy(&v,data,4);value=v/2147483648.f;}else if(format->wBitsPerSample==24){int32_t v=int32_t(data[0])|(int32_t(data[1])<<8)|(int32_t(data[2])<<16);if(v&0x800000)v|=int32_t(0xff000000);value=v/8388608.f;}sample+=std::isfinite(value)?value:0;}sample/=format->nChannels;}
      samples[cursor]=sample;cursor=(cursor+1)%1024;if(++pending>=256){pending=0;auto features=analyze(samples,cursor,float(format->nSamplesPerSec),previous);std::lock_guard lock(mutex_);latest_=features;}
     }capture->ReleaseBuffer(count);last=now;hr=capture->GetNextPacketSize(&packet);
    }
    if(now-last>std::chrono::milliseconds(250)){std::lock_guard lock(mutex_);latest_={};}
   }
  }
  cleanup();for(int i=0;i<10&&!stop.stop_requested();i++)std::this_thread::sleep_for(std::chrono::milliseconds(100));
 }
 CoUninitialize();
}
}
