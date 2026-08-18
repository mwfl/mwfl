#pragma once
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mwfl/core.h>
#include <span>
#include <stop_token>
#include <string>
#include <vector>
namespace mwfl {
inline constexpr std::uint32_t MaximumPipeFrameSize = 64u * 1024u * 1024u;
enum class PipeAccessPolicyKind { CurrentUser, CurrentUserAndLocalSystem, ExplicitSids };
struct PipeAccessPolicy {
    PipeAccessPolicyKind kind = PipeAccessPolicyKind::CurrentUser;
    std::vector<std::wstring> allowed_sids;
    [[nodiscard]] static PipeAccessPolicy CurrentUser() { return {}; }
    [[nodiscard]] static PipeAccessPolicy CurrentUserAndLocalSystem() {
        return {PipeAccessPolicyKind::CurrentUserAndLocalSystem, {}};
    }
    [[nodiscard]] static PipeAccessPolicy Explicit(std::vector<std::wstring> sids) {
        return {PipeAccessPolicyKind::ExplicitSids, std::move(sids)};
    }
};
struct PipeOptions {
    std::wstring name;
    std::uint32_t maximum_frame_size = 1024u * 1024u;
    PipeAccessPolicy access;
};
enum class PipeOperationStatus { Completed, TimedOut, Cancelled, Disconnected };
struct PipeWriteResult {
    PipeOperationStatus status = PipeOperationStatus::Completed;
};
struct PipeReadResult {
    PipeOperationStatus status = PipeOperationStatus::Completed;
    std::vector<std::byte> payload;
};
struct PipePeerIdentity {
    DWORD process_id = 0;
    DWORD session_id = 0;
    std::wstring user_sid;
};
class ScopedPipeImpersonation;
class PipeConnection final {
   public:
    PipeConnection() = default;
    PipeConnection(KernelHandle pipe, std::uint32_t maximum_frame_size, bool server_end) noexcept;
    PipeConnection(PipeConnection&& other) noexcept;
    PipeConnection& operator=(PipeConnection&& other) noexcept;
    PipeConnection(const PipeConnection&) = delete;
    PipeConnection& operator=(const PipeConnection&) = delete;
    [[nodiscard]] Result<PipeWriteResult> WriteFrame(
        std::span<const std::byte> payload,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{-1},
        std::stop_token stop = {});
    [[nodiscard]] Result<PipeReadResult> ReadFrame(
        std::chrono::milliseconds timeout = std::chrono::milliseconds{-1},
        std::stop_token stop = {});
    [[nodiscard]] Result<PipePeerIdentity> QueryPeerIdentity() const;
    [[nodiscard]] bool IsServerEnd() const noexcept { return server_end_; }

   private:
    friend class ScopedPipeImpersonation;
    KernelHandle pipe_;
    std::uint32_t maximum_frame_size_ = 0;
    bool server_end_ = false;
    std::atomic_flag reading_ = ATOMIC_FLAG_INIT;
    std::atomic_flag writing_ = ATOMIC_FLAG_INIT;
};
struct PipeAcceptResult {
    PipeOperationStatus status = PipeOperationStatus::Completed;
    PipeConnection connection;
};
struct PipeConnectResult {
    PipeOperationStatus status = PipeOperationStatus::Completed;
    PipeConnection connection;
};
class PipeServer final {
   public:
    PipeServer() = default;
    PipeServer(PipeServer&&) noexcept = default;
    PipeServer& operator=(PipeServer&&) noexcept = default;
    PipeServer(const PipeServer&) = delete;
    PipeServer& operator=(const PipeServer&) = delete;
    [[nodiscard]] static Result<PipeServer> Create(PipeOptions options);
    [[nodiscard]] Result<PipeAcceptResult> Accept(
        std::chrono::milliseconds timeout = std::chrono::milliseconds{-1},
        std::stop_token stop = {});

   private:
    PipeServer(PipeOptions options, KernelHandle listener)
        : options_(std::move(options)), listener_(std::move(listener)) {}
    PipeOptions options_;
    KernelHandle listener_;
};
[[nodiscard]] Result<PipeConnectResult> ConnectPipe(
    const PipeOptions& options, std::chrono::milliseconds timeout = std::chrono::milliseconds{-1},
    std::stop_token stop = {});
class ScopedPipeImpersonation final {
   public:
    ScopedPipeImpersonation() = default;
    ~ScopedPipeImpersonation();
    ScopedPipeImpersonation(ScopedPipeImpersonation&& other) noexcept;
    ScopedPipeImpersonation& operator=(ScopedPipeImpersonation&& other) noexcept;
    ScopedPipeImpersonation(const ScopedPipeImpersonation&) = delete;
    ScopedPipeImpersonation& operator=(const ScopedPipeImpersonation&) = delete;
    [[nodiscard]] static Result<ScopedPipeImpersonation> Create(PipeConnection& connection);
    [[nodiscard]] bool Active() const noexcept { return active_; }

   private:
    explicit ScopedPipeImpersonation(bool active) : active_(active) {}
    bool active_ = false;
};
}  // namespace mwfl
