# Build the Markdown Editor

The Markdown Editor is a product-style example that combines two optional mwfl
components without turning either dependency into part of the core library.
Scintilla owns source editing, WebView2 displays an offline HTML preview, and
mwfl owns the native application lifecycle, layout, document state, dialogs,
Unicode file I/O, and failure boundaries.

## Configure and run

```powershell
cmake --preset vs2026-x64-optional
cmake --build --preset vs2026-x64-optional-debug --target mwfl_markdown_editor
./build/presets/vs2026-x64-optional/examples/markdown_editor/Debug/mwfl_markdown_editor.exe
```

The build deploys pinned `Scintilla.dll` and `Lexilla.dll` beside the executable.
Lexilla's Markdown lexer colors headings, emphasis, links, lists, quotes, and
code with coordinated light and dark palettes. WebView2
uses the Evergreen Runtime. If that runtime is missing, the source editor and
safe file operations remain available and the status line explains why preview
is unavailable.

Create the tested x64 Release ZIP with one command:

```powershell
./scripts/package-markdown-editor.ps1 -VisualStudio 2026 -Version 0.1.0
```

The script configures both optional components, builds Release, runs the
Markdown unit and GUI tests, stages only the `markdown_editor` install
component, includes dependency licensing and a runtime README, then prints the
ZIP's SHA-256 digest.

## Composition

`MarkdownEditorWindow` projects its application-owned documents through
`mwfl::TabWorkspaceModel` and a native `mwfl::TabControl`. A single content
workspace overlays an `mwfl::ScintillaEditor` and an `mwfl::WebView2Host`;
**Ctrl+Shift+P** switches between editing and preview without a permanent split
view. Both surfaces are real child HWNDs owned by their wrappers and used only
from the creating STA thread. Each tab retains its path, text, selection,
encoding, file stamp, and dirty state while the application switches views.

The application-owned renderer uses statically linked md4c 0.5.2 at immutable
commit `729e6b8b320caa96328968ab27d7db2235e4fb47`. Its GitHub dialect supports
tables, task lists, strikethrough, and autolinks. Raw HTML blocks and spans are
disabled, URL schemes are allow-listed after rendering, and the page uses a
restrictive Content Security Policy. No document text is uploaded and no CDN
resource is required. The distributable component includes md4c's MIT license.

The application also offers native File/Edit/Format/View menus, standard
accelerators, in-window find/replace, window placement persistence, stale-save
protection, and delayed atomic recovery for the complete tab session. Recovery
stores a versioned session manifest plus one atomic UTF-8 recovery copy per
document; tab order, active identity, paths, selections, file stamps, and dirty
state are restored together. The GUI self-test covers a two-document recovery
round trip. Recovery is removed after an intentional close.

`mwfl::ReadTextFile` rejects malformed Unicode. `mwfl::WriteTextFileAtomic`
writes through a flushed sibling and uses the last observed `FileStamp` to
reject an overwrite when another process changed the file. `DocumentState`
changes only after successful I/O.

## Validation

```powershell
ctest --test-dir build/presets/vs2026-x64-optional -C Debug \
  -R "mwfl\\.markdown" --output-on-failure
```

`mwfl.markdown_renderer` verifies representative formatting, dark styling,
HTML escaping, and rejection of active URL schemes. `mwfl.markdown_editor_gui`
starts the real executable, asserts representative lexer token styles, exercises
file round-tripping and an offline WebView2 navigation, then exits with a
machine-readable result.

The renderer remains application code. General Markdown parsing is not a
Windows UI concern and should not be added to the stable mwfl public surface.
