#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <mwfl/docking_workspace.h>

namespace mwfl {

inline constexpr std::uint32_t docking_session_schema_version = 1;

struct DockingSessionLimits {
    std::size_t maximum_bytes = 4 * 1024 * 1024;
    std::size_t maximum_panels = 4096;
    std::size_t maximum_groups = 4096;
    std::size_t maximum_nodes = 8192;
    std::size_t maximum_floating_hosts = 1024;
    std::size_t maximum_auto_hide = 4096;
    std::size_t maximum_string_units = 32 * 1024;
};

enum class DockingSessionStatus {
    success,
    not_found,
    unsupported_version,
    malformed,
    oversized,
    duplicate_identity,
    invalid_graph,
    io_error,
};

struct DockingSessionResult {
    DockingSessionStatus status = DockingSessionStatus::malformed;
    std::optional<DockLayoutSnapshot> snapshot = std::nullopt;
    DWORD native_error = ERROR_SUCCESS;
    explicit operator bool() const noexcept {
        return status == DockingSessionStatus::success && snapshot.has_value();
    }
};

std::wstring SerializeDockingSession(const DockLayoutSnapshot& snapshot);
DockingSessionResult ParseDockingSession(
    std::wstring_view text, const DockingSessionLimits& limits = {});
DockingSessionResult LoadDockingSession(
    const std::filesystem::path& path, const DockingSessionLimits& limits = {});
DockingSessionStatus SaveDockingSessionAtomic(
    const std::filesystem::path& path, const DockLayoutSnapshot& snapshot,
    const DockingSessionLimits& limits = {});

}  // namespace mwfl
