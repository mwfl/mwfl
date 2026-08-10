get_filename_component(PROJECT_ROOT "${SITE_ROOT}" DIRECTORY)

foreach(required IN ITEMS
        index.html
        tutorial.html
        building.html
        changelog.html
        optional-integrations.html
        notepad.html
        components/index.html
        components/catalog.html
        components/application.html
        components/controls.html
        components/common-controls.html
        components/window.html
        components/hello.html
        assets/mwtl-mark.svg
        assets/site.js
        styles.css
        llms.txt
        llms-full.txt
        .nojekyll)
    if(NOT EXISTS "${SITE_ROOT}/${required}")
        message(FATAL_ERROR "required Pages file is missing: ${required}")
    endif()
endforeach()

file(READ "${SITE_ROOT}/components/catalog.html" component_catalog)
file(READ "${PROJECT_ROOT}/docs/capabilities.json" capabilities_json)
string(JSON capability_count LENGTH "${capabilities_json}" capabilities)
math(EXPR last_capability "${capability_count} - 1")
foreach(index RANGE 0 ${last_capability})
    string(JSON capability_id GET "${capabilities_json}" capabilities ${index} id)
    if(NOT component_catalog MATCHES "data-capability-id=\"${capability_id}\"")
        message(FATAL_ERROR "Pages catalog is missing capability: ${capability_id}")
    endif()
endforeach()

file(GLOB usage_docs RELATIVE "${PROJECT_ROOT}"
    "${PROJECT_ROOT}/docs/recipes/*.md"
    "${PROJECT_ROOT}/docs/tutorials/*.md")
list(FILTER usage_docs EXCLUDE REGEX "docs/recipes/index\\.md$")
foreach(usage_doc IN LISTS usage_docs)
    if(NOT component_catalog MATCHES "data-usage-path=\"${usage_doc}\"")
        message(FATAL_ERROR "Pages catalog is missing usage document: ${usage_doc}")
    endif()
endforeach()

if(NOT component_catalog MATCHES "component-search" OR
   NOT component_catalog MATCHES "data-catalog-filter" OR
   NOT component_catalog MATCHES "release-readiness.md")
    message(FATAL_ERROR "Pages catalog is missing search, filters, or the next plan")
endif()

file(READ "${SITE_ROOT}/tutorial.html" tutorial)
foreach(marker IN ITEMS
        "15 minutes"
        "Desktop development with C\\+\\+"
        "C\\+\\+ ATL"
        "Developer PowerShell"
        "git --version"
        "cmake --version"
        "CMakeLists.txt"
        "main.cpp"
        "app.manifest"
        "cmake -S . -B build"
        "cmake --build build"
        "mwtl_hello.exe"
        "Visual Studio 18 2026"
        "Visual Studio 17 2022"
        "Troubleshooting")
    if(NOT tutorial MATCHES "${marker}")
        message(FATAL_ERROR "beginner tutorial is missing required marker: ${marker}")
    endif()
endforeach()

file(READ "${SITE_ROOT}/building.html" building)
if(NOT building MATCHES "tutorial.html" OR
   NOT building MATCHES "scripts/doctor.ps1" OR
   NOT building MATCHES "scripts/verify.ps1")
    message(FATAL_ERROR "build reference is missing beginner or contributor guidance")
endif()

file(READ "${SITE_ROOT}/optional-integrations.html" optional_integrations)
foreach(marker IN ITEMS "mwtl::ribbon" "mwtl::mdi" "mwtl::graphics"
        "TaskbarCreated" "HelpRequest" "docs/tutorials/ribbon.md"
        "docs/tutorials/mdi.md" "docs/tutorials/graphics-help.md")
    if(NOT optional_integrations MATCHES "${marker}")
        message(FATAL_ERROR "optional integration guide is missing marker: ${marker}")
    endif()
endforeach()

file(READ "${SITE_ROOT}/notepad.html" notepad)
foreach(marker IN ITEMS "templates/basic-app" "DocumentState" "WriteTextFileAtomic"
        "mwtl.notepad_gui" "mwtl_notepad.exe" "Visual Studio 18 2026"
        "Visual Studio 17 2022")
    if(NOT notepad MATCHES "${marker}")
        message(FATAL_ERROR "Notepad tutorial is missing required marker: ${marker}")
    endif()
endforeach()

file(READ "${SITE_ROOT}/changelog.html" changelog)
foreach(marker IN ITEMS "First public release" "0.1.0" "releases/tag/v0.1.0" "x64" "ARM64")
    if(NOT changelog MATCHES "${marker}")
        message(FATAL_ERROR "changelog is missing release marker: ${marker}")
    endif()
endforeach()

file(READ "${SITE_ROOT}/styles.css" site_css)
if(NOT site_css MATCHES "@media \\(max-width: 480px\\)" OR
   NOT site_css MATCHES "flex: 1 0 100%" OR
   NOT site_css MATCHES "overflow-wrap: anywhere")
    message(FATAL_ERROR "narrow-screen layout guard is missing")
endif()

file(READ "${SITE_ROOT}/assets/site.js" site_script)
if(NOT site_script MATCHES "aria-pressed" OR
   NOT site_script MATCHES "Switch to.*theme")
    message(FATAL_ERROR "theme toggle state is not accessible")
endif()
if(NOT site_script MATCHES "showModal" OR
   NOT site_script MATCHES "target=\\\"_blank\\\"")
    message(FATAL_ERROR "example preview dialog is missing")
endif()

file(GLOB_RECURSE html_files "${SITE_ROOT}/*.html")
foreach(html_file IN LISTS html_files)
    file(READ "${html_file}" html)
    file(READ "${html_file}" html_hex HEX)
    string(TOLOWER "${html_hex}" html_hex)
    foreach(mojibake_hex IN ITEMS
            c3a2c282c294
            c382c2b7
            c3a2c280c2a6
            c3a2c280c2a0)
        if(html_hex MATCHES "${mojibake_hex}")
            message(FATAL_ERROR "UTF-8 mojibake in ${html_file}: ${mojibake_hex}")
        endif()
    endforeach()
    if(NOT html MATCHES "styles\\.css\\?v=20260810a" OR
       NOT html MATCHES "assets/site\\.js\\?v=20260810a")
        message(FATAL_ERROR "stale or inconsistent site asset version in ${html_file}")
    endif()
    string(REGEX MATCHALL "href=\"[^\"]+\"" hrefs "${html}")
    get_filename_component(html_directory "${html_file}" DIRECTORY)

    foreach(href IN LISTS hrefs)
        string(REGEX REPLACE "^href=\"|\"$" "" link "${href}")
        if(link MATCHES "^(https://|http://|mailto:|#)")
            continue()
        endif()
        string(REGEX REPLACE "#.*$" "" link_path "${link}")
        string(REGEX REPLACE "\\?.*$" "" link_path "${link_path}")
        if(link_path STREQUAL "")
            continue()
        endif()
        get_filename_component(target "${html_directory}/${link_path}" ABSOLUTE)
        if(NOT EXISTS "${target}")
            message(FATAL_ERROR "broken local link in ${html_file}: ${link}")
        endif()
    endforeach()
endforeach()
