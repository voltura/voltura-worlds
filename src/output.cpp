#include "output.h"
#include "desktop.h"
#include <wrl/client.h>
namespace worlds {
OutputConfig discoverOutput(const Monitor& monitor,OutputPreference preference,bool video){
 if(preference==OutputPreference::Sdr||video)return resolveOutput(preference,video,false,0,0);
 using Microsoft::WRL::ComPtr;
 // This factory is a short-lived discovery snapshot, never the shared presentation factory.
 ComPtr<IDXGIFactory6> factory;
 if(FAILED(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)))){auto c=resolveOutput(preference,false,false,0,0);c.reason=L"Display information unavailable";return c;}
 ComPtr<IDXGIAdapter1> adapter;bool active=true,matched=false,missingPeak=false;float peak=10000;
 if(SUCCEEDED(factory->EnumAdapterByLuid(monitor.adapter,IID_PPV_ARGS(&adapter))))for(UINT n=0;;n++){
  ComPtr<IDXGIOutput> output;if(adapter->EnumOutputs(n,&output)==DXGI_ERROR_NOT_FOUND)break;if(!output)break;
  ComPtr<IDXGIOutput6> modern;DXGI_OUTPUT_DESC1 desc{};
  if(SUCCEEDED(output.As(&modern))&&SUCCEEDED(modern->GetDesc1(&desc))&&desc.Monitor==monitor.handle){
   matched=true;missingPeak|=!usableLuminance(desc.MaxLuminance);active&=activeHdrSignal(desc.ColorSpace);peak=std::min(peak,usableLuminance(desc.MaxLuminance)?desc.MaxLuminance:400.f);
  }
 }
 active&=matched;
 MONITORINFOEXW info{};info.cbSize=sizeof(info);bool peakFallback=missingPeak||peak==10000,missingState=false;
 std::vector<OutputTarget> targets;
 if(GetMonitorInfoW(monitor.handle,&info)){
  for(const auto& p:activeDisplayPaths()){
   DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};source.header={DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME,sizeof(source),p.sourceInfo.adapterId,p.sourceInfo.id};
   if(DisplayConfigGetDeviceInfo(&source.header)!=ERROR_SUCCESS||wcscmp(source.viewGdiDeviceName,info.szDevice))continue;
   bool targetActive=active;
   // Advanced Color alone also includes SDR ACM. Require the target's HDR (PQ) signal
   // through DXGI above and reject any cloned target with Advanced Color disabled.
#if (NTDDI_VERSION >= NTDDI_WIN11_GA)
   DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO_2 modernColor{};modernColor.header={DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO_2,sizeof(modernColor),p.targetInfo.adapterId,p.targetInfo.id};
   if(DisplayConfigGetDeviceInfo(&modernColor.header)==ERROR_SUCCESS){targetActive&=modernColor.advancedColorActive&&modernColor.activeColorMode==DISPLAYCONFIG_ADVANCED_COLOR_MODE_HDR;}
   else
#endif
   {
   DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO color{};color.header={DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO,sizeof(color),p.targetInfo.adapterId,p.targetInfo.id};
   if(DisplayConfigGetDeviceInfo(&color.header)!=ERROR_SUCCESS){targetActive=false;missingState=true;}else if(!color.advancedColorEnabled)targetActive=false;
   }
   DISPLAYCONFIG_SDR_WHITE_LEVEL level{};level.header={DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL,sizeof(level),p.targetInfo.adapterId,p.targetInfo.id};
   float w=DisplayConfigGetDeviceInfo(&level.header)==ERROR_SUCCESS?sdrWhiteNits(level.SDRWhiteLevel):0;
   targets.push_back({targetActive,w,matched?peak:0});
  }
 }
 auto result=resolveTargets(preference,false,targets);
 result.peakFallback|=peakFallback;if(!matched||missingState)result.reason=L"HDR display information unavailable";return result;
}
}
