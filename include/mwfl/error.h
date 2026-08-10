#pragma once

#include <windows.h>

#include <source_location>
#include <string>
#include <string_view>
#include <system_error>

namespace mwfl {

class Error final : public std::system_error {
public:
    Error(DWORD native_error, std::string_view operation,
          std::source_location where = std::source_location::current());

    std::string_view Operation() const noexcept { return operation_; }
    const std::source_location& Location() const noexcept { return location_; }

private:
    std::string operation_;
    std::source_location location_;
};

namespace detail {
std::string DescribeOperation(
    std::string_view operation, const std::source_location& where);
constexpr DWORD NormalizeLastError(DWORD error) noexcept {
    return error == ERROR_SUCCESS ? ERROR_GEN_FAILURE : error;
}
}  // namespace detail
}  // namespace mwfl
