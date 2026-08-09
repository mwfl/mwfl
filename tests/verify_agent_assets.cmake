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
string(JSON example_count LENGTH "${examples_json}" examples)
if(NOT example_count EQUAL 27)
    message(FATAL_ERROR "docs/examples.json must describe all 27 examples")
endif()
math(EXPR last_example "${example_count} - 1")
foreach(index RANGE 0 ${last_example})
    string(JSON directory GET "${examples_json}" examples ${index} directory)
    if(NOT EXISTS "${PROJECT_ROOT}/examples/${directory}/main.cpp")
        message(FATAL_ERROR "example metadata points to missing source: ${directory}")
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
   NOT basic_cmake MATCHES "add_executable\\(mwtl_basic_app WIN32")
    message(FATAL_ERROR "basic template must pin v0.1.0 or an immutable revision and build a WIN32 executable")
endif()
