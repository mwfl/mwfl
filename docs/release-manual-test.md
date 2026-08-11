# v0.1.0 manual acceptance checklist

Use the four ZIP files produced from the release-candidate commit. Extract each
ZIP to a new directory instead of running executables from a build tree. Record
the Windows version, display scale, and WebView2 Runtime version with the test
result. A failed item is release-blocking unless it is explicitly marked as an
environment limitation.

## Package integrity

- Verify every SHA-256 value against `SHA256SUMS-x64.txt` from the release.
- Confirm Windows Defender or the organization's endpoint scanner accepts all
  extracted files.
- Launch each application without Visual Studio or the repository on `PATH`.
- Confirm the Markdown package contains `Scintilla.dll`, `Lexilla.dll`, and both
  license files. Confirm the Notepad and PDF packages need no adjacent private
  DLLs.

## MWFL Markdown Editor

- Launch `bin\mwfl_markdown_editor.exe`. Confirm the initial workspace has one
  document tab, one editing surface, no permanent split preview, no line-number
  gutter, and no always-visible search panel.
- Enter headings, bold, italic, links, lists, task lists, block quotes, inline
  code, and fenced code. Confirm syntax styling remains readable in light and
  dark modes.
- Press `Ctrl+F` and `Ctrl+H`. Confirm Find and Replace appear, carry useful hint
  text, find the next match, and replace only the intended text.
- Press `Ctrl+Shift+P` repeatedly. Confirm Edit and Preview replace each other in
  the same workspace, the tab remains selected, and preview rendering is local.
- Create three tabs, type distinct content in each, switch among them, and close
  the middle tab. Confirm text, selection, dirty markers, and active tab remain
  correct.
- Save UTF-8 Markdown, reopen it, use Save As, and drag or open a document whose
  path contains spaces and non-ASCII characters. Confirm no data loss or mojibake.
- Modify a saved file externally, then attempt to save the stale editor copy.
  Confirm the application does not silently overwrite the newer disk version.
- Leave two dirty tabs open, wait several seconds, terminate the process from
  Task Manager, and relaunch. Accept recovery and confirm both documents, active
  tab, text, dirty state, and selection are restored.
- Close with dirty documents and exercise Save, Don't Save, and Cancel on
  separate runs.

## MWFL Notepad

- Launch `bin\mwfl_notepad.exe`; type, select, cut, copy, paste, undo, redo, find,
  replace, and select all using both menus and keyboard shortcuts.
- Open and save UTF-8, UTF-8 BOM, UTF-16 LE, CRLF, and LF files. Confirm the two
  right-hand status fields report encoding and line endings accurately.
- Open a path containing spaces and non-ASCII characters by command line and by
  drag/drop. Confirm a second launch forwards the file to the existing window.
- Modify the open file externally and attempt to save. Confirm stale-file
  protection prevents a silent overwrite.
- Exercise New, Open, Save, Save As, recent files, Always on Top, word wrap,
  unsaved-close choices, and restored window placement.

## MWFL PDF Viewer

- Launch `bin\mwfl_pdf_viewer.exe`. Confirm the Welcome tab and empty state are
  readable and explain Open, `Ctrl+O`, and drag/drop.
- Open at least three PDFs: a small text document, an image-heavy document, and
  a long document. Include one path with spaces and non-ASCII characters.
- Open multiple files from the file picker and by drag/drop. Confirm each gets a
  native tab, switching is stable, and closing a middle tab selects a sensible
  neighbor.
- Exercise Back, Forward, Reload, `Ctrl+W`, recent files, and restored window
  placement. Reopen the application and confirm recent entries still resolve.
- In the embedded viewer, test page navigation, zoom, search, print, and save a
  copy. These commands are supplied by WebView2's PDF renderer and should not
  disturb MWFL tab state.
- If a machine without the Evergreen WebView2 Runtime is available, confirm the
  application shows a useful failure state instead of a blank or crashing
  window. Record this item as not available when such a machine is unavailable.

## Hot Corners

- Launch the Hot Corners example and confirm its tray icon and MWFL naming.
- Trigger each configured corner on every attached monitor. Confirm one action
  fires per deliberate entry, no repeated firing occurs while the pointer stays
  in the corner, and re-entry works after leaving the activation area.
- Change display scaling, rearrange monitors, disconnect/reconnect a monitor,
  and repeat. Confirm corner geometry follows the current monitor topology.
- Exercise enable/disable, configuration persistence, startup behavior, and
  clean exit from the tray menu.

## Cross-cutting product pass

- Repeat the principal flows at 100%, 150%, and 200% display scaling when
  available. Check clipped text, focus indicators, tab order, menus, and dialogs.
- Test keyboard-only operation and Windows high-contrast mode. Inspect the named
  editor, preview, tab, toolbar, status, and document controls with Narrator.
- Run for at least 30 minutes while repeatedly opening, switching, saving, and
  closing documents. Confirm memory remains bounded and no orphan process stays
  after exit.
- Report each issue with application, exact steps, expected result, actual
  result, Windows build, scale, and a screenshot or sample file when relevant.

The release may proceed only when every available item passes and every
environment-limited item is recorded explicitly in the release notes.
