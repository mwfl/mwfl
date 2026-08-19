#include <mwfl/app_support/update_checker.h>

#include <array>
#include <cwctype>

namespace mwfl::app_support {
namespace {
std::array<unsigned long, 4> VersionParts(std::wstring_view version) noexcept {
    std::array<unsigned long, 4> parts{};
    if (!version.empty() && (version.front() == L'v' || version.front() == L'V'))
        version.remove_prefix(1);
    std::size_t part = 0;
    std::size_t position = 0;
    while (part < parts.size() && position < version.size()) {
        if (!std::iswdigit(version[position])) break;
        unsigned long value = 0;
        while (position < version.size() && std::iswdigit(version[position])) {
            const unsigned digit = static_cast<unsigned>(version[position] - L'0');
            value = value > 100000000UL ? 1000000000UL : value * 10UL + digit;
            ++position;
        }
        parts[part++] = value;
        if (position >= version.size() || version[position] != L'.') break;
        ++position;
    }
    return parts;
}
}  // namespace

bool IsVersionNewer(std::wstring_view latest, std::wstring_view current) noexcept {
    return VersionParts(latest) > VersionParts(current);
}

}  // namespace mwfl::app_support
