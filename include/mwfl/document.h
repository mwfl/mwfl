#pragma once

#include <filesystem>
#include <string>

namespace mwfl {

enum class UnsavedChangesChoice { save, discard, cancel };
enum class DocumentTransition { proceed, save_required, cancelled };

// Pure, UI-independent identity and dirty-state model for one document.
// Applications own file contents and perform I/O; this object records only
// transitions that have actually succeeded.
class DocumentState final {
public:
    explicit DocumentState(std::wstring untitled_name = L"Untitled");

    bool HasPath() const noexcept { return !path_.empty(); }
    const std::filesystem::path& GetPath() const noexcept { return path_; }
    std::wstring GetDisplayName() const;
    bool IsDirty() const noexcept { return dirty_; }

    void ResetUntitled() noexcept;
    void MarkOpened(std::filesystem::path path);
    void MarkChanged() noexcept { dirty_ = true; }
    void MarkSaved() noexcept { dirty_ = false; }
    void MarkSavedAs(std::filesystem::path path);
    DocumentTransition EvaluateTransition(
        UnsavedChangesChoice choice = UnsavedChangesChoice::cancel) const noexcept;

private:
    std::filesystem::path path_;
    std::wstring untitled_name_;
    bool dirty_ = false;
};

}  // namespace mwfl
