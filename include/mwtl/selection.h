#pragma once

#include <windows.h>

#include <concepts>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mwtl {

template <typename Control>
concept SingleSelectionControl = requires(Control& control, const Control& read,
                                          std::wstring_view label, int index) {
    { control.AddItem(label) } -> std::signed_integral;
    { read.GetItemCount() } -> std::same_as<int>;
    { read.GetSelectedIndex() } -> std::same_as<std::optional<int>>;
    { control.SetSelection(index) } -> std::same_as<bool>;
    { control.RemoveItem(index) } -> std::same_as<bool>;
    { control.ClearItems() } -> std::same_as<bool>;
};

template <typename Value>
struct SelectionEntry {
    std::wstring label;
    Value value;
};

// Owns typed application values separately from the native control. The
// control stores only labels; no pointers or object addresses enter item data.
// Keep the adapter on the control's UI thread and destroy it before the control.
template <SingleSelectionControl Control, typename Value>
class SelectionAdapter final {
public:
    explicit SelectionAdapter(Control& control) noexcept : control_(&control) {}

    bool Add(std::wstring_view label, Value value) {
        if (control_->GetItemCount() != static_cast<int>(entries_.size())) {
            ::SetLastError(ERROR_INVALID_STATE);
            return false;
        }
        const int index = control_->AddItem(label);
        if (index < 0) return false;
        if (index != static_cast<int>(entries_.size())) {
            static_cast<void>(control_->RemoveItem(index));
            ::SetLastError(ERROR_INVALID_DATA);
            return false;
        }
        try {
            entries_.push_back({std::wstring{label}, std::move(value)});
        } catch (...) {
            static_cast<void>(control_->RemoveItem(index));
            throw;
        }
        return true;
    }

    bool Remove(std::size_t index) {
        if (index >= entries_.size()) {
            ::SetLastError(ERROR_INVALID_INDEX);
            return false;
        }
        if (!control_->RemoveItem(static_cast<int>(index))) return false;
        entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }

    bool Clear() {
        if (!control_->ClearItems()) return false;
        entries_.clear();
        return true;
    }

    bool Select(std::size_t index) noexcept {
        if (index >= entries_.size()) {
            ::SetLastError(ERROR_INVALID_INDEX);
            return false;
        }
        return control_->SetSelection(static_cast<int>(index));
    }

    bool SelectValue(const Value& value)
        requires std::equality_comparable<Value> {
        for (std::size_t index = 0; index < entries_.size(); ++index) {
            if (entries_[index].value == value) return Select(index);
        }
        ::SetLastError(ERROR_NOT_FOUND);
        return false;
    }

    std::optional<std::reference_wrapper<const Value>> GetSelectedValue() const noexcept {
        const auto index = control_->GetSelectedIndex();
        if (!index || *index < 0 || static_cast<std::size_t>(*index) >= entries_.size()) {
            return std::nullopt;
        }
        return std::cref(entries_[static_cast<std::size_t>(*index)].value);
    }

    const SelectionEntry<Value>* Get(std::size_t index) const noexcept {
        return index < entries_.size() ? &entries_[index] : nullptr;
    }
    std::size_t Size() const noexcept { return entries_.size(); }
    bool IsSynchronized() const noexcept {
        return control_->GetItemCount() == static_cast<int>(entries_.size());
    }

private:
    Control* control_;  // Borrowed; same UI-thread lifetime contract as Control.
    std::vector<SelectionEntry<Value>> entries_;
};

}  // namespace mwtl
