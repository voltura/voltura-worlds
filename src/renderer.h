#pragma once
#include "model.h"
#include "output.h"
#include "gpu_context.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <memory>
#include <vector>
namespace worlds {
class VideoPlayer;
using Microsoft::WRL::ComPtr;
void check(HRESULT hr,const char* operation);
struct FrameData {
 float width=1920,height=1080,time=0,dt=.016f;
 float bass=0,mid=0,treble=0,energy=0;
 float brightness=1,movement=1,detail=1,flow=1;
 float turbulence=1,density=1,reaction=0,palette=0;
 float grid=48,pass=0,impulse=0,seed=0;
 float spare[12]{};
};
class Renderer {
 friend struct HdrTestHarness;
public:
 Renderer(HWND,unsigned,unsigned,const std::filesystem::path&,bool debug=false,bool forceSingleSample=false,std::shared_ptr<GpuContext> context={});
 ~Renderer();
 bool configureOutput(const OutputConfig&,bool retry=false);
 const OutputConfig& output()const{return output_;}
 ID3D12PipelineState* outputPipelineIdentity()const{return output_.hdr?postHdr_.Get():post_.Get();}
 void setNonblocking(bool enabled){nonblocking_=enabled;}
 bool ready();
 bool deviceRemoved()const{return context_->removed();}
 std::shared_ptr<GpuContext> context()const{return context_;}
 uint64_t mutableAllocations()const{return allocations()-assetBytes_;}
 ID3D12Resource* textureIdentity(unsigned slot)const;
 ID3D12Resource* simulationIdentity()const{return velocity_[0].resource.Get();}
 ID3D12Resource* targetIdentity()const{return hdr_.Get();}
 ID3D12PipelineState* pipelineIdentity()const{return earth_.Get();}
 void render(const Assignment&,FrameData,bool present=true);
 void resize(unsigned,unsigned);
 void setOverlayInsets(std::array<unsigned,4> insets){overlayInsets_=insets;}
 void capture(const std::filesystem::path&);
 void captureFluidFields(const std::filesystem::path&);
 void idle();
 void setPaused(bool);
 void setPressureMultigrid(bool enabled){if(velocity_[0].resource||(enabled&&(grid_>128||(grid_&(grid_-1)))))throw std::runtime_error("Multigrid capture requires power-of-two grid32 to128 before initialization");pressureMultigrid_=enabled;}
 void setSmokeFilaments(bool enabled){smokeFilamentsEnabled_=enabled;}
 double videoPosition()const;
 bool hasVideoFrame()const{return videoView_!=nullptr;}
 bool videoRateKnown()const{return videoRateKnown_;}
 double videoPlaybackRate()const{return acceptedVideoRate_;}
 const std::wstring& videoRateMessage()const{return videoRateMessage_;}
 void validateDebugMessages();
 std::wstring adapterName()const{return adapterName_;}
 double gpuMs()const{return gpuMs_;}
 void setStageProfiling(bool enabled){stageProfiling_=enabled;}
 const std::array<double,14>& gpuStageMs()const{return gpuStageMs_;}
 uint64_t textureAllocations()const{return assetBytes_;}
 uint64_t allocations()const{return bodyStatisticsBytes_+uint64_t(width_)*height_*(output_.hdr?24:16)+depthTargetBytes_+uint64_t((width_+3)/4)*((height_+3)/4)*16+assetBytes_+smokeTracerBytes_+pressureHierarchyBytes_+fineAtlasBytes_+orbitTargetBytes_+videoBytes_+(velocity_[0].resource?fluidBytes_:0);}
 unsigned gridSize()const{return grid_;}
 unsigned densityGridSize()const{return referenceDensityGrid_?referenceDensityGrid_:(grid_>96?192:grid_*2);}
 std::array<unsigned,3> densityDimensions()const{return wideDensity_?std::array<unsigned,3>{grid_*6,((grid_*27/8+3)/4)*4,grid_}:std::array<unsigned,3>{densityGridSize(),densityGridSize(),densityGridSize()};}
 void setWideDensity(bool value){if(velocity_[0].resource)throw std::runtime_error("Set density layout before scene initialization");if(value&&grid_>96)throw std::runtime_error("Wide density experiments support grids up to 96");wideDensity_=value;}
 unsigned sceneSamples()const{return (lastScene_==Scene::Orbit||lastScene_==Scene::Bloom)?orbitSamples_:1;}
 void setGrid(unsigned);
 void setDetailCoordinates(bool value){if(velocity_[0].resource)throw std::runtime_error("Set detail coordinates before initialization");detailCoordinates_=value;}
 void setFineAtlas(bool value){if(velocity_[0].resource)throw std::runtime_error("Set fine atlas before initialization");fineAtlasEnabled_=value;}
 void setFineAtlasFront(bool value){if(velocity_[0].resource||!fineAtlasEnabled_)throw std::runtime_error("Front atlas requires fine atlas before initialization");fineAtlasFront_=value;}
 void setFineCurl(float value){if(velocity_[0].resource||!fineAtlasEnabled_||!(value>=0&&value<=.3f))throw std::runtime_error("Fine curl requires fine atlas and strength from 0 to 0.3");fineCurlStrength_=value;}
 void setFineLighting(bool value){if(velocity_[0].resource||!fineAtlasEnabled_)throw std::runtime_error("Fine lighting requires fine atlas before initialization");fineLightingEnabled_=value;}
 void setReferenceVolume(const std::filesystem::path& path){if(velocity_[0].resource)throw std::runtime_error("Set reference volume before initialization");referenceVolume_=path;}
 void setReferenceDensityGrid(unsigned value){
  if(referenceVolume_.empty()||velocity_[0].resource||wideDensity_||value<24||value>384||value%4)
   throw std::runtime_error("Reference density grid requires a frozen cubic volume, before initialization, and a multiple of four from 24 to 384");
  referenceDensityGrid_=value;
 }
 void setFlowmapLive(bool value){flowmapLive_=value;setDetailCoordinates(value);}
 void setFlowmapAdaptive(bool value){flowmapAdaptive_=value;}
 unsigned flowmapAdaptiveRenewals()const{return flowmapAdaptiveRenewals_;}
 void setFlowmapRenewal(float seconds){flowmapRenewal_=seconds;}
 unsigned flowmapRenewals()const{return flowmapRenewals_;}
private:
 void prepareBodyStatistics(const Assignment&,int palette);
 void drawBodyStatistics(const Assignment&);
 ComPtr<ID3D12Resource> bodyStatisticsTexture_;
 ComPtr<ID3D12PipelineState> bodyStatisticsPipeline_;
 Scene bodyStatisticsScene_=Scene::Off;
 int bodyStatisticsPalette_=-1;
 unsigned bodyStatisticsWidth_=0,bodyStatisticsHeight_=0,bodyStatisticsOutputWidth_=0,bodyStatisticsOutputHeight_=0;
 uint64_t bodyStatisticsBytes_=0;
 std::array<unsigned,4> overlayInsets_{},bodyStatisticsInsets_{};
 float bodyStatisticsX_=0,bodyStatisticsY_=0;
 bool pressureMultigrid_=false;
 struct PressureLevel{unsigned n,base,side=0;unsigned cells()const{return n*n*n;}unsigned offset()const{return base+side*cells();}};
 std::vector<PressureLevel> pressureLevels_;
 ComPtr<ID3D12Resource> pressureHierarchy_;
 ComPtr<ID3D12RootSignature> pressureRoot_;
 ComPtr<ID3D12PipelineState> pressureCyclePipeline_,pressureBridgePipeline_;
 uint64_t pressureHierarchyBytes_=0;
 void makePressureHierarchy();void solvePressureHierarchy();
 void releasePressureHierarchy(){pressureHierarchy_.Reset();pressureLevels_.clear();pressureHierarchyBytes_=0;}
 bool smokeFilamentsEnabled_=false;
 ComPtr<ID3D12PipelineState> smokeFilamentPipeline_,smokeTracerPipeline_;
 ComPtr<ID3D12Resource> smokeTracers_;bool smokeTracersReady_=false;uint64_t smokeTracerBytes_=0;
 void makeSmokeFilaments();void drawSmokeFilaments(FrameData);
 bool fineAtlasEnabled_=false,fineAtlasReady_=false,fineAtlasFront_=false;
 unsigned fineAtlasIndex_=0;
 float fineCurlStrength_=0;
 bool fineLightingEnabled_=false;
 uint64_t fineAtlasBytes_=0;
 std::array<ComPtr<ID3D12Resource>,2> fineAtlases_;
 ComPtr<ID3D12Resource> fineCoarseHistory_,finePages_,fineDestinations_,fineForward_;
 ComPtr<ID3D12RootSignature> fineRoot_;
 ComPtr<ID3D12PipelineState> finePipeline_,fineLightPipeline_;
 void makeFineAtlas();void stepFineAtlas(const FrameData&);void releaseFineAtlas();
 bool flowmapLive_=false,flowmapAdaptive_=false;
 ComPtr<ID3D12Resource> flowmapAudit_,flowmapAuditZero_;
 std::array<ComPtr<ID3D12Resource>,2> flowmapAuditReadback_;
 std::array<bool,2> flowmapAuditReady_{};
 std::array<unsigned,2> flowmapAuditEpoch_{};
 unsigned flowmapAdaptiveRenewals_=0;
 void releaseFlowmapAudit();
 float flowmapRenewal_=0,flowmapAge_=0;
 unsigned flowmapRenewals_=0;
 std::filesystem::path referenceVolume_;
 unsigned referenceDensityGrid_{};
 ComPtr<ID3D12Resource> referenceUpload_;
 D3D12_PLACED_SUBRESOURCE_FOOTPRINT referenceFootprint_{};
 struct Volume {ComPtr<ID3D12Resource> resource;};
 bool wideDensity_=false,detailCoordinates_=false;
 unsigned coordinateIndex_=0;
 std::array<Volume,2> coordinates_;
 ComPtr<ID3D12PipelineState> coordinatePipeline_,detailPipeline_;
 Volume detailVelocity_,flowmapBase_,flowmapTotal_;
 void combineFlowmap(FrameData&);
 void buildDetailField(FrameData&);
 void advectCoordinates(FrameData&,bool);
 struct Frame {ComPtr<ID3D12CommandAllocator> allocator;uint64_t fence=0;};
 std::shared_ptr<GpuContext> context_;
 std::map<ID3D12Resource*,std::shared_ptr<SharedTexture>> textureLeases_;
 std::map<unsigned,ID3D12Resource*> assetSlots_;
 bool cachedTexture(const std::wstring&,ComPtr<ID3D12Resource>&,unsigned);
 void retainTexture(const std::wstring&,const ComPtr<ID3D12Resource>&,unsigned);
 void releaseAsset(ComPtr<ID3D12Resource>&);
 OutputConfig output_{},requestedOutput_{};bool outputConfigured_=false;
 ComPtr<ID3D12PipelineState> postHdr_,bodyStatisticsHdr_;
 void outputConstants();
 bool nonblocking_=false,pendingPresent_=false;HANDLE latency_=nullptr;
 bool tryPresent();
 HWND window_{};unsigned width_=0,height_=0,frameIndex_=0,rtvStride_=0,descriptorStride_=0,grid_=48;
 std::wstring adapterName_;
 ComPtr<IDXGIFactory6> factory_;ComPtr<ID3D12Device> device_;ComPtr<ID3D12CommandQueue> queue_;
 ComPtr<IDXGISwapChain3> swap_;ComPtr<ID3D12Fence> fence_;HANDLE event_=nullptr;uint64_t fenceValue_=0;
 ComPtr<ID3D12GraphicsCommandList> list_;ComPtr<ID3D12DescriptorHeap> rtv_,dsv_,views_;
 std::array<Frame,2> frames_;std::array<ComPtr<ID3D12Resource>,2> buffers_;
 ComPtr<ID3D12Resource> depth_,hdr_,planetTexture_,cloudTexture_,nightTexture_,starTexture_,auroraArtwork_;ComPtr<ID3D12RootSignature> root_;
 ComPtr<ID3D12Resource> saturnColor_;
 ComPtr<ID3D12PipelineState> saturn_;
 ComPtr<ID3D12PipelineState> neptune_,neptuneSpace_,venus_,venusSpace_,uranus_,uranusSpace_;
 ComPtr<ID3D12Resource> jupiterColor_,jupiterAtmosphere_;
 ComPtr<ID3D12PipelineState> jupiter_,jupiterSpace_;
 ComPtr<ID3D12Resource> moonColor_,moonNormal_,moonEarth_;
 ComPtr<ID3D12PipelineState> moon_,moonSpace_;
 ComPtr<ID3D12Resource> sunImage_,sunProminence_,plutoColor_;
 ComPtr<ID3D12PipelineState> sun_;
 ComPtr<ID3D12PipelineState> pluto_,plutoSpace_;
 ComPtr<ID3D12Resource> mercuryColor_,mercuryNormal_;
 ComPtr<ID3D12PipelineState> mercuryPlanet_,mercurySpace_;
 ComPtr<ID3D12Resource> marsColor_,marsNormal_;
 ComPtr<ID3D12Resource> earthColor_,earthClouds_,earthMaterial_;
 ComPtr<ID3D12PipelineState> earth_,earthSpace_;
 ComPtr<ID3D12PipelineState> mars_,marsSpace_;
 ComPtr<ID3D12Resource> orbitColor_,orbitDepth_;unsigned orbitSamples_=1;uint64_t orbitTargetBytes_=0,depthTargetBytes_=0;
 std::array<ComPtr<ID3D12Resource>,2> bloomTargets_,auroraTextures_;
 std::filesystem::path assetDirectory_;uint64_t assetBytes_=0;bool starForeground_=false;
 ComPtr<ID3D12PipelineState> bloomSurface_,bloomBackdrop_,aurora_,space_,planet_,atmosphere_,smoke_,fluid_,fluidMultigrid_,post_,bloom_,videoPipeline_;
 std::array<ComPtr<ID3D12PipelineState>,7> proceduralWorlds_;
 bool videoRateKnown_=false;double requestedVideoRate_=-1,acceptedVideoRate_=1;std::wstring videoRateMessage_;
 std::unique_ptr<VideoPlayer> video_;std::filesystem::path videoPath_;ID3D12Resource* videoView_{};uint64_t videoBytes_{};
 void renderVideo(const Assignment&,FrameData,bool);
 std::array<Volume,2> velocity_,density_,pressure_;Volume divergence_,curl_;
 unsigned v_=0,d_=0,p_=0;bool fluidReady_=false;Scene lastScene_=Scene::Aurora;
 ComPtr<ID3D12QueryHeap> query_;ComPtr<ID3D12Resource> queryReadback_;UINT64 frequency_=0;
 double gpuMs_=0;uint64_t allocationBytes_=0,fluidBytes_=0;
 void targets();void makeDepthTarget();void makeOrbitTargets();void makeFluid();void loadAurora();void loadPlanet();void loadJupiter();void loadSaturn();void loadMoon();void loadMars();void loadMercury();void loadSun();void loadPluto();void loadEarth();void loadStars(bool foreground=false);void loadDds(const std::filesystem::path&,ComPtr<ID3D12Resource>&,unsigned);void loadTexture(const std::filesystem::path&,ComPtr<ID3D12Resource>&,unsigned,unsigned,bool grayscale=false);void simulate(FrameData&);void wait(uint64_t);void submit();
 static constexpr unsigned QueryStride=66;
 bool stageProfiling_=false;
 std::array<unsigned,2> stageCount_{};
 std::array<std::array<unsigned,32>,2> stageIds_{};
 std::array<double,14> gpuStageMs_{};
 unsigned beginGpuStage(unsigned);void endGpuStage(unsigned);
 void begin();void transition(ID3D12Resource*,D3D12_RESOURCE_STATES,D3D12_RESOURCE_STATES);
 D3D12_CPU_DESCRIPTOR_HANDLE cpu(unsigned)const;D3D12_GPU_DESCRIPTOR_HANDLE gpu(unsigned)const;
};
}
