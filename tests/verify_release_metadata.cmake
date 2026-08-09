file(READ "${PROJECT_ROOT}/CMakeLists.txt" cmake_text)
file(READ "${PROJECT_ROOT}/.github/workflows/release.yml" release_workflow_text)
file(READ "${PROJECT_ROOT}/.github/workflows/ci.yml" ci_workflow_text)

string(REGEX MATCH "project\\(mwtl VERSION ([0-9]+\\.[0-9]+\\.[0-9]+)" _ "${cmake_text}")
set(project_version "${CMAKE_MATCH_1}")
if(project_version STREQUAL "")
    message(FATAL_ERROR "Could not read the mwtl project version")
endif()
if(NOT EXISTS "${PROJECT_ROOT}/cmake/mwtlConfig.cmake.in" OR
   NOT EXISTS "${PROJECT_ROOT}/.github/workflows/release.yml")
    message(FATAL_ERROR "Release package metadata is incomplete")
endif()
if(NOT project_version STREQUAL "0.1.0")
    message(FATAL_ERROR "First public release must use project version 0.1.0")
endif()
if(NOT EXISTS "${PROJECT_ROOT}/tests/consumer_01_compat.cpp")
    message(FATAL_ERROR "0.1 source compatibility fixture is missing")
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
foreach(architecture IN ITEMS x64 ARM64)
    if(NOT release_workflow_text MATCHES "architecture: ${architecture}")
        message(FATAL_ERROR "Release workflow does not package ${architecture}")
    endif()
endforeach()
foreach(generator IN ITEMS "Visual Studio 18 2026" "Visual Studio 17 2022")
    if(NOT release_workflow_text MATCHES "${generator}")
        message(FATAL_ERROR "Release workflow is missing generator: ${generator}")
    endif()
endforeach()
foreach(versioned_file IN ITEMS
        site/building.html
        templates/basic-app/CMakeLists.txt templates/form-app/CMakeLists.txt)
    file(READ "${PROJECT_ROOT}/${versioned_file}" versioned_text)
    if(NOT versioned_text MATCHES "v0\\.1\\.0")
        message(FATAL_ERROR "${versioned_file} does not pin the v0.1.0 release")
    endif()
    if(versioned_text MATCHES "v0\\.[356]\\.0|e0c162b")
        message(FATAL_ERROR "${versioned_file} contains a stale pre-public version")
    endif()
endforeach()
file(READ "${PROJECT_ROOT}/README.md" readme_text)
if(readme_text MATCHES "v[0-9]+\\.[0-9]+\\.[0-9]+")
    message(FATAL_ERROR "README must remain version-neutral")
endif()
if(NOT release_workflow_text MATCHES [[gh release create]] OR
   NOT release_workflow_text MATCHES [[artifacts/\*\.zip]] OR
   NOT release_workflow_text MATCHES [[artifacts/SHA256SUMS-\*\.txt]])
    message(FATAL_ERROR "Release workflow does not publish only ZIP packages and checksums")
endif()
