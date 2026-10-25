// Compile the implementation into this isolated test executable to exercise internal policy
// without exporting production-only test interfaces or permitting arbitrary update feeds.
#include "../src/updates.cpp"
#include <iostream>

namespace wu {
namespace {
fs::path signedFixture;
bool handoff = false, lateShutdown = false;
int checks = 0;
void expect(bool ok, const char *name) {
    need(ok, name);
    ++checks;
}
template <class F> void rejects(F action, const char *name) {
    bool rejected = false;
    try {
        action();
    } catch (...) {
        rejected = true;
    }
    expect(rejected, name);
}
std::string metadata(std::string version = "1.3.1", bool setup = false) {
    return "{\"tag_name\":\"v" + version +
           "\",\"draft\":false,\"prerelease\":false,\"assets\":[{\"name\":\"" +
           (setup ? "VolturaWorlds-Setup-" + version + "-win-x64.exe" : "VolturaWorlds.exe") +
           "\"},{\"name\":\"SHA256SUMS.txt\"}]}";
}
void cleanup(const fs::path &root) {
    if (!fs::exists(root) || !ordinary(root))
        return;
    std::error_code ec;
    for (auto &item : fs::directory_iterator(root)) {
        if (!ordinary(item.path()))
            continue;
        if (item.is_directory()) {
            for (auto name : {L"payload.exe", L"helper.exe", L"download.part"})
                fs::remove(item.path() / name, ec);
            fs::remove(item.path(), ec);
        } else
            fs::remove(item.path(), ec);
    }
    fs::remove(root, ec);
}
} // namespace
void testUpdates() {
    if (handoff) {
        Updates updates;
        updates.state = std::make_shared<Updates::State>();
        auto work = updates.state;
        std::atomic_bool cancel = false;
        auto digest = hashFile(signedFixture, cancel);
        auto version = fileVersion(signedFixture).text();
        work->text = [&](const std::wstring &url, const std::atomic_bool &) {
            return url.ends_with(L"SHA256SUMS.txt") ? digest + "  VolturaWorlds.exe\n" : metadata(version);
        };
        work->file = [&](const std::wstring &, const fs::path &out, const std::atomic_bool &) {
            need(CopyFileW(signedFixture.c_str(), out.c_str(), TRUE) != FALSE);
        };
        updates.check();
        auto deadline = GetTickCount64() + 10000;
        while (work->busy && GetTickCount64() < deadline)
            Sleep(5);
        need(!work->busy && work->ready.has_value(), "Handoff fixture did not stage");
        std::wcout << cacheRoot(executable()).wstring() << '\n';
        updates.close(true);
        if (lateShutdown)
            updates.sessionEnd();
        return;
    }
    expect(Version::parse("1.10.0") > Version::parse("1.9.99"), "numeric version comparison");
    for (auto value : {"", "1.2", "1.2.3.4", "1.2.3-preview.1", "01.2.3", "1.-2.3", "65536.0.0", "1.2.3junk"})
        rejects([&] { Version::parse(value); }, "invalid or preview version rejected");
    expect(releaseInfo(metadata(), false).asset == "VolturaWorlds.exe", "portable asset selection");
    expect(releaseInfo(metadata("1.3.1", true), true).asset == "VolturaWorlds-Setup-1.3.1-win-x64.exe",
           "installer asset selection");
    auto preview = metadata();
    preview.replace(preview.find("prerelease\":false"), 17, "prerelease\":true");
    rejects([&] { releaseInfo(preview, false); }, "preview rejected");
    auto draft = metadata();
    draft.replace(draft.find("draft\":false"), 12, "draft\":true");
    rejects([&] { releaseInfo(draft, false); }, "draft rejected");
    rejects([&] { releaseInfo("{\"draft\":false,\"draft\":true}", false); }, "duplicate JSON keys rejected");
    rejects([&] { JsonReader{"{\"x\":true} trailing"}.document(); }, "trailing JSON rejected");
    rejects([&] { JsonReader{"{\"x\":\"\\q\"}"}.document(); }, "malformed JSON escape rejected");
    rejects([&] { releaseInfo(metadata(), true); }, "missing installer rejected");
    auto digest = std::string(64, 'a');
    expect(checksum(digest + "  VolturaWorlds.exe\r\n", "VolturaWorlds.exe") == digest, "CRLF checksums");
    rejects([&] { checksum(digest + "  other.exe\n", "VolturaWorlds.exe"); }, "wrong asset checksum rejected");
    rejects([&] { checksum(digest + "  VolturaWorlds.exe\n" + digest + "  VolturaWorlds.exe\n", "VolturaWorlds.exe"); },
            "duplicate checksum rejected");
    rejects([&] { checksum(std::string(64, 'g') + "  VolturaWorlds.exe\n", "VolturaWorlds.exe"); }, "nonhex checksum rejected");
    expect(identity(L"C:\\Test Folder\\VolturaWorlds.exe") == identity(L"c:/test folder/VolturaWorlds.exe"),
           "path namespace normalized");
    Session session;
    SendMessageW(session.hwnd, WM_QUERYENDSESSION, 0, ENDSESSION_LOGOFF);
    expect(session.ending && WaitForSingleObject(session.cancellation, 0) == WAIT_OBJECT_0,
           "session query cancels helper");
    SendMessageW(session.hwnd, WM_ENDSESSION, FALSE, ENDSESSION_LOGOFF);
    expect(!session.pump(), "cancelled shutdown still defers installation");

    auto temp =
        fs::temp_directory_path() / (L"Voltura update test å " + std::to_wstring(GetCurrentProcessId()));
    fs::create_directory(temp);
    auto target = temp / L"Renamed reader.exe";
    auto root = cacheRoot(target);
    cleanup(root);
    fs::create_directories(root);
    try {
        expect(!cached(root, target), "missing staged update ignored");
        writeAtomic(root / L"pending", "1.3.1\n" + digest + "\n../outside\nportable\n");
        expect(!cached(root, target), "cache traversal rejected");
        writeAtomic(root / L"pending", "wanted 1.4.0\n");
        expect(!cached(root, target) && wanted(root, target) == Version::parse("1.4.0"),
               "newer incomplete update blocks old payload");
        Candidate next{Version::parse("1.3.1"), digest, root, root / L"123", target, false};
        fs::create_directory(next.directory);
        writeAtomic(next.payload(), "new");
        writeAtomic(target, "old");
        writeAtomic(root / L"pending", next.marker());
        expect(cached(root, target)->marker() == next.marker(), "atomic ready marker roundtrip");
        std::atomic_bool cancel = false;
        auto before = hashFile(target, cancel);
        {
            Handle locked(CreateFileW(target.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL, nullptr));
            expect(!applyPortable(next) && hashFile(target, cancel) == before,
                   "portable lock failure preserves original");
        }
        expect(applyPortable(next) && read(target) == "new", "portable replacement preserves renamed target");
        expect(!fs::exists(fs::path(target.wstring() + L".update-old")),
               "portable backup cleaned on success");
        cancel = true;
        rejects([&] { hashFile(target, cancel); }, "hashing cancellation");
        rejects([&] { signer(target); }, "unsigned or non-PE update rejected");
        rejects([&] { verify(next, digest, cancel); }, "cancelled candidate rejected");
        fs::remove(target);
        cleanup(root);

        // Isolated installed-path detection: never alter the user's real Installed apps entry.
        HKEY hive = nullptr;
        auto hiveName = L"Software\\VolturaUpdateTests-" + std::to_wstring(GetCurrentProcessId());
        need(RegCreateKeyExW(HKEY_CURRENT_USER, hiveName.c_str(), 0, nullptr, 0, KEY_ALL_ACCESS, nullptr,
                             &hive, nullptr) == ERROR_SUCCESS);
        need(RegOverridePredefKey(HKEY_CURRENT_USER, hive) == ERROR_SUCCESS);
        HKEY uninstall = nullptr;
        need(RegCreateKeyExW(hive,
                             L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\VolturaWorlds", 0,
                             nullptr, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, nullptr, &uninstall,
                             nullptr) == ERROR_SUCCESS);
        auto path = temp.wstring();
        RegSetValueExW(uninstall, L"InstallLocation", 0, REG_SZ, (BYTE *)path.c_str(),
                       (DWORD)((path.size() + 1) * sizeof(wchar_t)));
        bool matching = installed(temp / L"VolturaWorlds.exe"), renamed = installed(target);
        RegCloseKey(uninstall);
        RegOverridePredefKey(HKEY_CURRENT_USER, nullptr);
        RegCloseKey(hive);
        RegDeleteTreeW(HKEY_CURRENT_USER, hiveName.c_str());
        expect(matching && !renamed, "only registered executable is installed");

        if (!signedFixture.empty()) {
            need(CopyFileW(executable().c_str(), target.c_str(), TRUE) != FALSE);
            auto pin = signer(target);
            cancel = false;
            auto fixtureHash = hashFile(signedFixture, cancel);
            expect(fileVersion(signedFixture) > fileVersion(target), "signed fixture is newer");
            expect(signer(signedFixture) == pin, "signed fixture publisher matches");
            Updates updates;
            updates.state = std::make_shared<Updates::State>();
            auto work = updates.state;
            work->target = target;
            std::atomic_int calls = 0;
            std::string selected = "1.3.1";
            bool offline = false, failDownload = false, incompleteAssets = false;
            work->text = [&](const std::wstring &url, const std::atomic_bool &) {
                ++calls;
                need(!offline, "fixture offline");
                if (incompleteAssets && !url.ends_with(L"SHA256SUMS.txt"))
                    return "{\"tag_name\":\"v" + selected +
                           "\",\"draft\":false,\"prerelease\":false,\"assets\":[]}";
                return url.ends_with(L"SHA256SUMS.txt") ? fixtureHash + "  VolturaWorlds.exe\n" : metadata(selected);
            };
            work->file = [&](const std::wstring &, const fs::path &out, const std::atomic_bool &) {
                need(!failDownload, "fixture interrupted");
                need(CopyFileW(signedFixture.c_str(), out.c_str(), TRUE) != FALSE);
            };
            auto wait = [&] {
                auto until = GetTickCount64() + 10000;
                while (work->busy && GetTickCount64() < until)
                    Sleep(5);
                expect(!work->busy, "background update completes");
            };
            updates.check();
            wait();
            expect(work->ready && cached(root, target) && calls == 2, "signed update staged atomically");
            auto older = work->ready;
            offline = true;
            updates.check();
            wait();
            expect(work->ready.has_value(), "offline recheck retains verified download");
            offline = false;
            selected = fileVersion(target).text();
            updates.check();
            wait();
            expect(!work->ready && !cached(root, target), "retracted release cannot remain eligible");
            selected = "1.3.1";
            updates.check();
            wait();
            older = work->ready;
            selected = "1.4.0";
            {
                Handle lockedMarker(CreateFileW((root / L"pending").c_str(), GENERIC_READ, FILE_SHARE_READ,
                                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
                expect(lockedMarker.h != nullptr, "marker write failure fixture");
                updates.check();
                wait();
                expect(!work->ready && WaitForSingleObject(*work->sessionSignal, 0) == WAIT_OBJECT_0,
                       "failed newer marker write invalidates memory and cancels old helpers");
            }
            // Subsequent scenarios model a fresh session after the filesystem failure.
            ResetEvent(*work->sessionSignal);
            offline = false;
            selected = "1.4.0";
            failDownload = true;
            updates.check();
            wait();
            expect(!work->ready && !cached(root, target) && fs::exists(older->payload()),
                   "newer failed download invalidates older ready update");
            incompleteAssets = true;
            updates.check();
            wait();
            expect(!work->ready && wanted(root, target) == Version::parse("1.4.0"),
                   "incomplete newer release still supersedes old download");
            incompleteAssets = false;
            selected = "1.3.1";
            failDownload = false;
            updates.check();
            wait();
            expect(!work->ready && !cached(root, target),
                   "older server response cannot restore superseded update");
            writeAtomic(root / L"pending", "none\n");
            updates.check();
            wait();
            expect(work->ready.has_value(), "retry stages complete download");
            {
                std::ofstream corrupt(work->ready->payload(), std::ios::binary | std::ios::app);
                corrupt << "tampered";
            }
            updates.check();
            wait();
            expect(work->ready && hashFile(work->ready->payload(), cancel) == fixtureHash,
                   "corrupt staged payload redownloaded");
            updates.close(false);
            expect(updates.state->closing, "shutdown close cannot hand off staged update");
            updates.sessionEnd();
            expect(WaitForSingleObject(*updates.state->sessionSignal, 0) == WAIT_OBJECT_0,
                   "late session query signals helper cancellation after handoff");
            updates.cancel();
            // Busy cancellation never waits for transport; the worker owns its own lifetime.
            cleanup(root);
            updates.state = std::make_shared<Updates::State>();
            work = updates.state;
            work->target = target;
            std::atomic_bool entered = false;
            work->text = [&](const std::wstring &, const std::atomic_bool &stop) {
                entered = true;
                while (!stop)
                    Sleep(5);
                need(false, "cancelled slow transport");
                return std::string{};
            };
            updates.check();
            auto until = GetTickCount64() + 10000;
            while (!entered && GetTickCount64() < until)
                Sleep(5);
            expect(entered, "slow background check started");
            auto at = GetTickCount64();
            updates.cancel();
            expect(GetTickCount64() - at < 100 && !updates.state, "unticking cancels without joining worker");
            wait();
        }
    } catch (...) {
        cleanup(root);
        std::error_code ec;
        fs::remove(target, ec);
        fs::remove(temp, ec);
        throw;
    }
    cleanup(root);
    fs::remove(target);
    fs::remove(temp);
    std::cout << "PASS: " << checks << " updater checks"
              << (signedFixture.empty() ? " (signed fixture suite not requested)"
                                        : " including signed staging/cancellation")
              << '\n';
}
} // namespace wu

int wmain(int argc, wchar_t **argv) {
    try {
        auto helper = wu::updateHelper(argc, argv);
        if (helper >= 0)
            return helper;
        if ((argc == 3 || argc == 4) && std::wstring_view(argv[1]) == L"--handoff-fixture") {
            wu::signedFixture = argv[2];
            wu::handoff = true;
            wu::lateShutdown = argc == 4;
        } else if (argc == 3 && std::wstring_view(argv[1]) == L"--signed-fixture")
            wu::signedFixture = argv[2];
        else if (argc == 3 && std::wstring_view(argv[1]) == L"--verify-signature") {
            std::cout << wu::signer(argv[2]) << '\n';
            return 0;
        } else if (argc == 2 && std::wstring_view(argv[1]) == L"--live-feed") {
            std::atomic_bool cancel = false;
            auto release = wu::releaseInfo(
                wu::getText(L"https://api.github.com/repos/voltura/voltura-worlds/releases/latest", cancel),
                true);
            auto url = L"https://github.com/voltura/voltura-worlds/releases/download/v" +
                       wu::wide(release.version.text()) + L"/SHA256SUMS.txt";
            wu::checksum(wu::getText(url, cancel), release.asset);
            std::cout << "PASS: live WinHTTP stable feed and release checksum download, v"
                      << release.version.text() << '\n';
            return 0;
        } else if (argc == 4 && std::wstring_view(argv[1]) == L"--verify-candidate") {
            std::atomic_bool cancel = false;
            std::filesystem::path source(argv[2]), file(argv[3]);
            wu::Candidate candidate{wu::fileVersion(file),
                                    wu::hashFile(file, cancel),
                                    file.parent_path(),
                                    file.parent_path(),
                                    source,
                                    false};
            wu::need(file.filename() == L"payload.exe");
            wu::verify(candidate, wu::signer(source), cancel);
            return 0;
        } else if ((argc == 4 || argc == 5) && std::wstring_view(argv[1]) == L"--prepare-helper") {
            namespace fs = std::filesystem;
            fs::path target(argv[2]), payload(argv[3]);
            auto root = wu::cacheRoot(target);
            wu::cleanup(root);
            fs::create_directories(root);
            std::atomic_bool cancel = false;
            wu::Candidate next{wu::fileVersion(payload),
                               wu::hashFile(payload, cancel),
                               root,
                               root / L"123456789",
                               target,
                               argc == 5 && std::wstring_view(argv[4]) == L"setup"};
            fs::create_directory(next.directory);
            wu::need(CopyFileW(payload.c_str(), next.payload().c_str(), TRUE) != FALSE);
            wu::need(CopyFileW(target.c_str(), next.helper().c_str(), TRUE) != FALSE);
            wu::writeAtomic(root / L"pending", next.marker());
            std::wcout << next.helper().wstring() << '\n';
            return 0;
        } else if (argc == 2 && std::wstring_view(argv[1]) == L"--hold") {
            if (!wu::updateReaderMayStart())
                return 0;
            wu::Session session;
            auto until = GetTickCount64() + 30000;
            while (IsWindow(session.hwnd) && GetTickCount64() < until)
                session.pump(10);
            return 0;
        }
        wu::testUpdates();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
