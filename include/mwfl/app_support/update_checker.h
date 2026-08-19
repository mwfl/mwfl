#pragma once

#include <windows.h>

#include <memory>
#include <string>
#include <string_view>

namespace mwfl::app_support {

struct UpdateCheckerOptions {
    std::wstring application_name;
    std::wstring repository;
    std::wstring current_version;
    std::wstring settings_key;
};

// Adds update commands to the host menu and performs rate-limited checks against
// the repository's latest stable GitHub Release. The host must outlive this object.
class UpdateChecker final {
public:
    UpdateChecker();
    ~UpdateChecker();

    UpdateChecker(const UpdateChecker&) = delete;
    UpdateChecker& operator=(const UpdateChecker&) = delete;

    void Attach(HWND host, UpdateCheckerOptions options, bool allow_automatic_check = true);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] bool IsVersionNewer(std::wstring_view latest,
                                  std::wstring_view current) noexcept;

}  // namespace mwfl::app_support
