#pragma once
#include "model.h"
namespace worlds {
struct WorldPackage {std::string title,description,attribution;Assignment assignment;std::vector<unsigned char> preview,video;};
WorldPackage readPackage(const std::filesystem::path&);
void writePackage(const std::filesystem::path&,const WorldPackage&);
Assignment installPackage(const std::filesystem::path& library,const WorldPackage&);
}
