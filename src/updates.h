// Adapted from Voltura Markdown, MIT, Copyright (c) 2026 Voltura AB.
#pragma once
#include <atomic>
#include <filesystem>
#include <memory>
#include <windows.h>

namespace wu {
// Owns no reader objects or HWNDs. Destruction cancels without joining network work.
class Updates {
    struct State;
    std::shared_ptr<State> state;
#ifdef WORLDS_UPDATE_TESTING
    friend void testUpdates();
#endif
  public:
    void check();
    void cancel();
    void sessionEnd();
    void close(bool install, bool restart = false);
    bool ready() const;
    ~Updates();
#ifdef WORLDS_UPDATE_TESTING
    void fixture(const std::filesystem::path &directory);
#endif
};
bool enabled();
void setEnabled(bool value);
// Invoked before graphics/preferences initialization. Returns -1 for a normal reader launch.
int updateHelper(int argc, wchar_t **argv);
// Cooperates with an active installation only; ordinary launches do no disk/network work.
bool updateReaderMayStart();
#ifdef WORLDS_UPDATE_TESTING
void testUpdates();
#endif
} // namespace wu
