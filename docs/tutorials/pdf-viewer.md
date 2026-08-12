# Build the PDF Viewer

The standalone [PDF Reader](https://github.com/mwfl/pdf-reader) is a local-document application. MWFL owns the native
window, commands, tabs, layout, dialogs, drag-and-drop, accessibility, window
placement, and failure boundaries. The pinned WebView2 component displays PDF
content using the locally installed Evergreen Runtime.

## Configure and run

```powershell
git clone https://github.com/mwfl/pdf-reader.git
cd pdf-reader
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-release
ctest --preset vs2026-x64-release
```

Open one or more local `.pdf` files with **Ctrl+O** or by dropping them on the
window. `TabWorkspaceModel` remains the authoritative document order and
selection; `TabControl::Synchronize` projects that state into native tabs.
The application converts only selected local paths to `file:` URIs and never
uploads document contents.

`WebView2Host` is initialized on the application STA. A missing runtime is a
normal visible failure, and a crashed browser process is restarted only after
posting back to the ordinary MWFL message path. Window placement is stored per
user and disabled during the deterministic GUI self-test.

## Validation

```powershell
ctest --preset vs2026-x64-release
```

The self-test creates the real hidden window, initializes the WebView2
controller, writes and opens a valid local PDF, validates the native tab
projection, and closes without reading or changing user settings.

Every push in the application repository runs the real PDF GUI self-test and uploads a portable Windows x64 ZIP artifact.
