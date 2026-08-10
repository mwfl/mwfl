#pragma once

#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <ranges>
#include <span>
#include <string_view>
#include <type_traits>

#include <mwfl/command_controls.h>
#include <mwfl/navigation_controls.h>

namespace mwfl {

struct BatchResult {
    std::size_t completed = 0;
    std::ptrdiff_t failed_at = -1;
    std::intptr_t native_result = 0;

    constexpr bool Succeeded() const noexcept {
        return failed_at < 0;
    }
    constexpr explicit operator bool() const noexcept {
        return Succeeded();
    }

    static constexpr BatchResult Success(
        std::size_t count) noexcept {
        return {count, -1, 0};
    }
    static constexpr BatchResult Failure(
        std::size_t count,
        std::intptr_t result) noexcept {
        return {count, static_cast<std::ptrdiff_t>(count), result};
    }
};

template <typename Control>
concept TextItemControl = requires(Control& control, std::wstring_view text) {
    { control.AddItem(text) } -> std::signed_integral;
};

template <TextItemControl Control, std::ranges::input_range Range>
    requires requires(std::ranges::range_reference_t<Range> item) {
                 std::wstring_view{item};
             }
BatchResult AddItems(Control& control, Range&& items) {
    std::size_t completed = 0;
    for (auto&& item : items) {
        const auto result = control.AddItem(std::wstring_view{item});
        if (result < 0) {
            return BatchResult::Failure(
                completed, static_cast<std::intptr_t>(result));
        }
        ++completed;
    }
    return BatchResult::Success(completed);
}

template <TextItemControl Control>
BatchResult AddItems(
    Control& control,
    std::initializer_list<std::wstring_view> items) {
    return AddItems(control, std::span(items.begin(), items.size()));
}

struct ColumnSpec {
    std::wstring_view text;
    int width_pixels = 100;
    int index = -1;
};

template <typename Control>
concept ColumnControl = requires(Control& control, const ColumnSpec& column) {
    { control.AddColumn(column.text, column.width_pixels, column.index) }
        -> std::signed_integral;
};

template <ColumnControl Control, std::ranges::input_range Range>
    requires std::convertible_to<
                 std::ranges::range_reference_t<Range>, ColumnSpec>
BatchResult AddColumns(Control& control, Range&& columns) {
    std::size_t completed = 0;
    for (auto&& value : columns) {
        const ColumnSpec column = value;
        const auto result = control.AddColumn(
            column.text, column.width_pixels, column.index);
        if (result < 0) {
            return BatchResult::Failure(
                completed, static_cast<std::intptr_t>(result));
        }
        ++completed;
    }
    return BatchResult::Success(completed);
}

template <ColumnControl Control>
BatchResult AddColumns(
    Control& control,
    std::initializer_list<ColumnSpec> columns) {
    return AddColumns(control, std::span(columns.begin(), columns.size()));
}

template <std::ranges::input_range Range>
    requires requires(std::ranges::range_reference_t<Range> item) {
                 std::wstring_view{item};
             }
BatchResult AddTabs(TabControl& control, Range&& tabs) {
    std::size_t completed = 0;
    for (auto&& item : tabs) {
        const int result = control.AddTab(std::wstring_view{item});
        if (result < 0) return BatchResult::Failure(completed, result);
        ++completed;
    }
    return BatchResult::Success(completed);
}

inline BatchResult AddTabs(
    TabControl& control,
    std::initializer_list<std::wstring_view> tabs) {
    return AddTabs(control, std::span(tabs.begin(), tabs.size()));
}

struct ToolbarButtonSpec {
    ControlId command{};
    std::wstring_view text;
};

template <std::ranges::input_range Range>
    requires std::convertible_to<
                 std::ranges::range_reference_t<Range>, ToolbarButtonSpec>
BatchResult AddButtons(Toolbar& toolbar, Range&& buttons) {
    std::size_t completed = 0;
    for (auto&& value : buttons) {
        const ToolbarButtonSpec button = value;
        ::SetLastError(ERROR_SUCCESS);
        if (!toolbar.AddTextButton(button.command, button.text)) {
            const DWORD error = ::GetLastError();
            return BatchResult::Failure(
                completed, error == ERROR_SUCCESS
                    ? std::intptr_t{-1}
                    : static_cast<std::intptr_t>(error));
        }
        ++completed;
    }
    return BatchResult::Success(completed);
}

inline BatchResult AddButtons(
    Toolbar& toolbar,
    std::initializer_list<ToolbarButtonSpec> buttons) {
    return AddButtons(toolbar, std::span(buttons.begin(), buttons.size()));
}

struct StatusPartText {
    int part = 0;
    std::wstring_view text;
};

template <std::ranges::input_range Range>
    requires std::convertible_to<
                 std::ranges::range_reference_t<Range>, StatusPartText>
BatchResult SetPartTexts(StatusBar& status, Range&& parts) {
    std::size_t completed = 0;
    for (auto&& value : parts) {
        const StatusPartText part = value;
        ::SetLastError(ERROR_SUCCESS);
        if (!status.SetPartText(part.part, part.text)) {
            const DWORD error = ::GetLastError();
            return BatchResult::Failure(
                completed, error == ERROR_SUCCESS
                    ? std::intptr_t{-1}
                    : static_cast<std::intptr_t>(error));
        }
        ++completed;
    }
    return BatchResult::Success(completed);
}

inline BatchResult SetPartTexts(
    StatusBar& status,
    std::initializer_list<StatusPartText> parts) {
    return SetPartTexts(status, std::span(parts.begin(), parts.size()));
}

}  // namespace mwfl
