#pragma once
#include <mwfl/core.h>
#include <string>
#include <string_view>
namespace mwfl {
struct PackageIdentity { bool packaged = false; std::wstring full_name; };
[[nodiscard]] Result<PackageIdentity> QueryCurrentPackageIdentity();
[[nodiscard]] Result<void> RegisterApplicationRestart(std::wstring_view arguments, DWORD flags = 0);
[[nodiscard]] Result<void> UnregisterApplicationRestart() noexcept;
class RestartRegistration final {
public:
    RestartRegistration() = default;
    explicit RestartRegistration(std::wstring_view arguments, DWORD flags = 0);
    ~RestartRegistration();
    RestartRegistration(const RestartRegistration&) = delete;
    RestartRegistration& operator=(const RestartRegistration&) = delete;
    RestartRegistration(RestartRegistration&& other) noexcept;
    RestartRegistration& operator=(RestartRegistration&& other) noexcept;
    [[nodiscard]] bool Active() const noexcept { return active_; }
private:
    bool active_ = false;
};
}  // namespace mwfl
