#pragma once

#include <windows.h>

namespace mwfl::detail {

void ReportHresult(const wchar_t* stage, HRESULT result, bool show_user) noexcept;
void ReportWin32(const wchar_t* stage, DWORD error, bool show_user) noexcept;
void ReportException(
    const wchar_t* stage,
    UINT message,
    const char* description,
    bool show_user) noexcept;
void ReportUnknownException(const wchar_t* stage, UINT message, bool show_user) noexcept;

}  // namespace mwfl::detail
