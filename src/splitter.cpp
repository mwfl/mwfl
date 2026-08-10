#include <mwfl/appearance.h>
#include <mwfl/splitter.h>

#include <windowsx.h>

#include <algorithm>
#include <cmath>

namespace mwfl {
namespace {

float Clean(float value) noexcept {
    return std::isfinite(value) ? (std::max)(value, 0.0f) : 0.0f;
}

}  // namespace

SplitterModel::SplitterModel(SplitterOrientation orientation,
                             SplitterConstraints constraints) noexcept
    : orientation_(orientation) {
    SetConstraints(constraints);
}

SplitterOrientation SplitterModel::GetOrientation() const noexcept {
    return orientation_;
}

SplitterConstraints SplitterModel::GetConstraints() const noexcept {
    return constraints_;
}

Dip SplitterModel::GetPosition() const noexcept {
    return position_;
}

void SplitterModel::SetOrientation(SplitterOrientation orientation) noexcept {
    orientation_ = orientation;
}

void SplitterModel::SetConstraints(SplitterConstraints constraints) noexcept {
    constraints_.minimum_first = Dip(Clean(constraints.minimum_first.value));
    constraints_.minimum_second = Dip(Clean(constraints.minimum_second.value));
    constraints_.bar_thickness = Dip(Clean(constraints.bar_thickness.value));
}

void SplitterModel::SetPosition(Dip position) noexcept {
    position_ = Dip(Clean(position.value));
}

void SplitterModel::Adjust(Dip delta) noexcept {
    const float value = std::isfinite(delta.value) ? delta.value : 0.0f;
    SetPosition(Dip(position_.value + value));
}

void SplitterModel::SetRatio(float ratio, SizeDip extent) noexcept {
    const float main_extent = orientation_ == SplitterOrientation::vertical
                                  ? Clean(extent.width.value)
                                  : Clean(extent.height.value);
    const float bar = (std::min)(Clean(constraints_.bar_thickness.value), main_extent);
    const float bounded_ratio = std::isfinite(ratio) ? std::clamp(ratio, 0.0f, 1.0f) : 0.5f;
    SetPosition(Dip((main_extent - bar) * bounded_ratio));
}

SplitterLayout SplitterModel::Arrange(SizeDip extent) noexcept {
    const float width = Clean(extent.width.value);
    const float height = Clean(extent.height.value);
    const float main_extent = orientation_ == SplitterOrientation::vertical ? width : height;
    const float bar = (std::min)(constraints_.bar_thickness.value, main_extent);
    const float available = main_extent - bar;
    const float minimum_first = constraints_.minimum_first.value;
    const float minimum_second = constraints_.minimum_second.value;
    const bool satisfied = minimum_first + minimum_second <= available;

    float first = 0.0f;
    if (satisfied) {
        first = std::clamp(position_.value, minimum_first, available - minimum_second);
    } else {
        const float minimum_sum = minimum_first + minimum_second;
        first = minimum_sum > 0.0f ? available * minimum_first / minimum_sum : available * 0.5f;
    }
    position_ = Dip(first);
    const float second = available - first;

    if (orientation_ == SplitterOrientation::vertical) {
        return {
            RectDip(0.0_dip, 0.0_dip, Dip(first), Dip(height)),
            RectDip(Dip(first), 0.0_dip, Dip(bar), Dip(height)),
            RectDip(Dip(first + bar), 0.0_dip, Dip(second), Dip(height)),
            satisfied,
        };
    }
    return {
        RectDip(0.0_dip, 0.0_dip, Dip(width), Dip(first)),
        RectDip(0.0_dip, Dip(first), Dip(width), Dip(bar)),
        RectDip(0.0_dip, Dip(first + bar), Dip(width), Dip(second)),
        satisfied,
    };
}

Splitter::~Splitter() noexcept {
    if (GetHwnd() != nullptr) {
        ::RemoveWindowSubclass(GetHwnd(), SubclassProcedure, 1);
    }
}

bool Splitter::Create(HWND parent, ControlId id, RectDip bounds, SplitterOptions options) {
    model_ = SplitterModel(options.orientation, options.constraints);
    model_.SetPosition(options.initial_position);
    keyboard_step_ = Dip(Clean(options.keyboard_step.value));
    first_ = nullptr;
    second_ = nullptr;
    dragging_ = false;
    if (options.orientation == SplitterOrientation::horizontal) {
        options.style |= TBS_VERT;
    } else {
        options.style &= ~static_cast<DWORD>(TBS_VERT);
    }
    if (!CreateNative(TRACKBAR_CLASSW, parent, id, L"Pane splitter", bounds, options.style,
                      options.extended_style)) {
        return false;
    }
    if (::SetWindowSubclass(GetHwnd(), SubclassProcedure, 1, reinterpret_cast<DWORD_PTR>(this)) ==
        FALSE) {
        const DWORD error = ::GetLastError();
        Destroy();
        ::SetLastError(error == ERROR_SUCCESS ? ERROR_FUNCTION_FAILED : error);
        return false;
    }
    if (!SetAccessibleName(GetHwnd(), L"Pane splitter")) {
        Destroy();
        ::SetLastError(ERROR_FUNCTION_FAILED);
        return false;
    }
    return true;
}

bool Splitter::AttachPanes(HWND first, HWND second) noexcept {
    const HWND host = GetHwnd();
    if (host == nullptr || first == nullptr || second == nullptr || first == second ||
        ::IsWindow(first) == FALSE || ::IsWindow(second) == FALSE || ::GetParent(first) != host ||
        ::GetParent(second) != host) {
        ::SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    DWORD first_process = 0;
    DWORD second_process = 0;
    const DWORD first_thread = ::GetWindowThreadProcessId(first, &first_process);
    const DWORD second_thread = ::GetWindowThreadProcessId(second, &second_process);
    if (first_thread != ::GetCurrentThreadId() || second_thread != first_thread ||
        first_process != ::GetCurrentProcessId() || second_process != first_process) {
        ::SetLastError(ERROR_INVALID_THREAD_ID);
        return false;
    }
    first_ = first;
    second_ = second;
    if (!Arrange()) {
        DetachPanes();
        return false;
    }
    return true;
}

void Splitter::DetachPanes() noexcept {
    first_ = nullptr;
    second_ = nullptr;
}

HWND Splitter::GetFirstPane() const noexcept {
    return first_ != nullptr && ::IsWindow(first_) != FALSE ? first_ : nullptr;
}

HWND Splitter::GetSecondPane() const noexcept {
    return second_ != nullptr && ::IsWindow(second_) != FALSE ? second_ : nullptr;
}

bool Splitter::SetBounds(RectDip bounds) noexcept {
    return NativeControl::SetBounds(bounds) && Arrange();
}

bool Splitter::Arrange() noexcept {
    const HWND host = GetHwnd();
    if (host == nullptr) {
        ::SetLastError(ERROR_INVALID_WINDOW_HANDLE);
        return false;
    }
    RECT client{};
    if (::GetClientRect(host, &client) == FALSE) return false;
    const DpiContext dpi = DpiContext::FromWindow(host);
    const SplitterLayout layout = model_.Arrange({
        dpi.FromPixels(client.right - client.left),
        dpi.FromPixels(client.bottom - client.top),
    });
    const int native_extent = model_.GetOrientation() == SplitterOrientation::vertical
                                  ? client.right - client.left
                                  : client.bottom - client.top;
    ::SendMessageW(host, TBM_SETRANGEMIN, FALSE, 0);
    ::SendMessageW(host, TBM_SETRANGEMAX, FALSE, native_extent);
    ::SendMessageW(host, TBM_SETPOS, TRUE, dpi.ToPixels(model_.GetPosition()));
    const HWND first = GetFirstPane();
    const HWND second = GetSecondPane();
    if ((first_ != nullptr && first == nullptr) || (second_ != nullptr && second == nullptr)) {
        DetachPanes();
        ::SetLastError(ERROR_INVALID_WINDOW_HANDLE);
        return false;
    }
    if (first != nullptr && second != nullptr) {
        const RECT first_bounds = dpi.ToPixels(layout.first);
        const RECT second_bounds = dpi.ToPixels(layout.second);
        HDWP batch = ::BeginDeferWindowPos(2);
        if (batch == nullptr) return false;
        batch =
            ::DeferWindowPos(batch, first, nullptr, first_bounds.left, first_bounds.top,
                             first_bounds.right - first_bounds.left,
                             first_bounds.bottom - first_bounds.top, SWP_NOACTIVATE | SWP_NOZORDER);
        if (batch == nullptr) return false;
        batch = ::DeferWindowPos(batch, second, nullptr, second_bounds.left, second_bounds.top,
                                 second_bounds.right - second_bounds.left,
                                 second_bounds.bottom - second_bounds.top,
                                 SWP_NOACTIVATE | SWP_NOZORDER);
        if (batch == nullptr || ::EndDeferWindowPos(batch) == FALSE) return false;
    }
    InvalidateBar();
    return true;
}

bool Splitter::SetPosition(Dip position) noexcept {
    model_.SetPosition(position);
    if (!Arrange()) return false;
    NotifyPositionChanged();
    return true;
}

Dip Splitter::GetPosition() const noexcept {
    return model_.GetPosition();
}

bool Splitter::SetConstraints(SplitterConstraints constraints) noexcept {
    model_.SetConstraints(constraints);
    return Arrange();
}

SplitterConstraints Splitter::GetConstraints() const noexcept {
    return model_.GetConstraints();
}

SplitterOrientation Splitter::GetOrientation() const noexcept {
    return model_.GetOrientation();
}

LRESULT CALLBACK Splitter::SubclassProcedure(HWND window, UINT message, WPARAM wparam,
                                             LPARAM lparam, UINT_PTR subclass_id,
                                             DWORD_PTR reference) noexcept {
    auto* self = reinterpret_cast<Splitter*>(reference);
    if (self == nullptr) {
        return ::DefSubclassProc(window, message, wparam, lparam);
    }
    const LRESULT result = self->ProcessMessage(window, message, wparam, lparam);
    if (message == WM_NCDESTROY) {
        ::RemoveWindowSubclass(window, SubclassProcedure, subclass_id);
        self->DetachPanes();
    }
    return result;
}

LRESULT Splitter::ProcessMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
    const auto pointer_position = [&]() noexcept {
        const DpiContext dpi = DpiContext::FromWindow(window);
        const int coordinate = GetOrientation() == SplitterOrientation::vertical
                                   ? GET_X_LPARAM(lparam)
                                   : GET_Y_LPARAM(lparam);
        return dpi.FromPixels(coordinate);
    };
    switch (message) {
        case WM_NOTIFY: {
            const auto* header = reinterpret_cast<const NMHDR*>(lparam);
            if (header != nullptr &&
                (header->hwndFrom == first_ || header->hwndFrom == second_)) {
                const HWND parent = ::GetParent(window);
                return parent == nullptr ? 0
                                         : ::SendMessageW(parent, message, wparam, lparam);
            }
            break;
        }
        case WM_COMMAND: {
            const HWND source = reinterpret_cast<HWND>(lparam);
            if (source != nullptr && (source == first_ || source == second_)) {
                const HWND parent = ::GetParent(window);
                return parent == nullptr ? 0
                                         : ::SendMessageW(parent, message, wparam, lparam);
            }
            break;
        }
        case WM_CONTEXTMENU: {
            const HWND source = reinterpret_cast<HWND>(wparam);
            if (source == first_ || source == second_) {
                const HWND parent = ::GetParent(window);
                return parent == nullptr ? 0
                                         : ::SendMessageW(parent, message, wparam, lparam);
            }
            break;
        }
        case WM_SIZE:
            static_cast<void>(Arrange());
            return 0;
        case WM_DPICHANGED_AFTERPARENT:
            static_cast<void>(Arrange());
            return 0;
        case WM_THEMECHANGED:
        case WM_SYSCOLORCHANGE:
        case WM_SETTINGCHANGE:
            InvalidateBar();
            break;
        case WM_LBUTTONDOWN: {
            const Dip pointer = pointer_position();
            const float bar_start = GetPosition().value;
            const float bar_end = bar_start + GetConstraints().bar_thickness.value;
            if (pointer.value >= bar_start && pointer.value <= bar_end) {
                dragging_ = true;
                drag_offset_ = Dip(pointer.value - bar_start);
                ::SetCapture(window);
                ::SetFocus(window);
            }
            return 0;
        }
        case WM_MOUSEMOVE:
            if (dragging_) {
                model_.SetPosition(pointer_position() - drag_offset_);
                if (Arrange()) NotifyPositionChanged();
            }
            return 0;
        case WM_LBUTTONUP:
            if (dragging_) {
                dragging_ = false;
                if (::GetCapture() == window) ::ReleaseCapture();
            }
            return 0;
        case WM_CAPTURECHANGED:
            dragging_ = false;
            return 0;
        case WM_GETDLGCODE:
            return DLGC_WANTARROWS;
        case WM_KEYDOWN: {
            float delta = 0.0f;
            const bool vertical = GetOrientation() == SplitterOrientation::vertical;
            if ((vertical && wparam == VK_LEFT) || (!vertical && wparam == VK_UP)) {
                delta = -keyboard_step_.value;
            } else if ((vertical && wparam == VK_RIGHT) || (!vertical && wparam == VK_DOWN)) {
                delta = keyboard_step_.value;
            } else if (wparam == VK_HOME) {
                model_.SetPosition(Dip(0.0f));
            } else if (wparam == VK_END) {
                model_.SetPosition(Dip(1000000.0f));
            } else {
                return ::DefSubclassProc(window, message, wparam, lparam);
            }
            if (delta != 0.0f) model_.Adjust(Dip(delta));
            if (Arrange()) NotifyPositionChanged();
            return 0;
        }
        case WM_SETCURSOR:
            if (LOWORD(lparam) == HTCLIENT) {
                ::SetCursor(::LoadCursorW(nullptr, GetOrientation() == SplitterOrientation::vertical
                                                       ? IDC_SIZEWE
                                                       : IDC_SIZENS));
                return TRUE;
            }
            break;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            const HDC dc = ::BeginPaint(window, &paint);
            RECT client{};
            ::GetClientRect(window, &client);
            const DpiContext dpi = DpiContext::FromWindow(window);
            const SplitterLayout layout =
                model_.Arrange({dpi.FromPixels(client.right), dpi.FromPixels(client.bottom)});
            RECT bar = dpi.ToPixels(layout.bar);
            ::FillRect(dc, &bar, ::GetSysColorBrush(COLOR_3DFACE));
            if (::GetFocus() == window) ::DrawFocusRect(dc, &bar);
            ::EndPaint(window, &paint);
            return 0;
        }
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            InvalidateBar();
            return 0;
        default:
            break;
    }
    return ::DefSubclassProc(window, message, wparam, lparam);
}

void Splitter::NotifyPositionChanged() noexcept {
    const HWND window = GetHwnd();
    const HWND parent = window == nullptr ? nullptr : ::GetParent(window);
    if (parent == nullptr) return;
    SplitterNotification notification{
        {window, static_cast<UINT_PTR>(GetId().value), kSplitterPositionChanged},
        GetPosition(),
    };
    ::SendMessageW(parent, WM_NOTIFY, notification.header.idFrom,
                   reinterpret_cast<LPARAM>(&notification));
}

void Splitter::InvalidateBar() noexcept {
    const HWND window = GetHwnd();
    if (window != nullptr) ::InvalidateRect(window, nullptr, FALSE);
}

}  // namespace mwfl
