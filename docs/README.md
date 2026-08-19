# mwfl documentation map

Choose the smallest layer that solves the application problem. Every layer
keeps real HWND and Win32 interoperability available.

## Core

Start here for application/window lifetime, controls, typed events, layout,
DPI, timers, and worker-to-UI handoff.

- [First application](tutorials/notepad.md)
- [Create a window](recipes/create-window.md)
- [Responsive layout](recipes/responsive-layout.md)
- [Background work](recipes/background-work.md)
- [Public header reference](reference.md)

## Application

Use these APIs for commands, forms, documents, settings, persistence, dialogs,
and complete desktop workflows.

- [Form validation](recipes/form-validation.md)
- [Commands, menu, and toolbar](recipes/commands-menu-toolbar.md)
- [Document workspace](tutorials/document-workspace.md)
- [Settings application](tutorials/settings-application.md)
- [Reference applications](reference-apps/index.md)

## Advanced

Add these focused native components when an application needs docking,
printing, OLE, Shell integration, Ribbon, MDI, or graphics interoperability.

- [Docking workspace](tutorials/docking-workspace.md)
- [Printing, OLE, and Shell](tutorials/printing-ole-shell.md)
- [Ribbon](tutorials/ribbon.md)
- [MDI](tutorials/mdi.md)
- [Graphics and Help](tutorials/graphics-help.md)

## Optional

These separately linked components host Direct2D/Direct3D, WIC images,
WebView2, and Scintilla without expanding the core dependency surface.

- [Drawing](recipes/direct2d-drawing.md)
- [Direct3D swap chain](recipes/direct3d-swap-chain.md)
- [Image viewer](tutorials/image-viewer.md)
- [Browser](tutorials/browser.md)
- [Code editor](tutorials/code-editor.md)
- [Markdown editor](tutorials/markdown-editor.md)

Applications that publish GitHub Releases can use
`mwfl::app_support::UpdateChecker` through the default `mwfl::app` target. It
provides a bounded latest-version check, reminder policy, and explicit
automatic-check opt-out. It opens the official Release page after confirmation
and never downloads or replaces binaries.

Before changing public API, read the [0.1 contract audit](public-api-contract-audit.md),
[stability policy](stability.md), and [development architecture](development-architecture.md).
