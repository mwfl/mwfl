#include <mwfl/document.h>

#include <cstdlib>
#include <filesystem>
#include <stdexcept>

namespace {
bool RejectsEmptyNamesAndPaths() {
    try {
        mwfl::DocumentState invalid{L""};
        static_cast<void>(invalid);
        return false;
    } catch (const std::invalid_argument&) {
    }
    mwfl::DocumentState document;
    try { document.MarkOpened({}); return false; }
    catch (const std::invalid_argument&) {}
    try { document.MarkSavedAs({}); return false; }
    catch (const std::invalid_argument&) {}
    return true;
}
}  // namespace

int main() {
    using enum mwfl::DocumentTransition;
    using enum mwfl::UnsavedChangesChoice;
    mwfl::DocumentState document{L"New note"};
    if (document.HasPath() || document.IsDirty() ||
        document.GetDisplayName() != L"New note" ||
        document.EvaluateTransition() != proceed) return EXIT_FAILURE;

    document.MarkChanged();
    if (!document.IsDirty() || document.EvaluateTransition(cancel) != cancelled ||
        document.EvaluateTransition(save) != save_required ||
        document.EvaluateTransition(discard) != proceed) return EXIT_FAILURE;

    const std::filesystem::path first = LR"(C:\notes\first.txt)";
    document.MarkSavedAs(first);
    if (!document.HasPath() || document.IsDirty() || document.GetPath() != first ||
        document.GetDisplayName() != L"first.txt") return EXIT_FAILURE;

    document.MarkChanged();
    document.MarkSaved();
    if (document.IsDirty() || document.GetPath() != first) return EXIT_FAILURE;

    const std::filesystem::path second = LR"(D:\work\second.txt)";
    document.MarkOpened(second);
    if (document.IsDirty() || document.GetDisplayName() != L"second.txt") return EXIT_FAILURE;

    document.MarkChanged();
    document.ResetUntitled();
    if (document.HasPath() || document.IsDirty() ||
        document.GetDisplayName() != L"New note") return EXIT_FAILURE;
    return RejectsEmptyNamesAndPaths() ? EXIT_SUCCESS : EXIT_FAILURE;
}
