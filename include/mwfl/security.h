#pragma once
#include <mwfl/core.h>
#include <cstddef>
#include <span>
#include <vector>
namespace mwfl {
class SecureBytes final {
public:
    SecureBytes() = default;
    explicit SecureBytes(std::vector<std::byte> bytes) : bytes_(std::move(bytes)) {}
    ~SecureBytes();
    SecureBytes(SecureBytes&& other) noexcept;
    SecureBytes& operator=(SecureBytes&& other) noexcept;
    SecureBytes(const SecureBytes&) = delete;
    SecureBytes& operator=(const SecureBytes&) = delete;
    [[nodiscard]] std::span<const std::byte> View() const noexcept { return bytes_; }
    [[nodiscard]] std::size_t Size() const noexcept { return bytes_.size(); }
    void Clear() noexcept;
private:
    std::vector<std::byte> bytes_;
};
using ProtectedData = std::vector<std::byte>;
[[nodiscard]] Result<ProtectedData> ProtectForCurrentUser(std::span<const std::byte> plaintext, std::wstring_view purpose = {});
[[nodiscard]] Result<SecureBytes> UnprotectForCurrentUser(std::span<const std::byte> protected_data, std::wstring_view purpose = {});
}  // namespace mwfl
