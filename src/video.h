#pragma once
#include <d3d12.h>
#include "video_settings.h"
#include <filesystem>
#include <memory>
#include <vector>
#include <string>
#include <stop_token>
#include <stdexcept>
namespace worlds {
enum class VideoErrorKind {Unavailable,Codec,Container,Damaged,Hdr,Geometry,Timeout,Cancelled,Copy};
struct VideoError:std::runtime_error {VideoErrorKind kind;VideoError(VideoErrorKind k,const char* message):std::runtime_error(message),kind(k){}};
struct VideoRateRejected:std::runtime_error {using std::runtime_error::runtime_error;};
struct VideoInfo {
 unsigned width{},height{},encodedWidth{},encodedHeight{},rotation{},pixelAspectX=1,pixelAspectY=1;
 double duration{},displayAspect=0;
 std::string container,codec;
};
VideoInfo inspectVideo(const std::filesystem::path&);
struct VideoPreflight {VideoInfo info;std::vector<unsigned char> preview;};
VideoPreflight preflightVideo(const std::filesystem::path&,std::stop_token={},LUID adapter={},unsigned timeoutMs=15000,double speed=1);
std::vector<unsigned char> videoPreview(const std::filesystem::path&,std::stop_token={},LUID adapter={});
std::filesystem::path copyVideoToLibrary(const std::filesystem::path&,const std::filesystem::path&,std::stop_token={},LUID adapter={});
class VideoPlayer {
public:
 VideoPlayer(ID3D12Device*,ID3D12CommandQueue*,const std::filesystem::path&);
 ~VideoPlayer();
 void setPaused(bool);
 bool setPlaybackRate(double); // False until metadata is ready; throws on rejection.
 double playbackRate()const;
 bool update();
 ID3D12Resource* frame()const;
 void consumed();
 double position()const;
 VideoInfo info()const;
private:
 struct Impl;std::unique_ptr<Impl> impl_;
};
}
