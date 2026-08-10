#include <mwfl/layout.h>

#include <windows.h>

#include <chrono>
#include <cstdlib>
#include <vector>

namespace {

class WindowList final {
public:
    ~WindowList() {
        for (HWND window : windows_) if (window != nullptr) ::DestroyWindow(window);
    }
    void Add(HWND window) { windows_.push_back(window); }
private:
    std::vector<HWND> windows_;
};

}  // namespace

int main() {
    using mwfl::operator""_dip;
    constexpr int child_count = 100;
    WindowList lifetime;
    const HWND parent = ::CreateWindowExW(0, L"STATIC", L"layout-quality",
        WS_OVERLAPPED, 0, 0, 800, 1000, nullptr, nullptr,
        ::GetModuleHandleW(nullptr), nullptr);
    if (parent == nullptr) return EXIT_FAILURE;
    lifetime.Add(parent);

    mwfl::LayoutNode column = mwfl::Column().Margin(8.0_dip).Gap(1.0_dip);
    std::vector<HWND> children;
    children.reserve(child_count);
    for (int index = 0; index < child_count; ++index) {
        const HWND child = ::CreateWindowExW(0, L"STATIC", L"item", WS_CHILD,
            0, 0, 1, 1, parent, nullptr, ::GetModuleHandleW(nullptr), nullptr);
        if (child == nullptr) return EXIT_FAILURE;
        children.push_back(child);
        // The parent destroys child HWNDs; do not add them to WindowList.
        column.Add(child, mwfl::Stretch(1.0f, 1.0_dip));
    }

    mwfl::LayoutHost layout(std::move(column));
    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < 250; ++iteration) {
        if (!layout.Arrange(parent)) return EXIT_FAILURE;
    }
    const auto elapsed = std::chrono::steady_clock::now() - started;
    if (elapsed > std::chrono::seconds(3)) return EXIT_FAILURE;

    RECT parent_client{};
    if (::GetClientRect(parent, &parent_client) == FALSE) return EXIT_FAILURE;
    LONG previous_bottom = 0;
    for (HWND child : children) {
        RECT bounds{};
        if (::GetWindowRect(child, &bounds) == FALSE) return EXIT_FAILURE;
        ::MapWindowPoints(nullptr, parent, reinterpret_cast<POINT*>(&bounds), 2);
        if (bounds.left < parent_client.left || bounds.right > parent_client.right ||
            bounds.top < previous_bottom || bounds.bottom > parent_client.bottom ||
            bounds.right < bounds.left || bounds.bottom < bounds.top) {
            return EXIT_FAILURE;
        }
        previous_bottom = bounds.bottom;
    }
    return EXIT_SUCCESS;
}
