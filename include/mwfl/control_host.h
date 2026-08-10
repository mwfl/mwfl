#pragma once

#include <windows.h>

#include <string_view>
#include <system_error>
#include <utility>

#include <mwfl/dpi.h>
#include <mwfl/concepts.h>
#include <mwfl/events.h>
#include <mwfl/error.h>

namespace mwfl {

// Binds repeated control creation to one parent. IDs are allocated automatically
// unless the caller supplies one explicitly.
class ControlHost final {
public:
    explicit ControlHost(HWND parent, ControlId first_id = {0x4000}) noexcept
        : parent_(parent), next_id_(first_id.value) {}

    template <WindowLike Parent>
    explicit ControlHost(const Parent& parent, ControlId first_id = {0x4000}) noexcept
        : parent_(parent.GetHwnd()), next_id_(first_id.value) {}

    template <typename Control, typename... Options>
        requires requires(Control& control, HWND parent, ControlId id,
                          Options&&... options) {
            { control.Create(parent, id, RectDip{},
                             std::forward<Options>(options)...) }
                -> std::convertible_to<bool>;
        }
    Control& Add(Control& control, Options&&... options) {
        return AddNative(control, NextId(), RectDip{},
                         std::forward<Options>(options)...);
    }

    template <typename Control, typename... Options>
        requires requires(Control& control, HWND parent, ControlId id,
                          std::wstring_view text, Options&&... options) {
            { control.Create(parent, id, text, RectDip{},
                             std::forward<Options>(options)...) }
                -> std::convertible_to<bool>;
        }
    Control& Add(Control& control, std::wstring_view text,
                 Options&&... options) {
        return AddNative(control, NextId(), text, RectDip{},
                         std::forward<Options>(options)...);
    }

    template <typename Control, typename... Options>
        requires requires(
            Control& control, HWND parent, ControlId id, Options&&... options) {
            { control.Create(
                parent, id, RectDip{}, std::forward<Options>(options)...) }
                -> std::convertible_to<bool>;
        }
    Control& Add(
        Control& control, ControlId id, Options&&... options) const {
        return AddNative(
            control, id, RectDip{}, std::forward<Options>(options)...);
    }

    template <typename Control, typename... Options>
        requires requires(
            Control& control, HWND parent, ControlId id,
            std::wstring_view text, Options&&... options) {
            { control.Create(
                parent, id, text, RectDip{},
                std::forward<Options>(options)...) }
                -> std::convertible_to<bool>;
        }
    Control& Add(
        Control& control, ControlId id, std::wstring_view text,
        Options&&... options) const {
        return AddNative(
            control, id, text, RectDip{},
            std::forward<Options>(options)...);
    }

    template <typename Control, typename... Options>
    Control& Add(Control& control, ControlId id, RectDip bounds,
                 Options&&... options) const {
        return AddNative(
            control, id, bounds, std::forward<Options>(options)...);
    }

    template <typename Control, typename... Options>
    Control& Add(Control& control, ControlId id, std::wstring_view text,
                 RectDip bounds, Options&&... options) const {
        return AddNative(
            control, id, text, bounds, std::forward<Options>(options)...);
    }

    template <typename Control, typename... Arguments>
        requires requires(Control& control, HWND parent, Arguments&&... arguments) {
            { control.Create(parent, std::forward<Arguments>(arguments)...) }
                -> std::convertible_to<bool>;
        }
    Control& AddNative(Control& control, Arguments&&... arguments) const {
        ::SetLastError(ERROR_SUCCESS);
        if (!control.Create(parent_, std::forward<Arguments>(arguments)...)) {
            DWORD error = ::GetLastError();
            if (error == ERROR_SUCCESS) error = ERROR_CANNOT_MAKE;
            throw Error(error, "mwfl control creation");
        }
        return control;
    }

    HWND GetParent() const noexcept { return parent_; }

private:
    ControlId NextId() noexcept { return {next_id_++}; }

    HWND parent_ = nullptr;
    int next_id_ = 0x4000;
};

}  // namespace mwfl
