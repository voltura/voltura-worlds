#pragma once
#include <d3d12.h>
#include <filesystem>
#include <memory>
#include <vector>
#include <stop_token>
namespace worlds {
struct VideoInfo {unsigned width{},height{};double duration{};};
VideoInfo inspectVideo(const std::filesystem::path&);
std::vector<unsigned char> videoPreview(const std::filesystem::path&);
std::filesystem::path copyVideoToLibrary(const std::filesystem::path&,const std::filesystem::path&,std::stop_token = {});
class VideoPlayer {
public:
 VideoPlayer(ID3D12Device*,ID3D12CommandQueue*,const std::filesystem::path&);
 ~VideoPlayer();
 void setPaused(bool);
 bool update();
 ID3D12Resource* frame()const;
 void consumed(); // After submitting every D3D12 use of frame().
 double position()const;
 VideoInfo info()const;
private:
 struct Impl;std::unique_ptr<Impl> impl_;
};
}
