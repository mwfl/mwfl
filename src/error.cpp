#include <mwfl/error.h>

namespace mwfl {
namespace detail {

std::string DescribeOperation(
    std::string_view operation, const std::source_location& where) {
    std::string description(operation.empty() ? "mwfl operation" : operation);
    description += " at ";
    description += where.file_name();
    description += ':';
    description += std::to_string(where.line());
    return description;
}

}  // namespace detail

Error::Error(DWORD native_error, std::string_view operation,
             std::source_location where)
    : std::system_error(
          static_cast<int>(detail::NormalizeLastError(native_error)),
          std::system_category(), detail::DescribeOperation(operation, where)),
      operation_(operation), location_(where) {}

}  // namespace mwfl
