#include <mwfl/command.h>

#include <cstdlib>

int main() {
    bool invoked = false;
    mwfl::CommandSet commands;
    commands.Add(mwfl::Command({101}, L"Save", [&] { invoked = true; }));
    commands.Find({101})->SetShortcut({FVIRTKEY | FCONTROL, 'S'});
    commands.Find({101})->SetVisible(false).SetImageIndex(3);
    if (commands.GetCount() != 1 || commands.Find({101}) == nullptr) return EXIT_FAILURE;
    if (commands.Find({101})->IsVisible() ||
        commands.Find({101})->GetImageIndex() != 3) return EXIT_FAILURE;
    if (!commands.Dispatch({{101}, BN_CLICKED, nullptr}).handled || !invoked) return EXIT_FAILURE;

    invoked = false;
    commands.Find({101})->SetEnabled(false);
    if (commands.Dispatch({{101}, BN_CLICKED, nullptr}).handled || invoked) return EXIT_FAILURE;
    if (commands.Dispatch({{999}, BN_CLICKED, nullptr}).handled) return EXIT_FAILURE;

    commands.Add(mwfl::Command({101}, L"Save as"));
    return commands.GetCount() == 1 && commands.Find({101})->GetText() == L"Save as" &&
            commands.Find({101})->IsVisible() &&
            !commands.Find({101})->GetImageIndex() &&
            !commands.Find({101})->GetShortcut()
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
