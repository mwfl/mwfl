#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
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
struct PipeEndpoint {
    std::wstring name;
    std::uint32_t maximum_frame_size = 1024u * 1024u;
    PipeAccessPolicy access;
};
struct PipePeer {
    DWORD process_id = 0;
    DWORD session_id = 0;
    std::wstring user_sid;
};
class PipeConnection;
class PipeReader final {
   public:
    PipeReader() = default;
    [[nodiscard]] Result<OperationOutcome<std::vector<std::byte>>> ReadFrame(
        Deadline deadline, std::stop_token stop = {});
   private:
    friend class PipeConnection;
    explicit PipeReader(std::shared_ptr<PipeConnection> connection) : connection_(std::move(connection)) {}
    std::shared_ptr<PipeConnection> connection_;
};
class PipeWriter final {
   public:
    PipeWriter() = default;
    [[nodiscard]] Result<OperationOutcome<void>> WriteFrame(
        std::span<const std::byte> payload, Deadline deadline, std::stop_token stop = {});
   private:
    friend class PipeConnection;
    explicit PipeWriter(std::shared_ptr<PipeConnection> connection) : connection_(std::move(connection)) {}
    std::shared_ptr<PipeConnection> connection_;
};
struct PipeChannels final { PipeReader reader; PipeWriter writer; };
class ScopedPipeImpersonation;
class PipeConnection final {
   public:
    PipeConnection() = default;
    PipeConnection(KernelHandle pipe, std::uint32_t maximum_frame_size, bool server_end) noexcept;
    PipeConnection(PipeConnection&& other) noexcept;
    PipeConnection& operator=(PipeConnection&& other) noexcept;
    PipeConnection(const PipeConnection&) = delete;
    PipeConnection& operator=(const PipeConnection&) = delete;
    [[nodiscard]] Result<PipePeer> QueryPeer() const;
    [[nodiscard]] bool IsServerEnd() const noexcept { return server_end_; }
    [[nodiscard]] PipeChannels Split() &&;
   private:
    friend class PipeReader;
    friend class PipeWriter;
    friend class ScopedPipeImpersonation;
    [[nodiscard]] Result<OperationOutcome<void>> WriteFrame(
        std::span<const std::byte> payload, Deadline deadline, std::stop_token stop);
    [[nodiscard]] Result<OperationOutcome<std::vector<std::byte>>> ReadFrame(
        Deadline deadline, std::stop_token stop);
    KernelHandle pipe_;
    std::uint32_t maximum_frame_size_ = 0;
    bool server_end_ = false;
    std::atomic_flag reading_ = ATOMIC_FLAG_INIT;
    std::atomic_flag writing_ = ATOMIC_FLAG_INIT;
};
class PipeListener final {
   public:
    PipeListener() = default;
    PipeListener(PipeListener&&) noexcept = default;
    PipeListener& operator=(PipeListener&&) noexcept = default;
    PipeListener(const PipeListener&) = delete;
    PipeListener& operator=(const PipeListener&) = delete;
    [[nodiscard]] static Result<PipeListener> Create(PipeEndpoint endpoint);
    [[nodiscard]] Result<OperationOutcome<PipeConnection>> Accept(
        Deadline deadline, std::stop_token stop = {});
   private:
    PipeListener(PipeEndpoint endpoint, KernelHandle listener)
        : endpoint_(std::move(endpoint)), listener_(std::move(listener)) {}
    PipeEndpoint endpoint_;
    KernelHandle listener_;
};
[[nodiscard]] Result<OperationOutcome<PipeConnection>> ConnectPipe(
    const PipeEndpoint& endpoint, Deadline deadline, std::stop_token stop = {});
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
