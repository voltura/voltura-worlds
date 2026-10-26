#pragma once
#include "model.h"
#include <functional>
#include <stop_token>
namespace worlds {
struct WorldPackage {std::string title,description,attribution;Assignment assignment;std::vector<unsigned char> preview,video;};
void validateWorldPreview(const std::vector<unsigned char>&);
WorldPackage readPackage(const std::filesystem::path&);
void writePackage(const std::filesystem::path&,const WorldPackage&);
Assignment installPackage(const std::filesystem::path& library,const WorldPackage&,std::filesystem::path& installedDirectory);
// Version 2 archives stream media from files (or byte ranges in another archive).
struct MediaReference {std::string name;std::filesystem::path source;uint64_t offset=0,size=0;};
struct PresetDisplay {std::wstring id,label;Assignment assignment;std::string media;};
struct DesktopPreset {
 std::string title,description,attribution;
 bool pauseBattery=true;std::wstring audioDevice;
 std::vector<PresetDisplay> displays;std::vector<MediaReference> media;
};
using PresetProgress=std::function<void(const std::wstring&)>;
DesktopPreset snapshotPreset(const Settings&,const std::vector<Monitor>&);
void validatePresetName(const std::string&);
bool samePresetName(const std::string&,const std::string&);
bool isDesktopPackage(const std::filesystem::path&);
DesktopPreset readDesktopPackage(const std::filesystem::path&,std::stop_token={});
void writeDesktopPackage(const std::filesystem::path&,const DesktopPreset&,std::stop_token={},PresetProgress={});
// Materialize portable videos in a new managed directory, leaving source archives intact.
DesktopPreset installDesktopMedia(const std::filesystem::path&,const DesktopPreset&,std::stop_token={},PresetProgress={});
struct SavedPreset {std::filesystem::path file;DesktopPreset preset;std::string error;};
std::vector<SavedPreset> listPresets(const std::filesystem::path&,std::stop_token={});
std::filesystem::path savePreset(const std::filesystem::path&,const DesktopPreset&,const std::filesystem::path& replace={},std::stop_token={},PresetProgress={});
std::string newPresetId();
}
