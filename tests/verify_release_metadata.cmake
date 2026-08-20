file(READ "${PROJECT_ROOT}/CMakeLists.txt" cmake_text)
file(READ "${PROJECT_ROOT}/.github/workflows/release.yml" release_workflow_text)
file(READ "${PROJECT_ROOT}/.github/workflows/ci.yml" ci_workflow_text)

string(REGEX MATCH "project\\(mwfl VERSION ([0-9]+\\.[0-9]+\\.[0-9]+)" _ "${cmake_text}")
set(project_version "${CMAKE_MATCH_1}")
if(project_version STREQUAL "")
    message(FATAL_ERROR "Could not read the mwfl project version")
endif()
if(NOT EXISTS "${PROJECT_ROOT}/cmake/mwflConfig.cmake.in" OR
   NOT EXISTS "${PROJECT_ROOT}/.github/workflows/release.yml")
    message(FATAL_ERROR "Release package metadata is incomplete")
endif()
if(NOT project_version STREQUAL "0.3.0")
    message(FATAL_ERROR "Current public release must use project version 0.3.0")
endif()
if(NOT EXISTS "${PROJECT_ROOT}/docs/release-readiness-0.3.0.md")
    message(FATAL_ERROR "0.3.0 release-readiness plan is missing")
endif()
if(NOT EXISTS "${PROJECT_ROOT}/tests/consumer_public_baseline.cpp")
    message(FATAL_ERROR "First-public-preview source baseline is missing")
endif()
if(NOT EXISTS "${PROJECT_ROOT}/tests/verify_coverage.ps1" OR
   NOT ci_workflow_text MATCHES "MinimumPercent 74" OR
   NOT ci_workflow_text MATCHES "output-format cobertura")
    message(FATAL_ERROR "Native source coverage gate is incomplete")
endif()
foreach(package_document IN ITEMS
        README.md SECURITY.md docs/api.md docs/design.md
        docs/stability.md docs/accessibility.md
        docs/reference.md docs/system-message-recipes.md)
    if(NOT cmake_text MATCHES "install\\([^)]*${package_document}")
        message(FATAL_ERROR
            "Release package does not install ${package_document}")
    endif()
endforeach()
if(NOT release_workflow_text MATCHES "Visual Studio 18 2026" OR
   NOT release_workflow_text MATCHES "architecture: x64" OR
   NOT release_workflow_text MATCHES "architecture: ARM64" OR
   NOT release_workflow_text MATCHES "windows-11-arm" OR
   NOT release_workflow_text MATCHES "attest-build-provenance")
    message(FATAL_ERROR "Release workflow must package and attest x64 and ARM64 artifacts")
endif()
foreach(versioned_file IN ITEMS
        site/building.html
        templates/basic-app/CMakeLists.txt templates/form-app/CMakeLists.txt)
    file(READ "${PROJECT_ROOT}/${versioned_file}" versioned_text)
    if(NOT versioned_text MATCHES "v0\\.3\\.0")
        message(FATAL_ERROR "${versioned_file} does not pin the v0.3.0 release")
    endif()
    if(versioned_text MATCHES "v0\\.[56]\\.0|e0c162b")
        message(FATAL_ERROR "${versioned_file} contains a stale pre-public version")
    endif()
endforeach()
file(READ "${PROJECT_ROOT}/README.md" readme_text)
if(NOT readme_text MATCHES "GIT_TAG v0\\.3\\.0" OR
   readme_text MATCHES "GIT_TAG main|completed 0\\.8|post-0\\.8")
    message(FATAL_ERROR "README does not describe the 0.3 public-preview release consistently")
endif()
if(NOT EXISTS "${PROJECT_ROOT}/docs/releases/v0.3.0.md")
    message(FATAL_ERROR "0.3.0 release notes are missing")
endif()
if(release_workflow_text MATCHES "package-markdown-editor|package-pdf-viewer" OR
   NOT release_workflow_text MATCHES [[Count -ne 2]] OR
   NOT release_workflow_text MATCHES [[Expected core and Notepad ZIP packages]])
    message(FATAL_ERROR "Release workflow does not match the current two-package release scope")
endif()
if(NOT release_workflow_text MATCHES [[gh release create]] OR
   NOT release_workflow_text MATCHES [[artifacts/\*\.zip]] OR
   NOT release_workflow_text MATCHES [[artifacts/SHA256SUMS-\*\.txt]] OR
   NOT release_workflow_text MATCHES [[docs/releases/\$\{\{ github\.ref_name \}\}\.md]])
    message(FATAL_ERROR "Release workflow does not publish only ZIP packages and checksums")
endif()
