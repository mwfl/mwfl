#pragma once
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>
namespace example { inline constexpr unsigned ProtocolVersion = 1; std::vector<std::byte> Encode(std::string_view command); bool Is(std::span<const std::byte> frame, std::string_view command); }
