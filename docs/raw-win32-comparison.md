# Raw Win32 and mwfl, side by side

mwfl keeps native HWND controls and the Win32 message model while removing the repetitive setup code around them. This comparison builds the same small window: a label, a button, a vertical layout, and a close action.

## Raw Win32

```cpp
LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    static HWND label = nullptr;
    static HWND close = nullptr;

    switch (message) {
    case WM_CREATE:
        label = CreateWindowExW(0, L"STATIC", L"Ready", WS_CHILD | WS_VISIBLE,
            20, 20, 280, 24, hwnd, nullptr, nullptr, nullptr);
        close = CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE,
            20, 56, 100, 36, hwnd, reinterpret_cast<HMENU>(1001), nullptr, nullptr);
        return 0;
    case WM_SIZE:
        MoveWindow(label, 20, 20, LOWORD(lparam) - 40, 24, TRUE);
        MoveWindow(close, 20, HIWORD(lparam) - 56, 100, 36, TRUE);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wparam) == 1001 && HIWORD(wparam) == BN_CLICKED) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}
```

This is only the callback. A complete program still needs class registration, window creation, error handling, DPI decisions, a message loop, and explicit lifetime management.

## The same window with mwfl

```cpp
#include <mwfl/mwfl.h>

using mwfl::operator""_dip;

class MainWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        mwfl::ControlHost ui{*this};
        ui.Add(message_, L"Ready");
        ui.Add(close_, L"Close");
        SetLayout(mwfl::Column().Margin(20.0_dip).Gap(10.0_dip)
            .Add(message_, mwfl::Stretch())
            .Add(close_, mwfl::Fixed(36.0_dip)));
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (event.IsClicked(close_)) {
            Close();
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

private:
    mwfl::Label message_;
    mwfl::Button close_;
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    return mwfl::RunApplication<MainWindow>(instance, show);
}
```

The controls are still real HWNDs. `GetHwnd()` remains available when an application needs direct Win32 behavior, and unhandled events continue through the native path.

## What mwfl owns—and what it does not

| Concern | Raw Win32 | mwfl |
|---|---|---|
| Controls | Real HWNDs | Real HWNDs |
| Message dispatch | Application callback and manual ID routing | Typed handlers with explicit propagation |
| Layout | Pixel calculations in `WM_SIZE` | Retained Row/Column/Overlay layouts in DIPs |
| Lifetime | Manual handle ownership | RAII wrappers with explicit ownership |
| Error boundary | Application-defined | No exception crosses a Win32 callback |
| Escape hatch | Native API | `GetHwnd()` and native API |
| Rendering engine | Windows | Windows |

mwfl is not a cross-platform renderer, virtual DOM, or replacement operating-system UI. It is a modern C++20 foundation for applications that deliberately want native Windows behavior.

Next: copy [`templates/basic-app`](../templates/basic-app) or follow the [agent usage guide](agent-usage.md).
