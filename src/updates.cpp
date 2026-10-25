// Adapted from Voltura Markdown, MIT, Copyright (c) 2026 Voltura AB.
#include "updates.h"
#include <algorithm>
#include <array>
#include <bcrypt.h>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <shellapi.h>
#include <shlobj.h>
#include <softpub.h>
#include <stdexcept>
#include <thread>
#include <tlhelp32.h>
#include <vector>
#include <wincrypt.h>
#include <winhttp.h>
#include <wintrust.h>

namespace wu {
namespace {
namespace fs = std::filesystem;
void need(bool ok, const char *message = "Update unavailable") {
    if (!ok)
        throw std::runtime_error(message);
}
struct Handle {
    HANDLE h = nullptr;
    explicit Handle(HANDLE value = nullptr) : h(value == INVALID_HANDLE_VALUE ? nullptr : value) {}
    ~Handle() {
        if (h)
            CloseHandle(h);
    }
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    operator HANDLE() const {
        return h;
    }
};
struct Internet {
    HINTERNET h;
    explicit Internet(HINTERNET value) : h(value) {
        need(h != nullptr);
    }
    ~Internet() {
        WinHttpCloseHandle(h);
    }
    operator HINTERNET() const {
        return h;
    }
};
struct Lock {
    Handle handle;
    bool owned = false;
    Lock(const std::wstring &name, DWORD timeout = 0) : handle(CreateMutexW(nullptr, FALSE, name.c_str())) {
        if (handle.h) {
            auto result = WaitForSingleObject(handle, timeout);
            owned = result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
        }
    }
    ~Lock() {
        if (owned)
            ReleaseMutex(handle);
    }
};
std::wstring wide(std::string_view value) {
    if (value.empty())
        return {};
    auto size =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), (int)value.size(), nullptr, 0);
    need(size > 0);
    std::wstring out(size, 0);
    need(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), (int)value.size(), out.data(),
                             size) == size);
    return out;
}
fs::path executable() {
    std::wstring out(32768, 0);
    auto n = GetModuleFileNameW(nullptr, out.data(), (DWORD)out.size());
    need(n && n < out.size());
    out.resize(n);
    return out;
}
std::wstring normalized(const fs::path &path) {
    auto out = path.lexically_normal().wstring();
    std::replace(out.begin(), out.end(), L'/', L'\\');
    if (out.starts_with(L"\\\\?\\"))
        out.erase(0, 4);
    CharLowerBuffW(out.data(), (DWORD)out.size());
    return out;
}
std::wstring identity(const fs::path &path) {
    // Cheap, stable namespace only, never an integrity check. No disk access on reader startup.
    uint64_t hash = 14695981039346656037ull;
    for (auto c : normalized(path)) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    return std::to_wstring(hash);
}
std::wstring gateName(const fs::path &path) {
    return L"Local\\VolturaWorldsUpdate-" + identity(path);
}
fs::path cacheRoot(const fs::path &target) {
    PWSTR root = nullptr;
    need(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &root)));
    fs::path path(root);
    CoTaskMemFree(root);
    return path / L"Voltura" / L"Worlds" / L"Updates" / identity(target);
}
bool ordinary(const fs::path &path) {
    auto current = path;
    while (!current.empty()) {
        auto attrs = GetFileAttributesW(current.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_REPARSE_POINT))
            return false;
        auto parent = current.parent_path();
        if (parent == current)
            break;
        current = parent;
    }
    return true;
}
void log(const fs::path &root, const char *message) noexcept {
    try {
        std::error_code ec;
        if (!ordinary(root))
            return;
        fs::create_directories(root, ec);
        auto file = root / L"updates.log";
        if (!ordinary(file))
            return;
        if (fs::file_size(file, ec) > 64 * 1024 && !ec)
            fs::remove(file, ec);
        std::ofstream out(file, std::ios::app);
        out << GetTickCount64() << ' ' << message << '\n';
    } catch (...) {
    }
}
struct Version {
    std::array<unsigned, 3> parts{};
    auto operator<=>(const Version &) const = default;
    std::string text() const {
        return std::to_string(parts[0]) + "." + std::to_string(parts[1]) + "." + std::to_string(parts[2]);
    }
    static Version parse(std::string_view text) {
        Version result;
        for (size_t i = 0; i < 3; ++i) {
            auto stop = text.find('.');
            auto item = text.substr(0, stop);
            need(!item.empty() && item.size() <= 5 && (item.size() == 1 || item[0] != '0'));
            auto [end, error] = std::from_chars(item.data(), item.data() + item.size(), result.parts[i]);
            need(error == std::errc{} && end == item.data() + item.size() && result.parts[i] <= 65535);
            need((i == 2) == (stop == text.npos));
            if (stop != text.npos)
                text.remove_prefix(stop + 1);
        }
        return result;
    }
};
// Small bounded JSON reader: rejects duplicate keys, malformed strings, and excessive nesting.
struct Json {
    char type = 0;
    std::string value;
    std::map<std::string, Json> fields;
    std::vector<Json> items;
    const Json &at(const char *key) const {
        need(type == '{' && fields.contains(key));
        return fields.at(key);
    }
    std::string string() const {
        need(type == '"');
        return value;
    }
    bool boolean() const {
        need(type == 'b');
        return value == "true";
    }
};
struct JsonReader {
    std::string_view input;
    size_t pos = 0;
    void space() {
        while (pos < input.size() &&
               (input[pos] == ' ' || input[pos] == '\t' || input[pos] == '\r' || input[pos] == '\n'))
            ++pos;
    }
    char take() {
        need(pos < input.size());
        return input[pos++];
    }
    bool eat(char c) {
        space();
        if (pos < input.size() && input[pos] == c) {
            ++pos;
            return true;
        }
        return false;
    }
    std::string string() {
        need(take() == '"');
        std::string value;
        for (;;) {
            auto c = take();
            if (c == '"')
                break;
            need((unsigned char)c >= 32);
            if (c == '\\') {
                c = take();
                switch (c) {
                case '"':
                case '\\':
                case '/':
                    break;
                case 'b':
                    c = '\b';
                    break;
                case 'f':
                    c = '\f';
                    break;
                case 'n':
                    c = '\n';
                    break;
                case 'r':
                    c = '\r';
                    break;
                case 't':
                    c = '\t';
                    break;
                case 'u': {
                    // Preserve escapes in irrelevant release prose; release identifiers must be ASCII.
                    value += "\\u";
                    for (int i = 0; i < 4; ++i) {
                        auto hex = take();
                        need(isxdigit((unsigned char)hex) != 0);
                        value += hex;
                    }
                    continue;
                }
                default:
                    need(false);
                }
            }
            value += c;
        }
        return value;
    }
    Json parse(int depth = 0) {
        need(depth < 32);
        space();
        need(pos < input.size());
        Json out;
        auto c = input[pos];
        out.type = c;
        if (c == '"')
            out.value = string();
        else if (c == '{') {
            ++pos;
            if (!eat('}'))
                for (;;) {
                    space();
                    auto key = string();
                    need(eat(':'));
                    need(!out.fields.contains(key));
                    out.fields.emplace(key, parse(depth + 1));
                    if (eat('}'))
                        break;
                    need(eat(','));
                }
        } else if (c == '[') {
            ++pos;
            if (!eat(']'))
                for (;;) {
                    out.items.push_back(parse(depth + 1));
                    if (eat(']'))
                        break;
                    need(eat(','));
                }
        } else {
            auto start = pos;
            while (pos < input.size() && input[pos] != ',' && input[pos] != '}' && input[pos] != ']' &&
                   input[pos] != ' ' && input[pos] != '\r' && input[pos] != '\n' && input[pos] != '\t')
                ++pos;
            out.value = std::string(input.substr(start, pos - start));
            if (out.value == "true" || out.value == "false")
                out.type = 'b';
            else if (out.value == "null")
                out.type = '0';
            else {
                double number;
                auto [end, error] =
                    std::from_chars(out.value.data(), out.value.data() + out.value.size(), number);
                need(error == std::errc{} && end == out.value.data() + out.value.size());
                out.type = 'n';
            }
        }
        return out;
    }
    Json document() {
        auto out = parse();
        space();
        need(pos == input.size());
        return out;
    }
};
Version fileVersion(const fs::path &file, std::optional<bool> setup = {}) {
    DWORD ignored;
    auto size = GetFileVersionInfoSizeW(file.c_str(), &ignored);
    need(size && size < 1024 * 1024);
    std::vector<BYTE> data(size);
    need(GetFileVersionInfoW(file.c_str(), 0, size, data.data()) != FALSE);
    auto text = [&](const wchar_t *name) {
        void *value = nullptr;
        UINT count = 0;
        need(VerQueryValueW(data.data(), name, &value, &count) && count > 0);
        return std::wstring((wchar_t *)value, count - 1);
    };
    need(text(L"\\StringFileInfo\\040904b0\\ProductName") == L"Voltura Worlds");
    need(text(L"\\StringFileInfo\\040904b0\\CompanyName") == L"Voltura AB");
    void *raw = nullptr;
    UINT count = 0;
    need(VerQueryValueW(data.data(), L"\\", &raw, &count) && count >= sizeof(VS_FIXEDFILEINFO));
    auto info = (VS_FIXEDFILEINFO *)raw;
    need(info->dwSignature == 0xfeef04bd && LOWORD(info->dwProductVersionLS) == 0);
    Version version{{HIWORD(info->dwProductVersionMS), LOWORD(info->dwProductVersionMS),
                     HIWORD(info->dwProductVersionLS)}};
    if (setup.has_value()) {
        auto original =
            *setup ? L"VolturaWorlds-Setup-" + wide(version.text()) + L"-win-x64.exe" : L"VolturaWorlds.exe";
        need(text(L"\\StringFileInfo\\040904b0\\OriginalFilename") == original);
        need(text(L"\\StringFileInfo\\040904b0\\FileDescription") ==
             (*setup ? L"Voltura Worlds Setup" : L"Voltura Worlds"));
        std::ifstream in(file, std::ios::binary);
        IMAGE_DOS_HEADER dos{};
        IMAGE_NT_HEADERS64 pe{};
        need((bool)in.read((char *)&dos, sizeof(dos)) && dos.e_magic == IMAGE_DOS_SIGNATURE &&
             dos.e_lfanew > 0 && dos.e_lfanew < 1024 * 1024);
        in.seekg(dos.e_lfanew);
        need((bool)in.read((char *)&pe, sizeof(pe)) && pe.Signature == IMAGE_NT_SIGNATURE);
        need(pe.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 ||
             (*setup && pe.FileHeader.Machine == IMAGE_FILE_MACHINE_I386));
    }
    return version;
}
std::string hex(const BYTE *bytes, size_t count) {
    constexpr char digits[] = "0123456789abcdef";
    std::string out;
    for (size_t i = 0; i < count; ++i) {
        out += digits[bytes[i] >> 4];
        out += digits[bytes[i] & 15];
    }
    return out;
}
bool digestText(std::string_view text) {
    return text.size() == 64 && std::all_of(text.begin(), text.end(), [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}
std::string hashFile(const fs::path &file, const std::atomic_bool &cancel) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    need(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0);
    struct Close {
        BCRYPT_ALG_HANDLE h;
        ~Close() {
            BCryptCloseAlgorithmProvider(h, 0);
        }
    } close{algorithm};
    BCRYPT_HASH_HANDLE hash = nullptr;
    need(BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0);
    struct Destroy {
        BCRYPT_HASH_HANDLE h;
        ~Destroy() {
            BCryptDestroyHash(h);
        }
    } destroy{hash};
    std::ifstream in(file, std::ios::binary);
    need((bool)in);
    char buffer[65536];
    while (in) {
        need(!cancel.load());
        in.read(buffer, sizeof(buffer));
        auto count = in.gcount();
        if (count)
            need(BCryptHashData(hash, (BYTE *)buffer, (ULONG)count, 0) >= 0);
    }
    need(in.eof());
    BYTE digest[32];
    need(BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0);
    return hex(digest, sizeof(digest));
}
std::string signer(const fs::path &file) {
    WINTRUST_FILE_INFO info{sizeof(info)};
    info.pcwszFilePath = file.c_str();
    WINTRUST_DATA data{sizeof(data)};
    data.dwUIChoice = WTD_UI_NONE;
    data.dwUnionChoice = WTD_CHOICE_FILE;
    data.pFile = &info;
    data.dwStateAction = WTD_STATEACTION_VERIFY;
    data.dwProvFlags = WTD_REVOCATION_CHECK_NONE | WTD_CACHE_ONLY_URL_RETRIEVAL;
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    auto result = WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &action, &data);
    std::string pin;
    auto provider = WTHelperProvDataFromStateData(data.hWVTStateData);
    auto signing = provider ? WTHelperGetProvSignerFromChain(provider, 0, FALSE, 0) : nullptr;
    if (signing && signing->csCertChain && signing->csCounterSigners) {
        auto cert = signing->pasCertChain[0].pCert;
        bool selfSigned = CertCompareCertificateName(X509_ASN_ENCODING, &cert->pCertInfo->Subject,
                                                     &cert->pCertInfo->Issuer) != FALSE;
        // The only permitted trust exception is a pinned self-signed publisher's untrusted root.
        if (result == ERROR_SUCCESS || (result == CERT_E_UNTRUSTEDROOT && selfSigned)) {
            BYTE digest[32];
            DWORD size = sizeof(digest);
            if (CryptHashCertificate2(BCRYPT_SHA256_ALGORITHM, 0, nullptr, cert->pbCertEncoded,
                                      cert->cbCertEncoded, digest, &size))
                pin = hex(digest, size);
        }
    }
    data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &action, &data);
    need(!pin.empty(), "Publisher signature verification failed");
    return pin;
}
bool installed(const fs::path &target) {
    wchar_t value[32768];
    DWORD size = sizeof(value);
    std::wstring key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\VolturaWorlds";
#ifdef WORLDS_UPDATE_TESTING
    wchar_t testKey[128]{};
    auto count = GetEnvironmentVariableW(L"WORLDS_UPDATES_TEST_PRODUCT_KEY", testKey, (DWORD)std::size(testKey));
    std::wstring_view selected(testKey);
    constexpr std::wstring_view prefix = L"VolturaWorlds-Test-";
    if (count && count < std::size(testKey) && selected.starts_with(prefix) &&
        selected.size() == prefix.size() + 32 &&
        std::all_of(
            selected.begin() + prefix.size(), selected.end(), [](wchar_t c) { return iswxdigit(c) != 0; }))
        key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + std::wstring(selected);
#endif
    auto status = RegGetValueW(HKEY_CURRENT_USER, key.c_str(), L"InstallLocation",
                               RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY, nullptr, value, &size);
    return status == ERROR_SUCCESS && normalized(fs::path(value) / L"VolturaWorlds.exe") == normalized(target);
}
void active(const std::atomic_bool &cancel, ULONGLONG deadline) {
    need(!cancel.load() && GetTickCount64() < deadline, "Update cancelled or timed out");
}
using Sink = std::function<void(const char *, size_t)>;
void download(std::wstring url, size_t limit, const std::atomic_bool &cancel, const Sink &sink) {
    auto deadline = GetTickCount64() + 120000;
    Internet session(
        WinHttpOpen(L"VolturaWorlds-Updater", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0));
    need(WinHttpSetTimeouts(session, 3000, 5000, 5000, 5000) != FALSE);
    for (int hop = 0; hop < 6; ++hop) {
        active(cancel, deadline);
        URL_COMPONENTS parts{sizeof(parts)};
        parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = (DWORD)-1;
        need(WinHttpCrackUrl(url.c_str(), 0, 0, &parts) != FALSE && parts.nScheme == INTERNET_SCHEME_HTTPS &&
             parts.nPort == 443);
        std::wstring host(parts.lpszHostName, parts.dwHostNameLength),
            path(parts.lpszUrlPath, parts.dwUrlPathLength);
        need(host == L"api.github.com" || host == L"github.com" ||
             host == L"release-assets.githubusercontent.com" || host == L"objects.githubusercontent.com");
        if (parts.dwExtraInfoLength)
            path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
        Internet connection(WinHttpConnect(session, host.c_str(), 443, 0));
        Internet request(WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr, nullptr, nullptr,
                                            WINHTTP_FLAG_SECURE));
        DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER, disable = WINHTTP_DISABLE_COOKIES;
        need(WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy)) != FALSE);
        WinHttpSetOption(request, WINHTTP_OPTION_DISABLE_FEATURE, &disable, sizeof(disable));
        need(WinHttpSendRequest(request, L"Accept: application/json\r\n", (DWORD)-1, nullptr, 0, 0, 0) !=
             FALSE);
        active(cancel, deadline);
        need(WinHttpReceiveResponse(request, nullptr) != FALSE);
        DWORD status = 0, bytes = sizeof(status);
        need(WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr,
                                 &status, &bytes, nullptr) != FALSE);
        if (status >= 300 && status < 400) {
            bytes = 0;
            WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, nullptr, nullptr, &bytes, nullptr);
            need(bytes && bytes < 32768);
            std::wstring next(bytes / sizeof(wchar_t), 0);
            need(WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, nullptr, next.data(), &bytes,
                                     nullptr) != FALSE);
            next.resize(wcslen(next.c_str()));
            need(next.starts_with(L"https://"));
            url = std::move(next);
            continue;
        }
        need(status == 200, "Release server unavailable");
        char buffer[65536];
        DWORD count = 0;
        size_t total = 0;
        for (;;) {
            active(cancel, deadline);
            need(WinHttpReadData(request, buffer, sizeof(buffer), &count) != FALSE);
            active(cancel, deadline);
            if (!count)
                return;
            total += count;
            need(total <= limit, "Update download exceeds size limit");
            sink(buffer, count);
        }
    }
    need(false, "Too many release redirects");
}
std::string getText(const std::wstring &url, const std::atomic_bool &cancel) {
    std::string out;
    download(url, 1024 * 1024, cancel, [&](const char *p, size_t n) { out.append(p, n); });
    return out;
}
struct Release {
    Version version;
    std::string asset;
};
Release releaseInfo(std::string_view json, bool setup, bool requireAssets = true) {
    auto doc = JsonReader{json}.document();
    need(!doc.at("draft").boolean() && !doc.at("prerelease").boolean());
    auto tag = doc.at("tag_name").string();
    need(tag.starts_with('v'));
    auto version = Version::parse(std::string_view(tag).substr(1));
    std::string name = setup ? "VolturaWorlds-Setup-" + version.text() + "-win-x64.exe" : "VolturaWorlds.exe";
    if (!requireAssets)
        return {version, name};
    auto &assets = doc.at("assets");
    need(assets.type == '[');
    int payload = 0, checksums = 0;
    for (auto &asset : assets.items) {
        auto assetName = asset.at("name").string();
        if (assetName == name)
            ++payload;
        if (assetName == "SHA256SUMS.txt")
            ++checksums;
    }
    need(payload == 1 && checksums == 1, "Release assets incomplete");
    return {version, name};
}
std::string checksum(std::string_view text, const std::string &name) {
    std::string result;
    while (!text.empty()) {
        auto stop = text.find('\n');
        auto line = text.substr(0, stop);
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        if (line.size() > 66 && line.substr(64, 2) == "  " && line.substr(66) == name) {
            need(result.empty());
            result = std::string(line.substr(0, 64));
            need(digestText(result));
        }
        if (stop == text.npos)
            break;
        text.remove_prefix(stop + 1);
    }
    need(!result.empty(), "Release checksum missing");
    return result;
}
std::string read(const fs::path &file) {
    std::ifstream in(file, std::ios::binary | std::ios::ate);
    need((bool)in && in.tellg() >= 0 && in.tellg() < 4096);
    std::string out((size_t)in.tellg(), 0);
    in.seekg(0);
    need((bool)in.read(out.data(), out.size()));
    return out;
}
void writeAtomic(const fs::path &file, std::string_view text) {
    auto temp = file;
    temp += L".tmp";
    need(ordinary(file) && ordinary(temp));
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out.write(text.data(), text.size());
        out.close();
        need((bool)out);
    }
    need(MoveFileExW(temp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) !=
         FALSE);
}
struct Candidate {
    Version version;
    std::string digest;
    fs::path root, directory, target;
    bool setup = false;
    fs::path payload() const {
        return directory / L"payload.exe";
    }
    fs::path helper() const {
        return directory / L"helper.exe";
    }
    std::string marker() const {
        auto id = directory.filename().string();
        return version.text() + "\n" + digest + "\n" + id + "\n" + (setup ? "setup\n" : "portable\n");
    }
};
bool automaticUpdatesAllowed(const fs::path &root) {
    auto profile = root.parent_path().parent_path() / L"preferences.ini";
#ifdef WORLDS_UPDATE_TESTING
    wchar_t testProfile[32768]{};
    auto count =
        GetEnvironmentVariableW(L"WORLDS_UPDATES_TEST_PROFILE", testProfile, (DWORD)std::size(testProfile));
    if (count && count < std::size(testProfile))
        profile = testProfile;
#endif
    return ordinary(profile) &&
           GetPrivateProfileIntW(L"worlds", L"automaticUpdates", 0, profile.c_str()) != 0;
}
std::optional<Candidate> cached(const fs::path &root, const fs::path &target) {
    try {
        auto value = read(root / L"pending");
        size_t a = value.find('\n'), b = value.find('\n', a + 1), c = value.find('\n', b + 1);
        need(a != value.npos && b != value.npos && c != value.npos);
        auto version = Version::parse(value.substr(0, a));
        auto digest = value.substr(a + 1, b - a - 1), id = value.substr(b + 1, c - b - 1);
        need(digestText(digest) && !id.empty() && id.size() < 80 &&
             std::all_of(id.begin(), id.end(), [](char x) { return x >= '0' && x <= '9'; }));
        auto mode = value.substr(c + 1);
        need(mode == "setup\n" || mode == "portable\n");
        Candidate out{version, digest, root, root / wide(id), target, mode == "setup\n"};
        need(ordinary(out.directory) && value == out.marker());
        return out;
    } catch (...) {
        return {};
    }
}
std::optional<Version> wanted(const fs::path &root, const fs::path &target) {
    if (auto pending = cached(root, target))
        return pending->version;
    try {
        auto text = read(root / L"pending");
        if (text.starts_with("wanted ") && text.ends_with('\n'))
            return Version::parse(std::string_view(text).substr(7, text.size() - 8));
    } catch (...) {
    }
    return {};
}
void verify(const Candidate &candidate, const std::string &pin, const std::atomic_bool &cancel) {
    need(ordinary(candidate.payload()) && !cancel.load());
    need(fileVersion(candidate.payload(), candidate.setup) == candidate.version, "Update version mismatch");
    need(hashFile(candidate.payload(), cancel) == candidate.digest, "Update checksum mismatch");
    need(signer(candidate.payload()) == pin, "Update publisher mismatch");
}
void prune(const fs::path &root, const fs::path &keep) {
    std::error_code ec;
    for (auto &item : fs::directory_iterator(root)) {
        auto id = item.path().filename().wstring();
        if (item.path() == keep || id.empty() ||
            !std::all_of(id.begin(), id.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; }) ||
            !ordinary(item.path()) || !item.is_directory())
            continue;
        for (auto name : {L"payload.exe", L"helper.exe", L"download.part"})
            fs::remove(item.path() / name, ec);
        fs::remove(item.path(), ec);
    }
}
bool startProcess(const fs::path &file, std::wstring arguments, PROCESS_INFORMATION &process,
                  HANDLE inherit = nullptr) {
    std::wstring command = L"\"" + file.wstring() + L"\" " + arguments;
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = inherit ? sizeof(startup) : sizeof(STARTUPINFOW);
    startup.StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startup.StartupInfo.wShowWindow = SW_HIDE;
    std::vector<BYTE> attributes;
    if (inherit) {
        SIZE_T bytes = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
        attributes.resize(bytes);
        startup.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)attributes.data();
        if (!InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &bytes))
            return false;
        if (!UpdateProcThreadAttribute(startup.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                       &inherit, sizeof(inherit), nullptr, nullptr)) {
            DeleteProcThreadAttributeList(startup.lpAttributeList);
            return false;
        }
    }
    bool result = CreateProcessW(file.c_str(), command.data(), nullptr, nullptr, inherit != nullptr,
                                 CREATE_NO_WINDOW | (inherit ? EXTENDED_STARTUPINFO_PRESENT : 0), nullptr,
                                 file.parent_path().c_str(), &startup.StartupInfo, &process) != FALSE;
    if (inherit)
        DeleteProcThreadAttributeList(startup.lpAttributeList);
    return result;
}
bool readersRunning(const fs::path &target) {
    Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    need(snapshot.h != nullptr);
    PROCESSENTRY32W entry{sizeof(entry)};
    need(Process32FirstW(snapshot, &entry) != FALSE);
    do {
        if (entry.th32ProcessID == GetCurrentProcessId())
            continue;
        if (_wcsicmp(entry.szExeFile, target.filename().c_str()) != 0)
            continue;
        Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID));
        if (!process.h)
            continue;
        wchar_t name[32768];
        DWORD size = (DWORD)std::size(name);
        if (QueryFullProcessImageNameW(process, 0, name, &size) && normalized(name) == normalized(target))
            return true;
    } while (Process32NextW(snapshot, &entry));
    return false;
}
struct Session {
    std::atomic_bool ending = false;
    Handle cancellation;
    HWND hwnd = nullptr;
    static LRESULT CALLBACK procedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        auto self = (Session *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        if (msg == WM_NCCREATE) {
            self = (Session *)((CREATESTRUCTW *)lp)->lpCreateParams;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        }
        if (self && (msg == WM_QUERYENDSESSION || msg == WM_ENDSESSION)) {
            self->ending = true;
            if (self->cancellation.h)
                SetEvent(self->cancellation);
            if (msg == WM_QUERYENDSESSION)
                return TRUE;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    explicit Session(const wchar_t *eventName = nullptr)
        : cancellation(CreateEventW(nullptr, TRUE, FALSE, eventName)) {
        need(cancellation.h != nullptr);
        WNDCLASSW cls{};
        cls.lpfnWndProc = procedure;
        cls.hInstance = GetModuleHandleW(nullptr);
        cls.lpszClassName = L"VolturaWorldsUpdateSession";
        RegisterClassW(&cls);
        // A hidden top-level window receives session broadcasts; message-only windows do not.
        hwnd = CreateWindowW(cls.lpszClassName, L"", WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr,
                             cls.hInstance, this);
        need(hwnd != nullptr);
    }
    ~Session() {
        if (hwnd)
            DestroyWindow(hwnd);
    }
    bool pump(DWORD ms = 0) {
        MsgWaitForMultipleObjects(0, nullptr, FALSE, ms, QS_ALLINPUT);
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (WaitForSingleObject(cancellation, 0) == WAIT_OBJECT_0)
            ending = true;
        return !ending.load();
    }
};
bool applyPortable(const Candidate &candidate) {
    auto replacement = candidate.target;
    replacement += L".update-new";
    auto backup = candidate.target;
    backup += L".update-old";
    need(ordinary(replacement) && ordinary(backup));
    need(!fs::exists(replacement) && !fs::exists(backup), "Previous portable recovery files need attention");
    need(CopyFileW(candidate.payload().c_str(), replacement.c_str(), TRUE) != FALSE);
    if (!ReplaceFileW(candidate.target.c_str(), replacement.c_str(), backup.c_str(), 0, nullptr, nullptr)) {
        // ReplaceFile can fail after moving the original: restore it if necessary.
        if (!fs::exists(candidate.target) && fs::exists(backup))
            MoveFileExW(backup.c_str(), candidate.target.c_str(), 0);
        std::error_code ec;
        fs::remove(replacement, ec);
        return false;
    }
    std::error_code ec;
    fs::remove(backup, ec);
    return true;
}
} // namespace

struct Updates::State {
    std::atomic_bool cancel = false, busy = false, closing = false;
    std::mutex mutex;
    std::optional<Candidate> ready;
    std::unique_ptr<Handle> sessionSignal;
#ifdef WORLDS_UPDATE_TESTING
    fs::path target;
    std::function<std::string(const std::wstring &, const std::atomic_bool &)> text;
    std::function<void(const std::wstring &, const fs::path &, const std::atomic_bool &)> file;
#endif
};
bool Updates::ready() const {
    if (!state) return false;
    std::lock_guard guard(state->mutex);
    return !state->cancel && state->ready.has_value();
}
bool enabled() { return automaticUpdatesAllowed(cacheRoot(executable())); }
void setEnabled(bool value) {
    auto profile = cacheRoot(executable()).parent_path().parent_path() / L"preferences.ini";
    need(ordinary(profile), "Update preferences path is not ordinary");
    fs::create_directories(profile.parent_path());
    need(WritePrivateProfileStringW(L"worlds", L"automaticUpdates", value ? L"1" : L"0", profile.c_str()) != FALSE,
         "Cannot save update preference");
}
Updates::~Updates() {
    cancel();
}
void Updates::cancel() {
    if (state)
        state->cancel = true;
    state.reset();
}
void Updates::sessionEnd() {
    if (state) {
        state->cancel = true;
        std::lock_guard guard(state->mutex);
        if (state->sessionSignal)
            SetEvent(*state->sessionSignal);
        state->ready.reset();
    }
}
#ifdef WORLDS_UPDATE_TESTING
void Updates::fixture(const fs::path &directory) {
    state = std::make_shared<State>();
    state->text = [directory](const std::wstring &url, const std::atomic_bool &cancel) {
        need(!cancel.load());
        return read(directory / (url.ends_with(L"SHA256SUMS.txt") ? L"SHA256SUMS.txt" : L"release.json"));
    };
    state->file = [directory](const std::wstring &, const fs::path &destination,
                              const std::atomic_bool &cancel) {
        writeAtomic(directory / L"download-started", "started\n");
        std::ifstream in(directory / L"payload.exe", std::ios::binary);
        std::ofstream out(destination, std::ios::binary);
        need((bool)in && (bool)out);
        char buffer[65536];
        while (in) {
            need(!cancel.load());
            in.read(buffer, sizeof(buffer));
            out.write(buffer, in.gcount());
            Sleep(150);
        }
        out.close();
        need(in.eof() && (bool)out);
        writeAtomic(directory / L"download-finished", "finished\n");
    };
}
#endif
void Updates::check() {
    if (!state) {
        try {
            state = std::make_shared<State>();
        } catch (...) {
            return;
        }
    }
    auto work = state;
    if (work->cancel || work->busy.exchange(true))
        return;
    try {
        std::thread([work] {
            struct BusyReset {
                std::atomic_bool &busy;
                ~BusyReset() {
                    busy = false;
                }
            } reset{work->busy};
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
            fs::path root;
            try {
                auto target = executable();
#ifdef WORLDS_UPDATE_TESTING
                if (!work->target.empty())
                    target = work->target;
#endif
                root = cacheRoot(target);
                Lock lock(gateName(target) + L"-download");
                // Another reader may be finishing the same download. Wait only on this worker,
                // then hydrate its verified cache so the last reader can install on close.
                auto lockDeadline = GetTickCount64() + 120000;
                while (!lock.owned && lock.handle.h && !work->cancel && !work->closing &&
                       GetTickCount64() < lockDeadline) {
                    auto result = WaitForSingleObject(lock.handle, 100);
                    lock.owned = result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
                }
                need(lock.owned && !work->cancel);
                need(ordinary(root));
                fs::create_directories(root);
                auto current = fileVersion(target);
                auto pin = signer(target); // Unsigned builds never stage or apply updates.
                SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
                auto signal = std::make_unique<Handle>(
                    CreateEventW(&security, TRUE, FALSE, (gateName(target) + L"-session").c_str()));
                need(signal->h != nullptr);
                {
                    std::lock_guard guard(work->mutex);
                    need(!work->cancel && !work->closing);
                    work->sessionSignal = std::move(signal);
                }
                auto setup = installed(target);
#ifndef WORLDS_UPDATE_TESTING
                need(setup, "Automatic updates require the installed app");
#endif
                auto old = cached(root, target);
                auto latestSeen = wanted(root, target);
                prune(root, old ? old->directory : fs::path{});
                {
                    std::lock_guard guard(work->mutex);
                    work->ready.reset();
                }
                if (old && old->version > current && old->setup == setup) {
                    try {
                        verify(*old, pin, work->cancel);
                        need(signer(old->helper()) == pin);
                        std::lock_guard guard(work->mutex);
                        work->ready = old;
                    } catch (...) {
                        old.reset();
                        {
                            std::lock_guard guard(work->mutex);
                            work->ready.reset();
                        }
                        writeAtomic(root / L"pending", "invalid\n");
                    }
                }
                auto text = [&](const std::wstring &url) {
#ifdef WORLDS_UPDATE_TESTING
                    if (work->text)
                        return work->text(url, work->cancel);
#endif
                    return getText(url, work->cancel);
                };
                auto metadata =
                    text(L"https://api.github.com/repos/voltura/voltura-worlds/releases/latest");
                auto release = releaseInfo(metadata, setup, false);
                auto invalidatePending = [&](const Version &version) {
                    {
                        std::lock_guard guard(work->mutex);
                        work->ready.reset();
                    }
                    try {
                        writeAtomic(root / L"pending", "wanted " + version.text() + "\n");
                    } catch (...) {
                        // A failed marker replacement must also cancel helpers holding the old one.
                        std::lock_guard guard(work->mutex);
                        if (work->sessionSignal)
                            SetEvent(*work->sessionSignal);
                        throw;
                    }
                };
                if (latestSeen && release.version < *latestSeen) {
                    invalidatePending(*latestSeen);
                    need(false, "Release older than the newest discovered update");
                }
                if (release.version <= current) {
                    {
                        std::lock_guard guard(work->mutex);
                        work->ready.reset();
                    }
                    writeAtomic(root / L"pending", "none\n");
                    return;
                }
                // Invalidate the shared marker before fetching any part of the newer payload.
                invalidatePending(release.version);
                releaseInfo(metadata,
                            setup); // A published tag counts as discovered even during asset upload.
                if (old && old->version == release.version && old->setup == setup) {
                    writeAtomic(root / L"pending", old->marker());
                    std::lock_guard guard(work->mutex);
                    work->ready = old;
                    return;
                }
                need(!work->closing && !work->cancel);
                auto url = L"https://github.com/voltura/voltura-worlds/releases/download/v" +
                           wide(release.version.text()) + L"/";
                auto digest = checksum(text(url + L"SHA256SUMS.txt"), release.asset);
                auto id = std::to_wstring(GetCurrentProcessId()) + std::to_wstring(GetTickCount64());
                Candidate next{release.version, digest, root, root / id, target, setup};
                need(fs::create_directory(next.directory));
                auto partial = next.directory / L"download.part";
                try {
#ifdef WORLDS_UPDATE_TESTING
                    if (work->file)
                        work->file(url + wide(release.asset), partial, work->cancel);
                    else
#endif
                    {
                        std::ofstream out(partial, std::ios::binary | std::ios::trunc);
                        need((bool)out);
                        download(url + wide(release.asset), 64 * 1024 * 1024, work->cancel,
                                 [&](const char *p, size_t n) {
                                     out.write(p, n);
                                     need((bool)out);
                                 });
                        out.close();
                        need((bool)out);
                    }
                    need(!work->cancel && !work->closing);
                    need(MoveFileExW(partial.c_str(), next.payload().c_str(), MOVEFILE_WRITE_THROUGH) !=
                         FALSE);
                    verify(next, pin, work->cancel);
                    need(CopyFileW(target.c_str(), next.helper().c_str(), TRUE) != FALSE);
                    need(signer(next.helper()) == pin && !work->cancel && !work->closing);
                    writeAtomic(root / L"pending", next.marker());
                    {
                        std::lock_guard guard(work->mutex);
                        work->ready = next;
                    }
                    log(root, "Update downloaded and verified");
                    // Remove only superseded, owned payload files, never recursively delete a path.
                    if (old && old->directory != next.directory) {
                        std::error_code ec;
                        for (auto name : {L"payload.exe", L"helper.exe", L"download.part"})
                            fs::remove(old->directory / name, ec);
                        fs::remove(old->directory, ec);
                    }
                } catch (...) {
                    std::error_code ec;
                    for (auto name : {L"payload.exe", L"helper.exe", L"download.part"})
                        fs::remove(next.directory / name, ec);
                    fs::remove(next.directory, ec);
                    throw;
                }
            } catch (const std::exception &e) {
                if (!root.empty())
                    log(root, e.what());
            }
        }).detach();
    } catch (...) {
        work->busy = false;
    }
}
void Updates::close(bool install, bool restart) {
    if (!state)
        return;
    auto work = state;
    if (work->closing.exchange(true))
        return;
    work->cancel = true;
    std::optional<Candidate> next;
    {
        std::lock_guard guard(work->mutex);
        if (install)
            next = work->ready;
    }
    if (next) {
        // No filesystem scans, copying, verification, or network work on the reader close path.
        PROCESS_INFORMATION process{};
        auto args =
            L"--apply-update \"" + next->target.wstring() + L"\" " + std::to_wstring(GetCurrentProcessId()) + (restart ? L" restart" : L"");
        HANDLE signal = nullptr;
        {
            std::lock_guard guard(work->mutex);
            if (work->sessionSignal)
                signal = work->sessionSignal->h;
        }
        if (signal && startProcess(next->helper(), args, process, signal)) {
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
        }
    }
    // Keep the signal until reader destruction: a late shutdown query during renderer teardown
    // must still cancel the helper. Its inherited handle bridges parent exit and helper startup.
}
bool updateReaderMayStart() {
    try {
        auto name = gateName(executable());
        // The object exists only while a helper is active. No cache/preferences I/O here.
        Handle gate(OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, name.c_str()));
        if (!gate.h)
            return true;
        Handle reopened(OpenEventW(EVENT_MODIFY_STATE, FALSE, (name + L"-reopened").c_str()));
        if (reopened.h)
            SetEvent(reopened);
        auto status = WaitForSingleObject(gate, 30000);
        if (status == WAIT_OBJECT_0 || status == WAIT_ABANDONED) {
            ReleaseMutex(gate);
            return true;
        }
        return false;
    } catch (...) {
        return true;
    }
}
int updateHelper(int argc, wchar_t **argv) {
    if (argc < 2 || std::wstring_view(argv[1]) != L"--apply-update")
        return -1;
    if (argc != 4 && !(argc == 5 && std::wstring_view(argv[4]) == L"restart"))
        return 1;
    SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
    fs::path root;
    try {
        fs::path target(argv[2]);
        need(target.is_absolute() && normalized(target.extension()) == L".exe" && ordinary(target));
        root = cacheRoot(target);
        auto self = executable();
        need(normalized(self.parent_path().parent_path()) == normalized(root) &&
                 self.filename() == L"helper.exe" && ordinary(self),
             "Helper is outside its update cache");
        auto next = cached(root, target);
        need(next && normalized(next->helper()) == normalized(self), "Pending update changed before handoff");
        auto parentId = std::wstring(argv[3]);
        need(!parentId.empty() &&
             std::all_of(parentId.begin(), parentId.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; }));
        auto id = std::stoul(parentId);
        need(id && id <= MAXDWORD);
        Handle parent(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)id));
        // Parent may already have exited. If present, it must be the target reader.
        if (parent.h && WaitForSingleObject(parent, 0) == WAIT_TIMEOUT) {
            wchar_t path[32768];
            DWORD length = (DWORD)std::size(path);
            if (QueryFullProcessImageNameW(parent, 0, path, &length))
                need(normalized(path) == normalized(target), "Handoff parent does not match reader");
            else
                need(WaitForSingleObject(parent, 0) == WAIT_OBJECT_0, "Cannot identify handoff parent");
        }
        auto cancelName = gateName(target) + L"-session";
        Session session(cancelName.c_str());
        Lock gate(gateName(target));
        need(gate.owned, "Another update installation is active");
        Handle reopened(CreateEventW(nullptr, TRUE, FALSE, (gateName(target) + L"-reopened").c_str()));
        need(reopened.h != nullptr);
        auto deadline = GetTickCount64() + 30000;
        ULONGLONG nextScan = 0;
        for (;;) {
            need(session.pump(50) && WaitForSingleObject(reopened, 0) != WAIT_OBJECT_0,
                 "Installation deferred for session end or reader reopen");
            if ((!parent.h || WaitForSingleObject(parent, 0) == WAIT_OBJECT_0) &&
                GetTickCount64() >= nextScan) {
                if (!readersRunning(target))
                    break;
                nextScan = GetTickCount64() + 500;
            }
            need(GetTickCount64() < deadline, "Reader still open; installation deferred");
        }
        // Verification happens on a worker so the hidden window services session-end requests.
        Handle payload(CreateFileW(next->payload().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        need(payload.h != nullptr);
        std::atomic_bool verified = false, finished = false;
        std::thread verification([&] {
            try {
                auto pin = signer(self);
                need(signer(target) == pin && next->version > fileVersion(target),
                     "Running reader identity changed");
                need(next->setup == installed(target), "Installation location changed");
#ifndef WORLDS_UPDATE_TESTING
                need(next->setup, "Only installed updates are supported");
#endif
                verify(*next, pin, session.ending);
                need(read(root / L"pending") == next->marker());
                verified = true;
            } catch (const std::exception &e) {
                log(root, e.what());
            }
            finished = true;
        });
        while (!finished)
            session.pump(20);
        verification.join();
        need(verified, "Update revalidation failed");
        need(session.pump(150) && WaitForSingleObject(reopened, 0) != WAIT_OBJECT_0,
             "Session ended or reader reopened before installation");
        need(!readersRunning(target), "Reader reopened before installation");
        // Download ownership prevents another running reader from changing the selected release.
        Lock downloadGate(gateName(target) + L"-download");
        need(downloadGate.owned, "Another update check is active; installation deferred");
        need(read(root / L"pending") == next->marker(), "A newer update was selected; installation deferred");
        need(automaticUpdatesAllowed(root), "Automatic updates disabled; installation deferred");
        need(session.pump() && !GetSystemMetrics(SM_SHUTTINGDOWN) &&
                 WaitForSingleObject(reopened, 0) != WAIT_OBJECT_0,
             "Session ended before installation");
        bool success = false;
        if (next->setup) {
            PROCESS_INFORMATION process{};
            need(startProcess(next->payload(),
                              L"/S /AUTOMATICUPDATE /UPDATECANCEL=" + cancelName + L" /D=" +
                                  target.parent_path().wstring(),
                              process));
            Handle thread(process.hThread), child(process.hProcess);
            while (WaitForSingleObject(child, 0) == WAIT_TIMEOUT)
                session.pump(50);
            DWORD code = 1;
            GetExitCodeProcess(child, &code);
            success = code == 0;
        } else
            success = applyPortable(*next);
        need(success, "Update installation failed; retained for retry");
        need(fileVersion(target) == next->version && signer(target) == signer(self));
        writeAtomic(root / L"pending", "none\n");
        log(root, "Update installed successfully");
        if (argc == 5 && session.pump() && !GetSystemMetrics(SM_SHUTTINGDOWN)) {
            PROCESS_INFORMATION launched{};
            if (startProcess(target, L"--wallpaper", launched)) {
                CloseHandle(launched.hThread); CloseHandle(launched.hProcess);
            }
        }
        return 0;
    } catch (const std::exception &e) {
        if (!root.empty())
            log(root, e.what());
        return 1;
    }
}
} // namespace wu
