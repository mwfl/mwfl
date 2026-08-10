#pragma once

#include <windows.h>

#include <concepts>

namespace mwfl {

struct ControlId;

// A native object that exposes a non-owning HWND view. This deliberately does
// not prescribe ownership, inheritance, or a specific wrapper base class.
template <typename T>
concept WindowLike = requires(const T& value) {
    { value.GetHwnd() } -> std::same_as<HWND>;
};

// A WindowLike object that also exposes a native control identifier.
template <typename T>
concept ControlLike = WindowLike<T> && requires(const T& value) {
    { value.GetId() } -> std::same_as<ControlId>;
};

}  // namespace mwfl
