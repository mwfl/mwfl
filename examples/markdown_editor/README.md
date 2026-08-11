# Markdown Editor

This compiled example demonstrates **product-style local-first tabbed Markdown editor with single-workspace edit and preview modes Lexilla syntax coloring GFM preview atomic saves recovery menus shortcuts and graceful runtime fallback**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![Markdown Editor example running on Windows](../../docs/images/examples/markdown-editor.png)

## What it demonstrates

- `ScintillaEditor`
- `WebView2Host`
- `TabWorkspaceModel`
- `TabControl`
- `DocumentState`
- `WriteTextFileAtomic`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class MarkdownEditorWindow final : public mwfl::WindowBase {
   public:
    void BuildUI() override {
        SetTitle(L"Untitled.md - MWFL Markdown");
        BuildCommands();
        if (!scintilla_runtime_.LoadAdjacent()) {
            throw std::runtime_error(
                "Scintilla.dll is unavailable beside the executable; enable "
                "MWFL_BUILD_SCINTILLA and deploy the runtime");
        }

        mwfl::ControlHost ui{*this};
        ui.Add(new_, kNew, L"New");
        ui.Add(open_, kOpen, L"Open...");
        ui.Add(save_, kSave, L"Save");
        ui.Add(heading_, kHeading, L"H1");
        ui.Add(bold_, kBold, L"Bold");
        ui.Add(italic_, kItalic, L"Italic");
```

Read the complete implementation in [`main.cpp`](main.cpp), [`markdown_editor.rc`](markdown_editor.rc), [`markdown_renderer.cpp`](markdown_renderer.cpp), [`markdown_renderer.h`](markdown_renderer.h), [`markdown_syntax.cpp`](markdown_syntax.cpp), [`markdown_syntax.h`](markdown_syntax.h), [`resource.h`](resource.h).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64-optional
cmake --build --preset vs2026-x64-optional-debug --target mwfl_markdown_editor
build\presets\vs2026-x64-optional\examples\markdown_editor\Debug\mwfl_markdown_editor.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain. This example uses the optional `scintilla+webview2` component; the optional preset shown above enables its pinned dependency and runtime staging rules.

## Validation

The focused validation targets are `mwfl.markdown_renderer`, `mwfl.markdown_editor_gui`. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
