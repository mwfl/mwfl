# Add a custom dialog

Use `mwfl/dialog.h` when a task dialog or file picker cannot contain the custom
controls your workflow needs. `Dialog` is move-only: it owns a modeless HWND,
while `ShowModal` uses the native dialog nested loop.

1. Keep every child-control wrapper alive for at least as long as the dialog.
2. Pass the parent window as the borrowed `owner`.
3. Create controls inside `callbacks.initialize`.
4. Give OK and Cancel buttons the native `IDOK` and `IDCANCEL` IDs.
5. Call `SetLayout` with the same DIP-based `Row` or `Column` used by a normal
   mwfl window.
6. Read values in the command callback before the native HWNDs are destroyed.
7. Inspect `DialogResult::status`; do not treat cancellation as failure.

```cpp
using mwfl::operator""_dip;

mwfl::TextBox name;
mwfl::Button ok, cancel;
std::wstring accepted_name;
mwfl::Dialog* active = nullptr;

mwfl::Dialog dialog({
    .owner = GetHwnd(),
    .title = L"Profile",
    .callbacks = {
        .initialize = [&](HWND window) {
            mwfl::ControlHost ui{window};
            ui.Add(name, {101}, L"Ada", {});
            ui.Add(ok, {IDOK}, L"OK", {});
            ui.Add(cancel, {IDCANCEL}, L"Cancel", {});
            return active->SetLayout(
                mwfl::Column().Margin(16.0_dip).Gap(8.0_dip)
                    .Add(name, mwfl::Fixed(34.0_dip))
                    .Add(mwfl::Row().Add(ok, mwfl::Stretch())
                                     .Add(cancel, mwfl::Stretch()),
                         mwfl::Fixed(36.0_dip)));
        },
        .command = [&](HWND, WORD id, WORD) {
            if (id == IDOK) accepted_name = name.GetText();
            return false; // let Dialog apply standard IDOK/IDCANCEL handling
        },
    },
});
active = &dialog;
const mwfl::DialogResult result = dialog.ShowModal();
```

All callbacks and control access belong to the creating UI thread. `Accept`,
`Cancel`, and destruction can post a close request from another thread, but the
creating thread must keep pumping messages until that request is processed.
Callback exceptions are returned through `callback_exception`; they never cross
the Win32 callback boundary. `GetHwnd()` is a borrowed escape hatch for Windows
10+ APIs not wrapped by mwfl.
