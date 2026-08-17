set(required_files
    docs/agent-usage.md
    docs/common-mistakes.md
    docs/agent-reference.md
    docs/terminology.md
    docs/capabilities.json
    docs/examples.json
    docs/llms.txt
    docs/llms-full.txt
    docs/recipes/index.md
    docs/reference-apps/index.md
    templates/basic-app/CMakeLists.txt
    templates/basic-app/main.cpp
    templates/basic-app/app.manifest
    templates/form-app/CMakeLists.txt
    templates/form-app/main.cpp
    tools/api-probe/CMakeLists.txt
    tools/api-probe/main.cpp
    site/llms.txt
    site/llms-full.txt)

foreach(path IN LISTS required_files)
    if(NOT EXISTS "${PROJECT_ROOT}/${path}")
        message(FATAL_ERROR "coding-agent asset is missing: ${path}")
    endif()
endforeach()

file(READ "${PROJECT_ROOT}/docs/capabilities.json" capabilities)
string(JSON schema_version GET "${capabilities}" schema_version)
string(JSON capability_count LENGTH "${capabilities}" capabilities)
if(NOT schema_version EQUAL 1 OR capability_count LESS 7)
    message(FATAL_ERROR "capabilities.json is incomplete")
endif()

file(READ "${PROJECT_ROOT}/docs/examples.json" examples_json)
file(READ "${PROJECT_ROOT}/examples/CMakeLists.txt" examples_cmake)
file(READ "${PROJECT_ROOT}/examples/README.md" examples_readme)
string(JSON example_count LENGTH "${examples_json}" examples)
file(GLOB example_sources "${PROJECT_ROOT}/examples/*/main.cpp")
list(LENGTH example_sources source_example_count)
if(NOT example_count EQUAL source_example_count)
    message(FATAL_ERROR
        "docs/examples.json describes ${example_count} examples, but ${source_example_count} exist")
endif()
math(EXPR last_example "${example_count} - 1")
foreach(index RANGE 0 ${last_example})
    string(JSON directory GET "${examples_json}" examples ${index} directory)
    if(NOT EXISTS "${PROJECT_ROOT}/examples/${directory}/main.cpp")
        message(FATAL_ERROR "example metadata points to missing source: ${directory}")
    endif()
    if(NOT examples_cmake MATCHES "add_subdirectory\\(${directory}\\)")
        message(FATAL_ERROR "example is not registered with CMake: ${directory}")
    endif()
    if(NOT examples_readme MATCHES "`${directory}`")
        message(FATAL_ERROR "examples README does not catalog: ${directory}")
    endif()
endforeach()

foreach(content_file IN ITEMS docs/agent-usage.md docs/llms.txt docs/llms-full.txt)
    file(READ "${PROJECT_ROOT}/${content_file}" content)
    foreach(term IN ITEMS WindowBase ControlHost WindowWakeup Propagate)
        if(NOT content MATCHES "${term}")
            message(FATAL_ERROR "${content_file} does not cover ${term}")
        endif()
    endforeach()
endforeach()

file(READ "${PROJECT_ROOT}/templates/basic-app/CMakeLists.txt" basic_cmake)
if(NOT basic_cmake MATCHES "GIT_TAG (v0\\.1\\.0|[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]+)" OR
   NOT basic_cmake MATCHES "add_executable\\(mwfl_basic_app WIN32")
    message(FATAL_ERROR "basic template must pin v0.1.0 or an immutable revision and build a WIN32 executable")
endif()
