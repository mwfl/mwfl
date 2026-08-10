# Contributing

mwtl targets x64 and ARM64 Windows applications using C++20 with MSVC-compatible
toolchains. Build and test both Debug and Release before submitting a change.
Public API changes need an
independent-header compile test, runtime coverage when behavior is observable,
and documentation of ownership, threading, and failure behavior.

The 0.1 core follows [the stability policy](docs/stability.md). Breaking a stable
contract requires a deprecation period, migration note, and compatibility
fixture. Provisional APIs may receive source-compatible additions.

```powershell
./scripts/doctor.ps1
./scripts/verify.ps1 -Mode Full -VisualStudio 2026
```

Visual Studio 2022 is the minimum supported IDE and Visual Studio 2026 is
recommended. The scripts accept `-VisualStudio 2022` for compatibility checks.
CI additionally owns AddressSanitizer and native ARM64 validation.

No exception may cross a Win32 callback. UI objects belong to their creating
thread; cross-thread work requires an explicitly documented handoff.
