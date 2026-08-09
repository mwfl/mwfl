#include <mwtl/document.h>

#include <stdexcept>
#include <utility>

namespace mwtl {

DocumentState::DocumentState(std::wstring untitled_name)
    : untitled_name_(std::move(untitled_name)) {
    if (untitled_name_.empty()) {
        throw std::invalid_argument("document untitled name must not be empty");
    }
}

std::wstring DocumentState::GetDisplayName() const {
    return HasPath() ? path_.filename().native() : untitled_name_;
}

void DocumentState::ResetUntitled() noexcept {
    path_.clear();
    dirty_ = false;
}

void DocumentState::MarkOpened(std::filesystem::path path) {
    if (path.empty()) {
        throw std::invalid_argument("opened document path must not be empty");
    }
    path_ = std::move(path);
    dirty_ = false;
}

void DocumentState::MarkSavedAs(std::filesystem::path path) {
    if (path.empty()) {
        throw std::invalid_argument("saved document path must not be empty");
    }
    path_ = std::move(path);
    dirty_ = false;
}

DocumentTransition DocumentState::EvaluateTransition(
    UnsavedChangesChoice choice) const noexcept {
    if (!dirty_ || choice == UnsavedChangesChoice::discard) {
        return DocumentTransition::proceed;
    }
    return choice == UnsavedChangesChoice::save
        ? DocumentTransition::save_required
        : DocumentTransition::cancelled;
}

}  // namespace mwtl
