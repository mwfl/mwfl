#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mwtl {

class TextHistory final {
public:
    explicit TextHistory(std::size_t maximum_entries = 100);

    void Reset(std::wstring text = {}, bool mark_saved = true);
    bool Record(std::wstring text);
    bool CanUndo() const noexcept { return current_ > 0; }
    bool CanRedo() const noexcept { return current_ + 1 < entries_.size(); }
    std::optional<std::wstring_view> Undo() noexcept;
    std::optional<std::wstring_view> Redo() noexcept;
    std::wstring_view Current() const noexcept;
    void MarkSaved() noexcept { saved_ = current_; }
    bool IsModified() const noexcept { return !saved_ || *saved_ != current_; }

private:
    void TrimToCapacity();
    std::vector<std::wstring> entries_;
    std::size_t current_{};
    std::optional<std::size_t> saved_;
    std::size_t maximum_entries_;
};

}  // namespace mwtl
