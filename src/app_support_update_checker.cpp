#include <mwfl/app_support/update_checker.h>

#include <commctrl.h>
#include <shellapi.h>
#include <winhttp.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cwctype>
#include <mutex>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace mwfl::app_support {
namespace {

constexpr UINT_PTR kSubclassId = 0x4D574655;
constexpr UINT kUpdateReadyMessage = WM_APP + 0x371;
constexpr UINT kCheckNowCommand = 0xEF40;
constexpr UINT kAutomaticChecksCommand = 0xEF41;
constexpr wchar_t kEnabledValue[] = L"AutomaticUpdateChecks";
constexpr wchar_t kLastCheckValue[] = L"UpdateLastCheckUtc";
constexpr wchar_t kRemindAfterValue[] = L"UpdateRemindAfterUtc";
constexpr std::uint64_t kSecondsPerDay = 24ULL * 60ULL * 60ULL;

struct ReleaseInfo {
    std::wstring version;
    std::wstring url;
};

struct CheckResult {
    std::optional<ReleaseInfo> release;
    std::wstring error;
    bool manual{};
};

std::uint64_t UnixNow() noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

std::optional<std::uint64_t> ReadQword(std::wstring_view key, const wchar_t* name) {
    ULONGLONG value{};
    DWORD size = sizeof(value);
    if (::RegGetValueW(HKEY_CURRENT_USER, std::wstring(key).c_str(), name,
                       RRF_RT_REG_QWORD, nullptr, &value, &size) != ERROR_SUCCESS)
        return std::nullopt;
    return static_cast<std::uint64_t>(value);
}

bool ReadEnabled(std::wstring_view key) {
    DWORD value = 1;
    DWORD size = sizeof(value);
    const LSTATUS status = ::RegGetValueW(HKEY_CURRENT_USER, std::wstring(key).c_str(),
                                          kEnabledValue, RRF_RT_REG_DWORD, nullptr,
                                          &value, &size);
    return status != ERROR_SUCCESS || value != 0;
}

void WriteQword(std::wstring_view key, const wchar_t* name, std::uint64_t value) {
    HKEY handle{};
    if (::RegCreateKeyExW(HKEY_CURRENT_USER, std::wstring(key).c_str(), 0, nullptr, 0,
                          KEY_SET_VALUE, nullptr, &handle, nullptr) != ERROR_SUCCESS)
        return;
    const ULONGLONG native = value;
    static_cast<void>(::RegSetValueExW(handle, name, 0, REG_QWORD,
                                       reinterpret_cast<const BYTE*>(&native),
                                       sizeof(native)));
    ::RegCloseKey(handle);
}

void WriteEnabled(std::wstring_view key, bool enabled) {
    HKEY handle{};
    if (::RegCreateKeyExW(HKEY_CURRENT_USER, std::wstring(key).c_str(), 0, nullptr, 0,
                          KEY_SET_VALUE, nullptr, &handle, nullptr) != ERROR_SUCCESS)
        return;
    const DWORD value = enabled ? 1U : 0U;
    static_cast<void>(::RegSetValueExW(handle, kEnabledValue, 0, REG_DWORD,
                                       reinterpret_cast<const BYTE*>(&value),
                                       sizeof(value)));
    ::RegCloseKey(handle);
}

std::wstring Utf8ToWide(std::string_view text) {
    if (text.empty()) return {};
    const int required = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                                static_cast<int>(text.size()), nullptr, 0);
    if (required <= 0) return {};
    std::wstring result(static_cast<std::size_t>(required), L'\0');
    if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                              static_cast<int>(text.size()), result.data(), required) <= 0)
        return {};
    return result;
}

std::optional<std::string> JsonString(std::string_view json, std::string_view field) {
    const std::string needle = "\"" + std::string(field) + "\"";
    std::size_t position = json.find(needle);
    if (position == std::string_view::npos) return std::nullopt;
    position = json.find(':', position + needle.size());
    if (position == std::string_view::npos) return std::nullopt;
    position = json.find('"', position + 1);
    if (position == std::string_view::npos) return std::nullopt;
    ++position;
    std::string value;
    for (; position < json.size(); ++position) {
        const char ch = json[position];
        if (ch == '"') return value;
        if (ch != '\\') {
            value.push_back(ch);
            continue;
        }
        if (++position >= json.size()) return std::nullopt;
        switch (json[position]) {
            case '"': value.push_back('"'); break;
            case '\\': value.push_back('\\'); break;
            case '/': value.push_back('/'); break;
            case 'b': value.push_back('\b'); break;
            case 'f': value.push_back('\f'); break;
            case 'n': value.push_back('\n'); break;
            case 'r': value.push_back('\r'); break;
            case 't': value.push_back('\t'); break;
            default: return std::nullopt;
        }
    }
    return std::nullopt;
}

struct InternetHandle {
    HINTERNET value{};
    ~InternetHandle() { if (value) ::WinHttpCloseHandle(value); }
};

CheckResult FetchLatest(const UpdateCheckerOptions& options, bool manual) {
    CheckResult result{.manual = manual};
    const std::wstring agent = options.application_name + L"/" + options.current_version;
    InternetHandle session{::WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!session.value) {
        result.error = L"Windows could not start the HTTPS request.";
        return result;
    }
    ::WinHttpSetTimeouts(session.value, 5000, 5000, 5000, 10000);
    InternetHandle connection{::WinHttpConnect(session.value, L"api.github.com",
                                                INTERNET_DEFAULT_HTTPS_PORT, 0)};
    if (!connection.value) {
        result.error = L"GitHub could not be reached.";
        return result;
    }
    const std::wstring path = L"/repos/mwfl/" + options.repository + L"/releases/latest";
    InternetHandle request{::WinHttpOpenRequest(connection.value, L"GET", path.c_str(), nullptr,
                                                 WINHTTP_NO_REFERER,
                                                 WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                 WINHTTP_FLAG_SECURE)};
    if (!request.value) {
        result.error = L"The release request could not be created.";
        return result;
    }
    constexpr wchar_t headers[] =
        L"Accept: application/vnd.github+json\r\n"
        L"X-GitHub-Api-Version: 2022-11-28\r\n";
    if (!::WinHttpSendRequest(request.value, headers, static_cast<DWORD>(-1),
                              WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !::WinHttpReceiveResponse(request.value, nullptr)) {
        result.error = L"GitHub did not return a release response.";
        return result;
    }
    DWORD status{};
    DWORD status_size = sizeof(status);
    if (!::WinHttpQueryHeaders(request.value,
                               WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                               WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
                               WINHTTP_NO_HEADER_INDEX) || status != 200) {
        result.error = status == 403 ? L"GitHub's anonymous request limit was reached. Try again later."
                                     : L"No stable GitHub Release is available yet.";
        return result;
    }
    std::string body;
    for (;;) {
        DWORD available{};
        if (!::WinHttpQueryDataAvailable(request.value, &available)) {
            result.error = L"The release response was interrupted.";
            return result;
        }
        if (available == 0) break;
        if (body.size() + available > 256U * 1024U) {
            result.error = L"The release response was unexpectedly large.";
            return result;
        }
        const std::size_t offset = body.size();
        body.resize(offset + available);
        DWORD read{};
        if (!::WinHttpReadData(request.value, body.data() + offset, available, &read)) {
            result.error = L"The release response could not be read.";
            return result;
        }
        body.resize(offset + read);
    }
    const auto tag = JsonString(body, "tag_name");
    const auto url = JsonString(body, "html_url");
    if (!tag || !url) {
        result.error = L"GitHub returned an unrecognized release response.";
        return result;
    }
    ReleaseInfo release{Utf8ToWide(*tag), Utf8ToWide(*url)};
    if (release.version.empty() || release.url.rfind(L"https://github.com/mwfl/", 0) != 0) {
        result.error = L"GitHub returned invalid release metadata.";
        return result;
    }
    result.release = std::move(release);
    return result;
}

std::wstring MenuText(HMENU menu, int position) {
    const int length = ::GetMenuStringW(menu, static_cast<UINT>(position), nullptr, 0,
                                         MF_BYPOSITION);
    if (length <= 0) return {};
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    ::GetMenuStringW(menu, static_cast<UINT>(position), text.data(), length + 1, MF_BYPOSITION);
    text.resize(static_cast<std::size_t>(length));
    std::erase(text, L'&');
    std::ranges::transform(text, text.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return text;
}

HMENU FindSettingsMenu(HMENU root) {
    const int count = ::GetMenuItemCount(root);
    for (int index = 0; index < count; ++index) {
        const std::wstring text = MenuText(root, index);
        if (text.find(L"settings") == std::wstring::npos &&
            text.find(L"options") == std::wstring::npos &&
            text.find(L"preferences") == std::wstring::npos)
            continue;
        if (HMENU submenu = ::GetSubMenu(root, index)) return submenu;
    }
    return nullptr;
}

}  // namespace

struct UpdateChecker::Impl {
    std::atomic<HWND> host{};
    UpdateCheckerOptions options;
    std::jthread worker;
    std::mutex mutex;
    std::optional<CheckResult> pending;
    bool running{};
    HMENU settings_menu{};

    ~Impl() { Detach(); }

    void Attach(HWND window, UpdateCheckerOptions configured, bool allow_automatic_check) {
        if (host.load() || !::IsWindow(window)) return;
        host.store(window);
        options = std::move(configured);
        ::SetWindowSubclass(window, SubclassProc, kSubclassId, reinterpret_cast<DWORD_PTR>(this));
        AddMenu();
        if (allow_automatic_check && IsDue()) Start(false);
    }

    void Detach() {
        if (worker.joinable()) {
            worker.request_stop();
            worker.join();
        }
        const HWND window = host.exchange(nullptr);
        if (window && ::IsWindow(window))
            ::RemoveWindowSubclass(window, SubclassProc, kSubclassId);
    }

    bool IsDue() const {
        if (!ReadEnabled(options.settings_key)) return false;
        const std::uint64_t now = UnixNow();
        const std::uint64_t last = ReadQword(options.settings_key, kLastCheckValue).value_or(0);
        const std::uint64_t remind = ReadQword(options.settings_key, kRemindAfterValue).value_or(0);
        return now >= remind && (last == 0 || now >= last + kSecondsPerDay);
    }

    void AddMenu() {
        const HWND window = host.load();
        HMENU root = ::GetMenu(window);
        if (!root) {
            root = ::CreateMenu();
            if (!root || !::SetMenu(window, root)) return;
        }
        settings_menu = FindSettingsMenu(root);
        if (settings_menu) {
            ::AppendMenuW(settings_menu, MF_SEPARATOR, 0, nullptr);
        } else {
            settings_menu = ::CreatePopupMenu();
            if (!settings_menu) return;
            ::AppendMenuW(root, MF_POPUP, reinterpret_cast<UINT_PTR>(settings_menu), L"&Settings");
        }
        ::AppendMenuW(settings_menu, MF_STRING, kCheckNowCommand, L"Check for &Updates...");
        ::AppendMenuW(settings_menu, MF_STRING | (ReadEnabled(options.settings_key) ? MF_CHECKED : 0),
                      kAutomaticChecksCommand, L"Automatically Check for Updates");
        ::DrawMenuBar(window);
    }

    void RefreshMenu() const {
        if (!settings_menu) return;
        ::CheckMenuItem(settings_menu, kAutomaticChecksCommand,
                        MF_BYCOMMAND | (ReadEnabled(options.settings_key) ? MF_CHECKED : MF_UNCHECKED));
        ::EnableMenuItem(settings_menu, kCheckNowCommand,
                         MF_BYCOMMAND | (running ? MF_GRAYED : MF_ENABLED));
        ::DrawMenuBar(host.load());
    }

    void Start(bool manual) {
        {
            std::scoped_lock lock(mutex);
            if (running) return;
            running = true;
        }
        RefreshMenu();
        if (worker.joinable()) worker.join();
        worker = std::jthread([this, manual](std::stop_token stop) {
            CheckResult result = FetchLatest(options, manual);
            if (stop.stop_requested()) return;
            WriteQword(options.settings_key, kLastCheckValue, UnixNow());
            {
                std::scoped_lock lock(mutex);
                pending = std::move(result);
                running = false;
            }
            const HWND window = host.load();
            if (window && ::IsWindow(window))
                ::PostMessageW(window, kUpdateReadyMessage, 0, 0);
        });
    }

    void ToggleAutomaticChecks() {
        const bool enabled = !ReadEnabled(options.settings_key);
        WriteEnabled(options.settings_key, enabled);
        RefreshMenu();
        if (enabled) Start(true);
    }

    void Defer(unsigned days) const {
        WriteQword(options.settings_key, kRemindAfterValue,
                   UnixNow() + static_cast<std::uint64_t>(days) * kSecondsPerDay);
    }

    void Present() {
        std::optional<CheckResult> result;
        {
            std::scoped_lock lock(mutex);
            result = std::move(pending);
            pending.reset();
        }
        RefreshMenu();
        if (!result) return;
        if (!result->error.empty()) {
            if (result->manual)
                ::MessageBoxW(host.load(), result->error.c_str(), L"Check for Updates",
                              MB_OK | MB_ICONINFORMATION);
            return;
        }
        if (!result->release ||
            !IsVersionNewer(result->release->version, options.current_version)) {
            if (result->manual) {
                const std::wstring text = options.application_name + L" " +
                    options.current_version + L" is up to date.";
                ::MessageBoxW(host.load(), text.c_str(), L"Check for Updates",
                              MB_OK | MB_ICONINFORMATION);
            }
            return;
        }

        const std::wstring content = L"Installed version: " + options.current_version +
            L"\nLatest version: " + result->release->version +
            L"\n\nPortable releases are replaced only after you explicitly download and extract them.";
        std::array<TASKDIALOG_BUTTON, 3> buttons{{
            {100, L"Download the Portable update\nOpen the official GitHub Release page."},
            {101, L"Remind me in 3 days"},
            {102, L"Remind me next week"},
        }};
        BOOL verification = ReadEnabled(options.settings_key) ? TRUE : FALSE;
        TASKDIALOGCONFIG dialog{};
        dialog.cbSize = sizeof(dialog);
        dialog.hwndParent = host.load();
        dialog.dwFlags = TDF_USE_COMMAND_LINKS | TDF_POSITION_RELATIVE_TO_WINDOW |
                         TDF_SIZE_TO_CONTENT;
        dialog.dwCommonButtons = TDCBF_CANCEL_BUTTON;
        dialog.pszWindowTitle = L"Update available";
        dialog.pszMainIcon = TD_INFORMATION_ICON;
        const std::wstring instruction = options.application_name + L" " +
            result->release->version + L" is available";
        dialog.pszMainInstruction = instruction.c_str();
        dialog.pszContent = content.c_str();
        dialog.cButtons = static_cast<UINT>(buttons.size());
        dialog.pButtons = buttons.data();
        dialog.nDefaultButton = 100;
        dialog.pszVerificationText = L"Automatically check for updates";
        int selected = IDCANCEL;
        if (SUCCEEDED(::TaskDialogIndirect(&dialog, &selected, nullptr, &verification))) {
            WriteEnabled(options.settings_key, verification == TRUE);
            RefreshMenu();
            if (selected == 100) {
                Defer(7);
                ::ShellExecuteW(host.load(), L"open", result->release->url.c_str(), nullptr,
                                nullptr, SW_SHOWNORMAL);
            } else if (selected == 101) {
                Defer(3);
            } else if (selected == 102) {
                Defer(7);
            } else {
                Defer(1);
            }
        }
    }

    static LRESULT CALLBACK SubclassProc(HWND window, UINT message, WPARAM wparam,
                                         LPARAM lparam, UINT_PTR, DWORD_PTR data) {
        auto* self = reinterpret_cast<Impl*>(data);
        if (message == WM_COMMAND) {
            const UINT command = LOWORD(wparam);
            if (command == kCheckNowCommand) {
                self->Start(true);
                return 0;
            }
            if (command == kAutomaticChecksCommand) {
                self->ToggleAutomaticChecks();
                return 0;
            }
        } else if (message == kUpdateReadyMessage) {
            self->Present();
            return 0;
        } else if (message == WM_NCDESTROY) {
            ::RemoveWindowSubclass(window, SubclassProc, kSubclassId);
            self->host.store(nullptr);
            if (self->worker.joinable()) {
                self->worker.request_stop();
                self->worker.join();
            }
        }
        return ::DefSubclassProc(window, message, wparam, lparam);
    }
};

UpdateChecker::UpdateChecker() : impl_(std::make_unique<Impl>()) {}
UpdateChecker::~UpdateChecker() = default;

void UpdateChecker::Attach(HWND host, UpdateCheckerOptions options,
                           bool allow_automatic_check) {
    impl_->Attach(host, std::move(options), allow_automatic_check);
}

}  // namespace mwfl::app_support
