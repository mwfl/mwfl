#include <mwfl/help.h>

#include <htmlhelp.h>
#include <shellapi.h>

#include <algorithm>
#include <cwctype>
#include <ranges>

namespace mwfl {
namespace {

HelpResult Result(HelpStatus status, DWORD error = ERROR_SUCCESS) noexcept {
    return {status, error};
}

bool Clean(std::wstring_view value) noexcept {
    return value.find(L'\0') == std::wstring_view::npos && value.size() <= 32768 &&
           std::none_of(value.begin(), value.end(), [](wchar_t value) {
               return value < 0x20 && value != L'\t';
           });
}

bool SafeTopic(std::wstring_view topic) noexcept {
    if (!Clean(topic) || topic.find(L'"') != std::wstring_view::npos ||
        topic.find(L'\'') != std::wstring_view::npos ||
        topic.find(L"..") != std::wstring_view::npos ||
        topic.find(L':') != std::wstring_view::npos ||
        topic.find(L'\\') != std::wstring_view::npos) return false;
    return topic.empty() || topic.front() == L'/' || topic.front() == L'#';
}

bool ExtensionIs(const std::filesystem::path& path, std::wstring_view expected) {
    std::wstring extension = path.extension().native();
    std::ranges::transform(extension, extension.begin(), std::towlower);
    return extension == expected;
}

HelpResult DefaultBackend(HWND owner, const HelpRequest& request) {
    if (request.kind == HelpTargetKind::compiled_html) {
        std::wstring target = request.local_path.native();
        if (!request.topic.empty()) target += L"::" + request.topic;
        ::SetLastError(ERROR_SUCCESS);
        if (::HtmlHelpW(owner, target.c_str(), HH_DISPLAY_TOPIC, 0))
            return Result(HelpStatus::success);
        const DWORD error = ::GetLastError();
        return Result(error == ERROR_CANCELLED ? HelpStatus::cancelled
                                               : HelpStatus::unavailable,
                      error == ERROR_SUCCESS ? ERROR_NOT_SUPPORTED : error);
    }
    std::wstring target = request.kind == HelpTargetKind::local_html
        ? request.local_path.native() : request.uri;
    if (!request.topic.empty()) target += request.topic;
    SHELLEXECUTEINFOW execute{};
    execute.cbSize = sizeof(execute);
    execute.fMask = SEE_MASK_FLAG_NO_UI;
    execute.hwnd = owner;
    execute.lpVerb = L"open";
    execute.lpFile = target.c_str();
    execute.nShow = SW_SHOWNORMAL;
    if (::ShellExecuteExW(&execute)) return Result(HelpStatus::success);
    const DWORD error = ::GetLastError();
    return Result(error == ERROR_CANCELLED ? HelpStatus::cancelled
                                           : HelpStatus::native_failure,
                  error);
}

}  // namespace

HelpResult ValidateHelpRequest(const HelpRequest& request) noexcept {
    try {
        if (!SafeTopic(request.topic))
            return Result(HelpStatus::untrusted_target, ERROR_INVALID_NAME);
        if (!request.topic.empty() && request.kind != HelpTargetKind::compiled_html &&
            request.topic.front() != L'#')
            return Result(HelpStatus::untrusted_target, ERROR_INVALID_NAME);
        if (request.kind == HelpTargetKind::https_uri) {
            if (!request.local_path.empty() || !Clean(request.uri) ||
                request.uri.size() < 9 ||
                ::CompareStringOrdinal(request.uri.data(), 8, L"https://", 8, TRUE) !=
                    CSTR_EQUAL ||
                request.uri.find_first_of(L"\r\n\t \"") != std::wstring::npos)
                return Result(HelpStatus::untrusted_target, ERROR_INVALID_NAME);
            const std::size_t host_end = request.uri.find_first_of(L"/?#", 8);
            const std::wstring_view host = std::wstring_view(request.uri).substr(
                8, host_end == std::wstring::npos ? std::wstring::npos : host_end - 8);
            if (host.empty() || host.find(L'@') != std::wstring_view::npos)
                return Result(HelpStatus::untrusted_target, ERROR_INVALID_NAME);
            return Result(HelpStatus::success);
        }
        if (!request.uri.empty() || request.local_path.empty() ||
            !request.local_path.is_absolute() ||
            request.local_path.native().starts_with(L"\\\\") ||
            std::ranges::any_of(request.local_path,
                [](const auto& part) { return part == L".."; }))
            return Result(HelpStatus::invalid_argument, ERROR_INVALID_PARAMETER);
        if (request.kind == HelpTargetKind::compiled_html) {
            if (!ExtensionIs(request.local_path, L".chm"))
                return Result(HelpStatus::untrusted_target, ERROR_INVALID_NAME);
        } else if (!ExtensionIs(request.local_path, L".html") &&
                   !ExtensionIs(request.local_path, L".htm")) {
            return Result(HelpStatus::untrusted_target, ERROR_INVALID_NAME);
        }
        std::error_code error;
        if (!std::filesystem::is_regular_file(request.local_path, error))
            return Result(HelpStatus::not_found,
                          error ? static_cast<DWORD>(error.value()) : ERROR_FILE_NOT_FOUND);
        return Result(HelpStatus::success);
    } catch (...) {
        return {HelpStatus::native_failure, ERROR_OUTOFMEMORY, std::current_exception()};
    }
}

HelpResult LaunchHelpWithBackend(HWND owner, const HelpRequest& request,
                                 const HelpLaunchBackend& backend) noexcept {
    if (owner && !::IsWindow(owner))
        return Result(HelpStatus::invalid_argument, ERROR_INVALID_WINDOW_HANDLE);
    const auto valid = ValidateHelpRequest(request);
    if (!valid) return valid;
    if (!backend) return Result(HelpStatus::invalid_argument, ERROR_INVALID_PARAMETER);
    try { return backend(owner, request); }
    catch (...) {
        return {HelpStatus::callback_failed, ERROR_UNHANDLED_EXCEPTION,
                std::current_exception()};
    }
}

HelpResult LaunchHelp(HWND owner, const HelpRequest& request) noexcept {
    return LaunchHelpWithBackend(owner, request, DefaultBackend);
}

}  // namespace mwfl
