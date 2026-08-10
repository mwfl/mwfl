# Public API probe

Put uncertain generated code in `main.cpp` and compile it before expanding an
application. Configure with:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DMWFL_SOURCE_DIR=C:/src/mwfl
cmake --build build --config Debug
```

The repository test build also compiles this source as `mwfl_agent_api_probe`.

