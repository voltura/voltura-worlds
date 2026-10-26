#pragma once
#include "package.h"
namespace worlds {
struct PresetUiContext {
 std::filesystem::path profile;
 std::function<DesktopPreset()> snapshot;
 std::function<std::vector<Monitor>()> monitors;
 std::function<void(const std::filesystem::path&)> importFile;
 // -1 skips an entry. Called on the UI thread after all mapped media passes preflight.
 std::function<void(const DesktopPreset&,const std::vector<int>&,const std::vector<Monitor>&)> apply;
};
void showSavedPresets(HWND owner,PresetUiContext,const std::filesystem::path& importFile={});
}
