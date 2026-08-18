#pragma once
#include <chrono>
#include <filesystem>
#include <functional>
#include <mwfl/core.h>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>
namespace mwfl {
struct Version {
    std::uint16_t major = 0, minor = 0, build = 0, revision = 0;
    friend auto operator<=>(const Version&, const Version&) = default;
};
struct PackageIdentity {
    bool packaged = false;
    std::wstring full_name;
    std::wstring family_name;
    std::wstring application_user_model_id;
    Version version;
};
[[nodiscard]] Result<PackageIdentity> QueryCurrentPackageIdentity();
[[nodiscard]] Result<Version> QueryFileVersion(const std::filesystem::path& path);
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
using RecoveryCallback = std::function<Result<void>(std::stop_token)>;
class RecoveryRegistration final {
   public:
    RecoveryRegistration() = default;
    ~RecoveryRegistration();
    RecoveryRegistration(RecoveryRegistration&& other) noexcept;
    RecoveryRegistration& operator=(RecoveryRegistration&& other) noexcept;
    RecoveryRegistration(const RecoveryRegistration&) = delete;
    RecoveryRegistration& operator=(const RecoveryRegistration&) = delete;
    [[nodiscard]] static Result<RecoveryRegistration> Register(
        RecoveryCallback callback, std::chrono::milliseconds ping_interval);
    [[nodiscard]] bool Active() const noexcept { return active_; }

   private:
    explicit RecoveryRegistration(bool active) : active_(active) {}
    bool active_ = false;
};
enum class SignatureStatus { Valid, Unsigned, Untrusted, Invalid, RevocationUnavailable };
enum class RevocationPolicy { Offline, Online };
struct SignatureVerification {
    SignatureStatus status = SignatureStatus::Invalid;
    LONG native_status = 0;
};
[[nodiscard]] Result<SignatureVerification> VerifyAuthenticode(
    const std::filesystem::path& path, RevocationPolicy policy = RevocationPolicy::Offline);
struct UpdateVerificationPolicy {
    bool require_valid_signature = true;
    RevocationPolicy revocation = RevocationPolicy::Offline;
    Version minimum_version{};
};
class VerifiedUpdate final {
   public:
    VerifiedUpdate() = default;
    VerifiedUpdate(VerifiedUpdate&&) noexcept = default;
    VerifiedUpdate& operator=(VerifiedUpdate&&) noexcept = default;
    VerifiedUpdate(const VerifiedUpdate&) = delete;
    VerifiedUpdate& operator=(const VerifiedUpdate&) = delete;
    [[nodiscard]] const std::filesystem::path& Candidate() const noexcept { return candidate_; }
    [[nodiscard]] const std::filesystem::path& Target() const noexcept { return target_; }
    [[nodiscard]] const std::filesystem::path& Backup() const noexcept { return backup_; }
    [[nodiscard]] const std::filesystem::path& RestartExecutable() const noexcept {
        return restart_executable_;
    }
    [[nodiscard]] const std::vector<std::wstring>& RestartArguments() const noexcept {
        return restart_arguments_;
    }
    [[nodiscard]] const UpdateVerificationPolicy& Policy() const noexcept { return policy_; }

   private:
    friend Result<VerifiedUpdate> VerifyUpdate(
        const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&,
        const std::filesystem::path&, std::vector<std::wstring>, const UpdateVerificationPolicy&);
    std::filesystem::path candidate_, target_, backup_, restart_executable_;
    std::vector<std::wstring> restart_arguments_;
    UpdateVerificationPolicy policy_;
};
class StagedUpdate final {
   public:
    StagedUpdate() = default;
    ~StagedUpdate();
    StagedUpdate(StagedUpdate&& other) noexcept;
    StagedUpdate& operator=(StagedUpdate&& other) noexcept;
    StagedUpdate(const StagedUpdate&) = delete;
    StagedUpdate& operator=(const StagedUpdate&) = delete;
    [[nodiscard]] const std::filesystem::path& StagingPath() const noexcept { return staging_; }
   private:
    friend Result<StagedUpdate> StageUpdate(VerifiedUpdate);
    friend Result<OperationOutcome<struct AppliedUpdate>> ApplyUpdate(
        StagedUpdate, DWORD, Deadline, std::stop_token);
    VerifiedUpdate verified_;
    std::filesystem::path staging_;
    bool active_ = false;
};
struct AppliedUpdate {
    std::filesystem::path target;
    std::filesystem::path backup;
};
[[nodiscard]] Result<VerifiedUpdate> VerifyUpdate(
    const std::filesystem::path& candidate, const std::filesystem::path& target,
    const std::filesystem::path& backup, const std::filesystem::path& restart_executable,
    std::vector<std::wstring> restart_arguments, const UpdateVerificationPolicy& policy = {});
[[nodiscard]] Result<StagedUpdate> StageUpdate(VerifiedUpdate verified);
[[nodiscard]] Result<OperationOutcome<AppliedUpdate>> ApplyUpdate(
    StagedUpdate staged, DWORD target_process_id, Deadline deadline, std::stop_token stop = {});
}  // namespace mwfl
