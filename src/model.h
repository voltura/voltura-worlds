#pragma once
#include <windows.h>
#include <array>
#include <string>
#include <filesystem>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
namespace worlds {
enum class Scene { Aurora, Orbit, Smoke, Video, Bloom, Off, Silk, Eclipse, Lattice, Drift, Contour, Mercury };
enum class Quality { Eco, Auto, High };
enum class Reaction { Off, Gentle, Expressive };
struct Parameters {
    float brightness=1, movement=1, detail=1, flow=1, turbulence=1, density=1, strength=.5f;
    int palette=0;
};
struct Assignment {
    Scene scene=Scene::Aurora;
    Parameters params;
    Reaction reaction=Reaction::Off;
    std::wstring video;
    bool fit=false;
};
struct Settings {
    Quality quality=Quality::Auto;
    int fps=60;
    bool pauseBattery=true;
    std::wstring audioDevice;
    std::vector<std::pair<std::wstring,Assignment>> displays;
};
struct Monitor { HMONITOR handle{}; RECT rect{}; std::wstring id,name; };
struct AudioFeatures { float bass=0,mid=0,treble=0,energy=0,transient=0; };
struct PauseState { bool manual=false,locked=false,asleep=false,suspended=false,remote=false,battery=false,covered=false; };
bool shouldPause(const PauseState&,bool pauseBattery);
bool valid(const Parameters&);
float boundedDelta(float seconds);
std::filesystem::path profilePath();
Settings loadSettings(const std::filesystem::path&);
void saveSettings(const std::filesystem::path&,const Settings&);
Assignment assignmentFor(const Settings&,const std::wstring&);
std::string utf8(const std::wstring&);
std::wstring wide(const std::string&);
std::string sceneName(Scene);
Scene parseScene(const std::string&);
bool isProceduralWorld(Scene);
}
