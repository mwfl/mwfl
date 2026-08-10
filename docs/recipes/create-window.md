# Create a native window and controls

Use `WindowBase`, `ControlHost`, member controls, retained layout, and
`RunApplication`. Start with `templates/basic-app`; the complete compiled source
is `templates/basic-app/main.cpp`.

Required public surface: `<mwfl/mwfl.h>`, `WindowBase`, `Label`, `Button`,
`ControlHost`, `Column`, `EventResult`, and `RunApplication`.

Creation order matters: construct the C++ window, enter `BuildUI()`, create
native child controls through `ControlHost`, then install a layout. Unhandled
commands return `Propagate()`.

Build the copied template with:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

Do not make controls local variables and do not omit the manifest.

