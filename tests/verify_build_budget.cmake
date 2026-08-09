if(NOT EXISTS "${LIBRARY}")
    message(FATAL_ERROR "mwtl library does not exist: ${LIBRARY}")
endif()
file(SIZE "${LIBRARY}" library_bytes)
# Debug MSVC libraries include substantial compiler metadata. This is a
# regression ceiling, not a claim about final executable size. The 0.3 custom
# dialog, tray, and control-resource implementations account for roughly 1 MiB
# of independently measured object code. ARM64 Debug COFF archives are larger
# than x64 archives even for the same source; enforce a measured ceiling for
# each architecture instead of making the x64 budget absorb ABI padding.
set(library_limit 15728640) # 15 MiB; current VS 2026 x64 is ~14.14 MiB.
if(MWTL_ARCHITECTURE STREQUAL "ARM64")
    set(library_limit 16252928) # 15.5 MiB; ARM64 COFF adds roughly 0.35 MiB.
endif()
if(library_bytes GREATER library_limit)
    message(FATAL_ERROR
        "mwtl ${MWTL_ARCHITECTURE} static library exceeded ${library_limit} bytes: ${library_bytes}")
endif()

file(GLOB public_headers "${PROJECT_ROOT}/include/mwtl/*.h")
set(core_header_bytes 0)
set(optional_header_bytes 0)
set(optional_headers
    d2d_host.h
    d3d_host.h
    imaging.h
    ole_data.h
    ole_drag_drop.h
    printing.h
    printing_native.h
    printing_settings.h
    settings_store.h
    scintilla.h
    webview2.h)
foreach(header IN LISTS public_headers)
    file(SIZE "${header}" header_bytes)
    get_filename_component(header_name "${header}" NAME)
    if(header_name IN_LIST optional_headers)
        math(EXPR optional_header_bytes "${optional_header_bytes} + ${header_bytes}")
        if(header_bytes GREATER 8192)
            message(FATAL_ERROR
                "optional public header ${header_name} exceeded 8 KiB: ${header_bytes}")
        endif()
    else()
        math(EXPR core_header_bytes "${core_header_bytes} + ${header_bytes}")
    endif()
endforeach()
if(core_header_bytes GREATER 157286)
    message(FATAL_ERROR
        "core top-level public headers exceeded 150 KiB: ${core_header_bytes}")
endif()
if(optional_header_bytes GREATER 49152)
    message(FATAL_ERROR
        "component public headers exceeded 48 KiB: ${optional_header_bytes}")
endif()

file(STRINGS "${PROJECT_ROOT}/include/mwtl/window.h" window_lines)
list(LENGTH window_lines window_line_count)
if(window_line_count GREATER 525)
    message(FATAL_ERROR
        "window.h exceeded its post-refactor 525-line budget: ${window_line_count}")
endif()

message(STATUS
    "build budget: library=${library_bytes}; core headers=${core_header_bytes}; optional headers=${optional_header_bytes}; window.h=${window_line_count} lines")
