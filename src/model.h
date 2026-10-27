#pragma once
#include <windows.h>
#include "video_settings.h"
#include <array>
#include <string>
#include <filesystem>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <exception>
namespace worlds {
enum class Scene { Aurora, Orbit, Smoke, Video, Bloom, Off, Silk, Eclipse, Lattice, Drift, Contour, PlasticMetal, Ripples, Mars, Earth, Moon, Jupiter, Saturn, Mercury, Venus, Uranus, Neptune, Pluto, Sun, ForestWater, TropicalParadise, TropicalIsland };
enum class OutputPreference { Auto, Sdr };
enum class Quality { Eco, Auto, High };
enum class FrameLimit { Auto, Fps30, Fps60 };
struct DisplayPerformance {
    Quality quality=Quality::Auto;
    FrameLimit frameLimit=FrameLimit::Auto;
    bool operator==(const DisplayPerformance&)const=default;
};
enum class Reaction { Off, Gentle, Expressive };
enum class RandomPool { All, SolarSystem };
struct Parameters {
    float brightness=1, movement=1, detail=1, flow=1, turbulence=1, density=1, strength=.5f;
    int palette=0;
};
struct Assignment {
    DisplayPerformance performance;
    OutputPreference output=OutputPreference::Auto;
    Scene scene=Scene::Aurora;
    Parameters params;
    Reaction reaction=Reaction::Off;
    std::wstring video;
    bool fit=false;
    VideoSettings videoSettings;
    bool random=false;
    int randomInterval=300;
    RandomPool randomPool=RandomPool::All;
    bool showBodyStatistics=false;
};
struct Settings {
    Quality quality=Quality::Auto;
    int fps=60;
    bool pauseBattery=true;
    std::wstring audioDevice;
    std::vector<std::pair<std::wstring,Assignment>> displays;
};
struct Monitor { HMONITOR handle{}; RECT rect{}; std::wstring id,name; double refreshHz=60; LUID adapter{}; };
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
std::wstring describeError(const std::exception&);
std::string sceneName(Scene);
Scene parseScene(const std::string&);
bool isProceduralWorld(Scene);
}
