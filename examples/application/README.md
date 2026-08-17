# `mwfl::Application` demo

![Application lifecycle example running on Windows](../../docs/images/examples/application.png)

This executable demonstrates the complete milestone-1 application lifecycle:

1. Construct `Application` from the non-owning process `HINSTANCE`.
2. Observe the same handle with `GetInstance()`.
3. Call `Run<ApplicationWindow>(show_command)`.
4. Return its message-loop result as the process exit code.
5. Return its result through the process entry point. Most applications can use
the one-line `RunApplication<Window>()` helper instead.

`Application` is intentionally non-copyable and non-movable. It activates its `mwfl::MessageLoop` on the UI thread, owns the stack lifetime of `ApplicationWindow`, and releases everything in reverse order. An `Application` may run repeatedly in one process.

## Key code

The complete application lifecycle is intentionally visible at the entry point:

```cpp
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    try {
        mwfl::Application application(instance);
        if (application.GetInstance() != instance) {
            return EXIT_FAILURE;
        }
        return application.Run<ApplicationWindow>(show_command);
    } catch (const std::exception& error) {
        ::OutputDebugStringA(error.what());
    }
    return EXIT_FAILURE;
}
```

See [`main.cpp`](main.cpp) for the window and exit-code behavior.

## Build and run

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_application_demo
build\presets\vs2026-x64\examples\application\Debug\mwfl_application_demo.exe
```

Run `./scripts/verify-change.ps1 -Execute` after changing the example.
