#pragma once

#include <windows.h>

#include <concepts>
#include <functional>
#include <source_location>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#include <mwfl/error.h>

namespace mwfl {

namespace detail {

template <typename Result>
concept DetailedCheckResult = requires(const Result& result) {
    { result.Succeeded() } -> std::same_as<bool>;
    result.completed;
    result.failed_at;
    result.native_result;
};

}  // namespace detail

inline void Must(
    bool succeeded,
    std::string_view operation = {},
    const std::source_location where = std::source_location::current()) {
    if (succeeded) return;
    throw Error(ERROR_GEN_FAILURE, operation, where);
}

template <typename Operation>
    requires std::invocable<Operation&> &&
        std::convertible_to<std::invoke_result_t<Operation&>, bool>
auto MustInvoke(
    Operation&& operation,
    std::string_view description = {},
    const std::source_location where = std::source_location::current()) {
    ::SetLastError(ERROR_SUCCESS);
    auto result = std::invoke(std::forward<Operation>(operation));
    if (static_cast<bool>(result)) return result;
    DWORD error = ::GetLastError();
    throw Error(detail::NormalizeLastError(error), description, where);
}

inline void Must(
    bool succeeded,
    DWORD native_error,
    std::string_view operation,
    const std::source_location where = std::source_location::current()) {
    if (!succeeded) {
        throw Error(detail::NormalizeLastError(native_error), operation, where);
    }
}

template <detail::DetailedCheckResult Result>
Result Must(
    Result result,
    std::string_view operation = {},
    const std::source_location where = std::source_location::current()) {
    if (result.Succeeded()) return result;
    std::ostringstream stream;
    stream << (operation.empty() ? "mwfl batch operation" : operation)
           << " failed at item " << result.failed_at
           << " after " << result.completed
           << " completed; native result " << result.native_result;
    throw Error(ERROR_GEN_FAILURE, stream.str(), where);
}

}  // namespace mwfl
