#pragma once
#include "package.h"
#include <functional>
namespace worlds {
void showWorldImport(HWND owner,const std::filesystem::path& file,const std::filesystem::path& library,
 const std::wstring& destination,std::function<void(const Assignment&)> apply,bool personalVideo=false);
}
