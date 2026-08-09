#include <mwtl/text_history.h>

#include <cstdlib>
#include <stdexcept>

int main() {
    try { mwtl::TextHistory invalid{1}; return EXIT_FAILURE; }
    catch (const std::invalid_argument&) {}

    mwtl::TextHistory history{4};
    if (history.IsModified() || history.CanUndo() || history.CanRedo()) return EXIT_FAILURE;
    if (!history.Record(L"a") || !history.Record(L"ab") || !history.IsModified()) return EXIT_FAILURE;
    history.MarkSaved();
    if (history.IsModified()) return EXIT_FAILURE;
    if (!history.Record(L"abc") || history.Undo() != L"ab" || history.IsModified()) return EXIT_FAILURE;
    if (history.Redo() != L"abc" || !history.IsModified()) return EXIT_FAILURE;
    if (history.Undo() != L"ab" || history.Undo() != L"a") return EXIT_FAILURE;
    if (!history.Record(L"replacement") || history.CanRedo()) return EXIT_FAILURE;

    history.Record(L"two");
    history.Record(L"three");
    history.Record(L"four");
    if (history.Current() != L"four") return EXIT_FAILURE;
    int undo_count{};
    while (history.Undo()) ++undo_count;
    if (undo_count != 3 || !history.IsModified()) return EXIT_FAILURE;

    history.Reset(L"loaded", true);
    return history.Current() == L"loaded" && !history.IsModified()
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
