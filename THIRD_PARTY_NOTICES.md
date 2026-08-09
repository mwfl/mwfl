# Third-party notices

mwtl is an independent project. Microsoft and the upstream WTL and WIL projects do not endorse mwtl.

## Windows Template Library (WTL)

- Official repository: <https://git.code.sf.net/p/wtl/git>
- Release version: `10.01` (upstream does not publish a corresponding Git tag)
- Locked commit: `011be908a1122e7bc9fd1106ecc48f22f5f86f00`
- License: Microsoft Public License (Ms-PL)
- Upstream license notice: each distributed WTL header contains the Ms-PL notice and links to <https://opensource.org/license/ms-pl-html>
- Acquisition: CMake `FetchContent`, or a caller-provided `WTL::WTL` target / `MWTL_WTL_SOURCE_DIR`

WTL is consumed from source and is not copied into this repository.

## Windows Implementation Library (WIL)

- Official repository: <https://github.com/microsoft/wil>
- Release tag: `v1.0.260126.7`
- Locked commit: `cbf677fb0a942557d08fd129f4c106a76247b2ec`
- License: MIT
- Upstream license: <https://github.com/microsoft/wil/blob/cbf677fb0a942557d08fd129f4c106a76247b2ec/LICENSE>
- Acquisition: CMake `FetchContent`, or a caller-provided `WIL::WIL` target / `MWTL_WIL_SOURCE_DIR`

WIL is consumed from source and is not copied into this repository.

## Scintilla (optional)

- Official site: <https://www.scintilla.org/>
- Release: `5.6.5`
- Source archive: `scintilla565.zip`
- Source SHA-256: `345140a60bf4ceea3340942e8205e6a8fbda8db13eb48f827126b9be15bd3da1`
- Official x64 runtime archive: `wscite565.zip`
- Runtime archive SHA-256: `9b5a7af4beb2d61ba6a5f62fa678ff68c96d175d19664fc1a801f444b357303b`
- License: Scintilla license (BSD-style; the exact `License.txt` from the
  source archive is installed with the optional component)
- Acquisition: hash-verified CMake `FetchContent` only when
  `MWTL_BUILD_SCINTILLA=ON`

The core target does not download, link, or deploy Scintilla. The optional
package installs the official `Scintilla.dll`; applications opt into deployment
with `mwtl_deploy_scintilla(target)`.
