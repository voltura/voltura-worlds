#pragma once
#include "shell_menu.h"
#include <thread>
#include <memory>
#include <optional>
namespace worlds::shellmenu {
class Server {
 struct Impl;std::unique_ptr<Impl> impl;
public:
 Server();~Server();
 bool start(HWND controller);
 void publish(const State& state);
 std::optional<Request> takeRequest();
 void stop();
};
// The client verifies that the pipe belongs to the installed Worlds executable.
bool exchange(const Request&,Response&,const std::filesystem::path& executable);
}
