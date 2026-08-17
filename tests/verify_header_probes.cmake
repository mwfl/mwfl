file(GLOB public_header_paths "${PROJECT_ROOT}/include/mwfl/*.h")
file(GLOB probe_paths "${PROJECT_ROOT}/tests/header_*.cpp")

set(public_headers "")
foreach(path IN LISTS public_header_paths)
    get_filename_component(name "${path}" NAME_WE)
    list(APPEND public_headers "${name}")
endforeach()

set(probe_headers "")
foreach(path IN LISTS probe_paths)
    get_filename_component(name "${path}" NAME_WE)
    string(REGEX REPLACE "^header_" "" name "${name}")
    if(name STREQUAL "umbrella")
        set(name "mwfl")
    endif()
    list(APPEND probe_headers "${name}")
endforeach()

list(SORT public_headers)
list(SORT probe_headers)
if(NOT public_headers STREQUAL probe_headers)
    message(FATAL_ERROR
        "Public-header/probe mismatch. Public: ${public_headers}; probes: ${probe_headers}")
endif()

message(STATUS "all public headers have independent compile probes")
