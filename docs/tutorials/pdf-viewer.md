# Build the PDF Viewer

The PDF Viewer is a flagship local-document example. MWFL owns the native
window, commands, tabs, layout, dialogs, drag-and-drop, accessibility, window
placement, and failure boundaries. The pinned WebView2 component displays PDF
content using the locally installed Evergreen Runtime.

## Configure and run

```powershell
cmake --preset vs2026-x64-webview2
cmake --build --preset vs2026-x64-webview2-debug --target mwfl_pdf_viewer
./build/presets/vs2026-x64-webview2/examples/pdf_viewer/Debug/mwfl_pdf_viewer.exe
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
ctest --test-dir build/presets/vs2026-x64-webview2 -C Debug \
  -R "mwfl\.pdf_viewer_gui" --output-on-failure
```

The self-test creates the real hidden window, initializes the WebView2
controller, writes and opens a valid local PDF, validates the native tab
projection, and closes without reading or changing user settings.

Create the tested x64 Release ZIP with:

```powershell
./scripts/package-pdf-viewer.ps1 -VisualStudio 2026 -Version 0.1.0
```

The script builds Release, runs the real PDF GUI self-test, stages only the
`pdf_viewer` install component, and prints the archive SHA-256 digest.
