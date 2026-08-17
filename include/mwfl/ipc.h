#pragma once
#include <mwfl/core.h>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>
namespace mwfl {
struct PipeOptions {
    std::wstring name;
    std::uint32_t maximum_frame_size = 1024 * 1024;
};
class PipeConnection final {
public:
    PipeConnection() = default;
    PipeConnection(KernelHandle pipe, std::uint32_t maximum_frame_size) noexcept;
    PipeConnection(PipeConnection&&) noexcept = default;
    PipeConnection& operator=(PipeConnection&&) noexcept = default;
    PipeConnection(const PipeConnection&) = delete;
    PipeConnection& operator=(const PipeConnection&) = delete;
    [[nodiscard]] Result<void> WriteFrame(std::span<const std::byte> payload);
    [[nodiscard]] Result<std::vector<std::byte>> ReadFrame();
private:
    KernelHandle pipe_;
    std::uint32_t maximum_frame_size_ = 0;
};
class PipeServer final {
public:
    explicit PipeServer(PipeOptions options) : options_(std::move(options)) {}
    [[nodiscard]] Result<PipeConnection> Accept() const;
private:
    PipeOptions options_;
};
[[nodiscard]] Result<PipeConnection> ConnectPipe(const PipeOptions& options);
}  // namespace mwfl
