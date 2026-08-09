if(NOT EXISTS "${LIBRARY}")
    message(FATAL_ERROR "mwtl library does not exist: ${LIBRARY}")
endif()
file(SIZE "${LIBRARY}" library_bytes)
# Debug MSVC libraries include substantial compiler metadata. This is a
# regression ceiling, not a claim about final executable size. The 0.3 custom
# dialog implementation accounts for roughly 622 KiB of independently measured
# object code; keep less than 0.5 MiB of headroom above the current library.
if(library_bytes GREATER 13631488)
    message(FATAL_ERROR "mwtl static library exceeded 13 MiB: ${library_bytes}")
endif()

file(GLOB public_headers "${PROJECT_ROOT}/include/mwtl/*.h")
set(public_header_bytes 0)
foreach(header IN LISTS public_headers)
    file(SIZE "${header}" header_bytes)
    math(EXPR public_header_bytes "${public_header_bytes} + ${header_bytes}")
endforeach()
if(public_header_bytes GREATER 157286)
    message(FATAL_ERROR
        "top-level public headers exceeded 150 KiB: ${public_header_bytes}")
endif()

file(STRINGS "${PROJECT_ROOT}/include/mwtl/window.h" window_lines)
list(LENGTH window_lines window_line_count)
if(window_line_count GREATER 525)
    message(FATAL_ERROR
        "window.h exceeded its post-refactor 525-line budget: ${window_line_count}")
endif()

message(STATUS
    "build budget: library=${library_bytes}; headers=${public_header_bytes}; window.h=${window_line_count} lines")
