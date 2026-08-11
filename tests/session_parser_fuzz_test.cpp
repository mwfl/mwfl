#include <mwfl/docking_session.h>
#include <mwfl/document_session.h>

#include <array>
#include <cstdint>
#include <string>

namespace {

std::uint32_t Next(std::uint32_t& state) noexcept {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

bool Valid(mwfl::DocumentSessionStatus status) noexcept {
    using enum mwfl::DocumentSessionStatus;
    return status >= success && status <= io_error;
}

bool Valid(mwfl::DockingSessionStatus status) noexcept {
    using enum mwfl::DockingSessionStatus;
    return status >= success && status <= io_error;
}

}  // namespace

int main() {
    constexpr std::array<std::wstring_view, 8> seeds{
        L"", L"MWFL_DOCUMENT_SESSION 1 0\n", L"MWFL_DOCK_LAYOUT 1\n",
        L"MWFL_DOCUMENT_SESSION 999999999 999999999\n",
        L"MWFL_DOCK_LAYOUT 1\nR 1\n", L"\0\0\0", L"\r\nX \"\\\n", L"99999999999999999999"};
    std::uint32_t random = 0x4d57464cU;
    for (int iteration = 0; iteration < 20000; ++iteration) {
        std::wstring input{seeds[Next(random) % seeds.size()]};
        const auto mutations = 1U + Next(random) % 24U;
        for (std::uint32_t mutation = 0; mutation < mutations; ++mutation) {
            const auto operation = Next(random) % 3U;
            if (operation == 0 && !input.empty()) {
                input.erase(Next(random) % input.size(), 1);
            } else if (operation == 1 && input.size() < 4096) {
                const auto position = input.empty() ? 0 : Next(random) % (input.size() + 1);
                input.insert(input.begin() + static_cast<std::ptrdiff_t>(position),
                             static_cast<wchar_t>(Next(random) & 0xffffU));
            } else if (!input.empty()) {
                input[Next(random) % input.size()] =
                    static_cast<wchar_t>(Next(random) & 0xffffU);
            }
        }
        if (!Valid(mwfl::ParseDocumentSession(input).status) ||
            !Valid(mwfl::ParseDockingSession(input).status)) return 1;
    }
    return 0;
}
