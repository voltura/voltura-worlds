#pragma once
#include "model.h"
#include <thread>
namespace worlds {
class AudioCapture {
public:
 ~AudioCapture();
 void configure(bool enabled,const std::wstring& device);
 AudioFeatures features()const;
 static std::vector<std::pair<std::wstring,std::wstring>> devices();
private:
 std::jthread thread_;mutable std::mutex mutex_;AudioFeatures latest_{};std::wstring device_;bool enabled_=false;
 void run(std::stop_token,std::wstring);
};
}
