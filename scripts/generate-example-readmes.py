"""Create self-contained README pages for compiled mwfl examples.

Existing READMEs are preserved so product examples can carry hand-written tours.
The generated pages use docs/examples.json for intent/symbols and quote a real
class declaration from main.cpp instead of inventing sample code.
"""

from __future__ import annotations

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EXAMPLES = ROOT / "examples"
IMAGES = ROOT / "docs" / "images" / "examples"


def title(name: str) -> str:
    special = {"dpi": "DPI", "mdi": "MDI", "ole": "OLE", "pdf": "PDF", "com": "COM"}
    return " ".join(special.get(part, part.capitalize()) for part in name.split("_"))


def target_for(directory: Path) -> str:
    text = (directory / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"mwfl_add_example\(\s*([^\s\)]+)", text)
    if not match:
        raise RuntimeError(f"No mwfl_add_example target in {directory}")
    return match.group(1)


def excerpt_for(source: Path) -> str:
    lines = source.read_text(encoding="utf-8").splitlines()
    start = next((i for i, line in enumerate(lines) if re.match(r"class\s+\w+.*Window", line)), None)
    if start is None:
        start = next((i for i, line in enumerate(lines) if "void BuildUI()" in line), None)
    if start is None:
        start = next((i for i, line in enumerate(lines) if "mwfl::" in line and not line.startswith("using ")), 0)
    excerpt = lines[start : start + 18]
    return "\n".join(excerpt).rstrip()


def source_links(directory: Path) -> str:
    files = [path.name for path in sorted(directory.iterdir()) if path.suffix in {".cpp", ".h", ".rc"}]
    return ", ".join(f"[`{name}`]({name})" for name in files)


def render(entry: dict[str, object], directory: Path) -> str:
    name = str(entry["directory"])
    display = title(name)
    intent = str(entry["intent"])
    symbols = [str(item) for item in entry.get("symbols", [])]
    tests = [str(item) for item in entry.get("tests", [])]
    optional = entry.get("optional_component")
    target = target_for(directory)
    slug = name.replace("_", "-")
    configure_preset = "vs2026-x64-optional" if optional else "vs2026-x64"
    build_preset = f"{configure_preset}-debug"
    bullets = "\n".join(f"- `{symbol}`" for symbol in symbols)
    validation = (
        "The focused validation targets are " + ", ".join(f"`{test}`" for test in tests) + "."
        if tests
        else "This example is compiled in both Debug and Release and participates in the example catalog checks."
    )
    optional_note = (
        f" This example uses the optional `{optional}` component; the optional preset shown above enables its pinned dependency and runtime staging rules."
        if optional
        else ""
    )
    return f"""# {display}

This compiled example demonstrates **{intent}**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![{display} example running on Windows](../../docs/images/examples/{slug}.png)

## What it demonstrates

{bullets}

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
{excerpt_for(directory / 'main.cpp')}
```

Read the complete implementation in {source_links(directory)}.

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset {configure_preset}
cmake --build --preset {build_preset} --target {target}
build\\presets\\{configure_preset}\\examples\\{name}\\Debug\\{target}.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.{optional_note}

## Validation

{validation} Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
"""


def main() -> None:
    manifest = json.loads((ROOT / "docs" / "examples.json").read_text(encoding="utf-8"))
    created: list[str] = []
    for entry in manifest["examples"]:
        name = str(entry["directory"])
        directory = EXAMPLES / name
        readme = directory / "README.md"
        image = IMAGES / f"{name.replace('_', '-')}.png"
        if not directory.is_dir() or not (directory / "CMakeLists.txt").exists():
            raise RuntimeError(f"Example directory is missing: {name}")
        if not image.exists():
            raise RuntimeError(f"Capture the example screenshot before generating: {image}")
        if readme.exists():
            continue
        readme.write_text(render(entry, directory), encoding="utf-8", newline="\n")
        created.append(name)
    print(f"Created {len(created)} example README files")
    if created:
        print("\n".join(created))


if __name__ == "__main__":
    main()
