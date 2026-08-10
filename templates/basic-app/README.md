# mwfl basic application template

Copy this directory, then configure and build on Windows:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

For local mwfl development add `-DMWFL_SOURCE_DIR=C:/path/to/mwfl`.

