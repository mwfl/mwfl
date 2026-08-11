set(required_files
    AGENTS.md
    CLAUDE.md
    .editorconfig
    .clang-format
    .vsconfig
    CMakePresets.json
    docs/development-architecture.md
    docs/change-matrix.json
    docs/scope-map.md
    docs/api-index.json
    docs/public-api-contract-audit.md
    scripts/developer-tools.ps1
    scripts/doctor.ps1
    scripts/new-mwfl-app.ps1
    scripts/verify.ps1
    scripts/verify-change.ps1)
foreach(path IN LISTS required_files)
    if(NOT EXISTS "${PROJECT_ROOT}/${path}")
        message(FATAL_ERROR "developer-agent asset is missing: ${path}")
    endif()
endforeach()

file(READ "${PROJECT_ROOT}/scripts/verify-change.ps1" change_selector)
foreach(term IN ITEMS change-matrix.json ChangedFiles Execute VerifyMode)
    if(NOT change_selector MATCHES "${term}")
        message(FATAL_ERROR "verify-change.ps1 does not cover ${term}")
    endif()
endforeach()

file(READ "${PROJECT_ROOT}/.vsconfig" vsconfig)
foreach(component IN ITEMS
        Microsoft.VisualStudio.Workload.NativeDesktop
        Microsoft.VisualStudio.Component.VC.CMake.Project
        Microsoft.VisualStudio.Component.VC.ATL
        Microsoft.VisualStudio.Component.VC.Tools.ARM64)
    if(NOT vsconfig MATCHES "${component}")
        message(FATAL_ERROR ".vsconfig is missing required component: ${component}")
    endif()
endforeach()

file(READ "${PROJECT_ROOT}/CMakePresets.json" presets)
foreach(name IN ITEMS
        vs2022-x64 vs2022-arm64 vs2026-x64 vs2026-arm64
        vs2022-x64-debug vs2022-x64-release
        vs2026-x64-debug vs2026-x64-release)
    if(NOT presets MATCHES "\"name\"[ \t]*:[ \t]*\"${name}\"")
        message(FATAL_ERROR "required CMake preset is missing: ${name}")
    endif()
endforeach()

file(READ "${PROJECT_ROOT}/docs/change-matrix.json" change_matrix)
string(JSON schema_version GET "${change_matrix}" schema_version)
string(JSON path_count LENGTH "${change_matrix}" paths)
if(NOT schema_version EQUAL 1 OR path_count LESS 10)
    message(FATAL_ERROR "development change matrix is incomplete")
endif()

file(READ "${PROJECT_ROOT}/AGENTS.md" agents)
foreach(term IN ITEMS doctor.ps1 verify.ps1 "Visual Studio 2022" "Visual Studio 2026")
    if(NOT agents MATCHES "${term}")
        message(FATAL_ERROR "AGENTS.md does not cover ${term}")
    endif()
endforeach()

file(READ "${PROJECT_ROOT}/scripts/verify.ps1" verify_script)
foreach(mode IN ITEMS Fast Full Docs Package)
    if(NOT verify_script MATCHES "'${mode}'")
        message(FATAL_ERROR "verify.ps1 does not implement ${mode} mode")
    endif()
endforeach()

file(READ "${PROJECT_ROOT}/.github/workflows/ci.yml" ci_workflow)
foreach(term IN ITEMS windows-2022 windows-2025-vs2026 vs2022-x64-release vs2026-x64-debug)
    if(NOT ci_workflow MATCHES "${term}")
        message(FATAL_ERROR "CI does not cover required toolchain marker: ${term}")
    endif()
endforeach()
foreach(term IN ITEMS vs2026-x64-optional-debug vs2026-x64-optional-release session_parser_fuzz)
    if(NOT ci_workflow MATCHES "${term}")
        message(FATAL_ERROR "CI does not cover optional/reliability marker: ${term}")
    endif()
endforeach()
foreach(term IN ITEMS windows-11-arm "Visual Studio 17 2022" ARM64)
    if(NOT ci_workflow MATCHES "${term}")
        message(FATAL_ERROR "CI does not cover native ARM64 marker: ${term}")
    endif()
endforeach()
