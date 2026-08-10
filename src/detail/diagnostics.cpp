#include "detail/diagnostics.h"

#include <cstdio>
#include <cwchar>

namespace mwfl::detail {
namespace {

bool TestDialogsDisabled() noexcept {
#ifdef MWFL_TESTING
    wchar_t value[2]{};
    return ::GetEnvironmentVariableW(
        L"MWFL_TEST_NO_DIALOGS", value, _countof(value)) != 0;
#else
    return false;
#endif
}

bool ShouldShowUserDialog(bool requested) noexcept {
    return requested && !TestDialogsDisabled();
}

void Emit(const wchar_t* text, bool show_user) noexcept {
    ::OutputDebugStringW(text);
    if (TestDialogsDisabled()) {
        std::fputws(text, stderr);
        std::fflush(stderr);
    }
    if (ShouldShowUserDialog(show_user)) {
        ::MessageBoxW(nullptr, text, L"mwfl startup error", MB_OK | MB_ICONERROR | MB_TASKMODAL);
    }
}

void FormatSystemMessage(DWORD error, wchar_t* buffer, DWORD count) noexcept {
    if (count == 0) {
        return;
    }
    buffer[0] = L'\0';
    const DWORD written = ::FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        0,
        buffer,
        count,
        nullptr);
    if (written == 0) {
        _snwprintf_s(buffer, count, _TRUNCATE, L"No system message is available.");
    }
}

void WidenDescription(const char* source, wchar_t* destination, int count) noexcept {
    if (count <= 0) {
        return;
    }
    destination[0] = L'\0';
    if (source == nullptr) {
        return;
    }
    if (::MultiByteToWideChar(CP_UTF8, 0, source, -1, destination, count) == 0) {
        _snwprintf_s(destination, count, _TRUNCATE, L"Exception text was not valid UTF-8.");
    }
}

}  // namespace

void ReportHresult(const wchar_t* stage, HRESULT result, bool show_user) noexcept {
    const DWORD original_error = ::GetLastError();
    wchar_t system_message[384]{};
    wchar_t output[640]{};
    FormatSystemMessage(static_cast<DWORD>(result), system_message, _countof(system_message));
    _snwprintf_s(
        output,
        _countof(output),
        _TRUNCATE,
        L"mwfl: %s failed (HRESULT 0x%08lX): %s\r\n",
        stage,
        static_cast<unsigned long>(result),
        system_message);
    Emit(output, show_user);
    ::SetLastError(original_error);
}

void ReportWin32(const wchar_t* stage, DWORD error, bool show_user) noexcept {
    const DWORD original_error = ::GetLastError();
    wchar_t system_message[384]{};
    wchar_t output[640]{};
    FormatSystemMessage(error, system_message, _countof(system_message));
    _snwprintf_s(
        output,
        _countof(output),
        _TRUNCATE,
        L"mwfl: %s failed (Win32 %lu): %s\r\n",
        stage,
        static_cast<unsigned long>(error),
        system_message);
    Emit(output, show_user);
    ::SetLastError(original_error);
}

void ReportException(
    const wchar_t* stage,
    UINT message,
    const char* description,
    bool show_user) noexcept {
    const DWORD original_error = ::GetLastError();
    wchar_t wide_description[320]{};
    wchar_t output[640]{};
    WidenDescription(description, wide_description, _countof(wide_description));
    _snwprintf_s(
        output,
        _countof(output),
        _TRUNCATE,
        L"mwfl: C++ std::exception during %s (message 0x%04X): %s\r\n",
        stage,
        message,
        wide_description);
    Emit(output, show_user);
    ::SetLastError(original_error);
}

void ReportUnknownException(const wchar_t* stage, UINT message, bool show_user) noexcept {
    const DWORD original_error = ::GetLastError();
    wchar_t output[384]{};
    _snwprintf_s(
        output,
        _countof(output),
        _TRUNCATE,
        L"mwfl: unknown C++ exception during %s (message 0x%04X).\r\n",
        stage,
        message);
    Emit(output, show_user);
    ::SetLastError(original_error);
}

}  // namespace mwfl::detail
