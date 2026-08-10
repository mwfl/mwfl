# Build an Explorer-style application

This tutorial builds the complete composition in `examples/explorer`. The
primary local workflow uses Visual Studio 2026, MSVC, C++20, x64 Debug and
Release. Start only after the unchanged repository builds successfully.

## 1. Configure and run the canonical application

Open **Developer PowerShell for VS 2026** at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_explorer_demo
& .\build\presets\vs2026-x64\examples\explorer\Debug\mwfl_explorer_demo.exe
```

Use the folder tree, click list columns, drag the splitter, press F5, switch
tabs, and open a list context menu. These paths use real Common Controls HWNDs.

## 2. Copy the supported starter

Copy the maintained C++20/Per-Monitor-V2 template and the canonical Explorer
sources into a separate project:

```powershell
Copy-Item -Recurse .\templates\basic-app ..\mwfl-explorer-tutorial
Copy-Item .\examples\explorer\main.cpp, `
  .\examples\explorer\explorer_model.cpp, `
  .\examples\explorer\explorer_model.h `
  ..\mwfl-explorer-tutorial
Set-Location ..\mwfl-explorer-tutorial
```

Change the executable line in `CMakeLists.txt`:

```cmake
add_executable(mwfl_basic_app WIN32
    main.cpp explorer_model.cpp explorer_model.h app.manifest)
```

Configure against the checkout, build, and run before editing:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 `
  -DMWFL_SOURCE_DIR=D:/GitHub/mwfl
cmake --build build --config Debug
& .\build\Debug\mwfl_basic_app.exe
```

## 3. Define stable application data

Derive an application-owned model from `VirtualListModel`. Folder and file IDs
must be unique, stable, and nonzero:

```cpp
struct FileEntry {
    mwfl::ListItemId id;
    mwfl::TreeItemId folder;
    std::wstring name;
    std::wstring type;
    std::uint64_t size_bytes;
    int image_index;
};

class ExplorerModel final : public mwfl::VirtualListModel {
public:
    std::size_t GetRowCount() const noexcept override;
    mwfl::ListItemId GetRowId(std::size_t row) const noexcept override;
    std::wstring GetCellText(std::size_t row, int column) const override;
};
```

Filtering and sorting rebuild a vector of indices, not the file records or IDs.
This preserves selection identity when visible order changes.

## 4. Create the command strip

Create one owned `ImageList` before controls that borrow it. Add Back, Up,
Refresh, and Sort to one `CommandSet`; give Refresh an F5 shortcut. Create the
Rebar as a top-level child, create Toolbar under the Rebar, then attach the image
list and insert the toolbar as a band:

```cpp
rebar_ui.Add(toolbar, {201}, toolbar_bounds);
toolbar.SetImageList(images);
for (const auto& command : commands.GetCommands()) toolbar.AddCommand(command);
rebar.AddBand(toolbar, L"Navigation", 500);
```

The ImageList owns copied icon images. Toolbar, TreeView, and ListView borrow its
native handle, so declare the ImageList before those wrappers and keep it alive
until their HWNDs are destroyed.

## 5. Compose the split navigation surface

Create Splitter under the main window. Its two panes must be direct children:

```cpp
ui.Add(splitter, {203}, {}, mwfl::SplitterOptions{
    .constraints = {180.0_dip, 320.0_dip, 6.0_dip},
    .initial_position = 250.0_dip,
});
mwfl::ControlHost panes{splitter};
panes.Add(tree, {204}, {});
panes.Add(list, {205}, {}, mwfl::ListViewOptions{.virtual_data = true});
mwfl::Must(splitter.AttachPanes(tree.GetHwnd(), list.GetHwnd()),
           "attach Explorer panes");
```

Splitter forwards `WM_NOTIFY`, child-control `WM_COMMAND`, and `WM_CONTEXTMENU`
to its parent. This is essential for TreeView selection and owner-data ListView
callbacks. Do not reparent panes or add a second notification relay.

## 6. Populate stable navigation and virtual columns

Insert the root and children with `TreeItemId`. Add Name, Type, and Size columns,
then attach the shared model:

```cpp
mwfl::AddColumns(list, {{L"Name", 300}, {L"Type", 210}, {L"Size", 120}});
list.SetVirtualModel(model);
```

At the beginning of `OnNotify`, let the ListView service owner-data callbacks:

```cpp
LRESULT result = 0;
if (list.HandleNotification(event.header, result)) {
    if (auto error = list.TakeVirtualException()) std::rethrow_exception(error);
    return mwfl::EventResult::Handled(result);
}
```

Decode remaining list notifications for column sorting and selection status.
Decode TreeView selection and mutate the model inside `UpdateVirtualModel`; it
preserves selected stable IDs that remain visible.

## 7. Add tabs, status, and retained layout

Use `TabWorkspaceModel` for stable tab identity, then synchronize TabControl.
Let Rebar and StatusBar use intrinsic height, keep tabs fixed, and stretch the
splitter:

```cpp
SetLayout(mwfl::Column()
    .Add(rebar, mwfl::Auto())
    .Add(tabs, mwfl::Fixed(34.0_dip))
    .Add(splitter, mwfl::Stretch())
    .Add(status, mwfl::Auto()));
```

Update status-part right edges from `OnResize`. Name Toolbar, TreeView, ListView,
and Tabs with `SetAccessibleName`; visible labels are not available for these
structural surfaces.

## 8. Route a keyboard context menu

Handle `WM_CONTEXTMENU` for either pane. Coordinates `(-1,-1)` mean keyboard
activation; use the current cursor position as a safe screen anchor. Build an
owned popup `Menu`, call `TrackResult`, distinguish cancellation from failure,
and post a selected ID through `WM_COMMAND`. Use the same command IDs as Toolbar
and accelerators.

## 9. Run focused and full evidence

```powershell
cmake --build --preset vs2026-x64-debug `
  --target mwfl_explorer_demo mwfl_explorer_model_test mwfl_splitter_native_test
ctest --test-dir build/presets/vs2026-x64 -C Debug `
  -R "^mwfl[.](explorer_model|explorer_gui|splitter_native)$" `
  --output-on-failure
```

Before committing, run both complete local gates:

```powershell
cmake --build --preset vs2026-x64-debug
ctest --preset vs2026-x64-debug
cmake --build --preset vs2026-x64-release
ctest --preset vs2026-x64-release
```

The GUI self-test uses the real executable and intentionally avoids filesystem
or Shell namespace dependencies, so results are deterministic on Windows 10+
while still proving native desktop composition.
