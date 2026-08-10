#include <mwfl/text_history.h>

#include <stdexcept>
#include <utility>

namespace mwfl {

TextHistory::TextHistory(std::size_t maximum_entries)
    : maximum_entries_(maximum_entries) {
    if (maximum_entries_ < 2) throw std::invalid_argument("text history needs at least two entries");
    Reset();
}

void TextHistory::Reset(std::wstring text, bool mark_saved) {
    entries_.clear();
    entries_.push_back(std::move(text));
    current_ = 0;
    saved_ = mark_saved ? std::optional<std::size_t>{0} : std::nullopt;
}

bool TextHistory::Record(std::wstring text) {
    if (text == entries_[current_]) return false;
    entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(current_ + 1), entries_.end());
    if (saved_ && *saved_ > current_) saved_.reset();
    entries_.push_back(std::move(text));
    ++current_;
    TrimToCapacity();
    return true;
}

std::optional<std::wstring_view> TextHistory::Undo() noexcept {
    if (!CanUndo()) return std::nullopt;
    --current_;
    return entries_[current_];
}

std::optional<std::wstring_view> TextHistory::Redo() noexcept {
    if (!CanRedo()) return std::nullopt;
    ++current_;
    return entries_[current_];
}

std::wstring_view TextHistory::Current() const noexcept {
    return entries_[current_];
}

void TextHistory::TrimToCapacity() {
    if (entries_.size() <= maximum_entries_) return;
    const std::size_t remove_count = entries_.size() - maximum_entries_;
    entries_.erase(entries_.begin(), entries_.begin() + static_cast<std::ptrdiff_t>(remove_count));
    current_ -= remove_count;
    if (saved_) {
        if (*saved_ < remove_count) saved_.reset();
        else *saved_ -= remove_count;
    }
}

}  // namespace mwfl
