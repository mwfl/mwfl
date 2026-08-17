# Third-party notices

mwfl is an independent project. Microsoft and the upstream WIL project do not endorse mwfl.

## Windows Implementation Library (WIL)

- Official repository: <https://github.com/microsoft/wil>
- Release tag: `v1.0.260126.7`
- Locked commit: `cbf677fb0a942557d08fd129f4c106a76247b2ec`
- License: MIT
- Upstream license: <https://github.com/microsoft/wil/blob/cbf677fb0a942557d08fd129f4c106a76247b2ec/LICENSE>
- Acquisition: CMake `FetchContent`, or a caller-provided `WIL::WIL` target / `MWFL_WIL_SOURCE_DIR`

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
  `MWFL_BUILD_SCINTILLA=ON`

The core target does not download, link, or deploy Scintilla. The optional
package installs the official `Scintilla.dll`; applications opt into deployment
with `mwfl_deploy_scintilla(target)`.

## Microsoft Edge WebView2 SDK (optional)

- Official package: <https://www.nuget.org/packages/Microsoft.Web.WebView2>
- SDK release: `1.0.4129.50`
- Package SHA-256: `d3934f482d484b89fb4825df720c710664e1143a1e90f7b3a60794ef33f473d2`
- License: Microsoft software license terms included as `LICENSE.txt` in the
  official NuGet package and installed with the optional component
- Acquisition: hash-verified CMake `FetchContent` only when
  `MWFL_BUILD_WEBVIEW2=ON`

The core target does not download or link WebView2. The optional component
links the official x64 static loader; applications use an installed Evergreen
WebView2 Runtime, whose absence is reported as a structured runtime result.
