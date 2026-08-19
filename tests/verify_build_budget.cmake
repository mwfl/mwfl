cmake_minimum_required(VERSION 3.21)

if(NOT EXISTS "${LIBRARY}")
    message(FATAL_ERROR "mwfl library does not exist: ${LIBRARY}")
endif()
file(SIZE "${LIBRARY}" library_bytes)
# Debug MSVC libraries include substantial compiler metadata. This is a
# regression ceiling, not a claim about final executable size. The 0.3 custom
# dialog, tray, and control-resource implementations account for roughly 1 MiB
# of independently measured object code. ARM64 Debug COFF archives are larger
# than x64 archives even for the same source; enforce a measured ceiling for
# each architecture instead of making the x64 budget absorb ABI padding.
# 0.7 adds nine docking model/native/persistence units. Their independently
# measured VS2026 x64 Debug COFF objects total 6,131,009 bytes; the complete
# archive is 25,233,412 bytes (Release is 8,936,512 bytes). Keep less than 8%
# Debug headroom instead of hiding this deliberate core capability increase.
set(library_limit 27262976) # 26 MiB; 0.7 measures 24.06 MiB on VS2026 x64.
if(MWFL_ARCHITECTURE STREQUAL "ARM64")
    # The post-0.8 VS2022 ARM64 Debug archive measures 26,182,312 bytes
    # (24.97 MiB). Keep a measured ceiling with less than 12% headroom.
    set(library_limit 30408704) # 29 MiB.
endif()
if(library_bytes GREATER library_limit)
    message(FATAL_ERROR
        "mwfl ${MWFL_ARCHITECTURE} static library exceeded ${library_limit} bytes: ${library_bytes}")
endif()

file(GLOB public_headers "${PROJECT_ROOT}/include/mwfl/*.h")
file(GLOB_RECURSE detail_headers "${PROJECT_ROOT}/include/mwfl/detail/*.h")
file(GLOB_RECURSE app_support_headers "${PROJECT_ROOT}/include/mwfl/app_support/*.h")
set(core_header_bytes 0)
set(optional_header_bytes 0)
set(foundation_header_bytes 0)
set(detail_header_bytes 0)
set(optional_headers
    d2d_host.h
    d3d_host.h
    imaging.h
    file_association.h
    ole_data.h
    ole_drag_drop.h
    printing.h
    printing_native.h
    printing_settings.h
    ribbon.h
    mdi.h
    graphics.h
    help.h
    settings_store.h
    shell_integration.h
    scintilla.h
    webview2.h)
set(foundation_headers
    core.h
    service.h
    process.h
    ipc.h
    diagnostics.h
    security.h
    deployment.h
    scheduler.h)
foreach(header IN LISTS public_headers)
    file(SIZE "${header}" header_bytes)
    get_filename_component(header_name "${header}" NAME)
    if(header_name IN_LIST foundation_headers)
        math(EXPR foundation_header_bytes "${foundation_header_bytes} + ${header_bytes}")
        if(header_bytes GREATER 8192)
            message(FATAL_ERROR
                "foundation public header ${header_name} exceeded 8 KiB: ${header_bytes}")
        endif()
    elseif(header_name IN_LIST optional_headers)
        math(EXPR optional_header_bytes "${optional_header_bytes} + ${header_bytes}")
        if(header_bytes GREATER 8192)
            message(FATAL_ERROR
                "optional public header ${header_name} exceeded 8 KiB: ${header_bytes}")
        endif()
    else()
        math(EXPR core_header_bytes "${core_header_bytes} + ${header_bytes}")
    endif()
endforeach()
foreach(header IN LISTS app_support_headers)
    file(SIZE "${header}" header_bytes)
    math(EXPR optional_header_bytes "${optional_header_bytes} + ${header_bytes}")
    if(header_bytes GREATER 8192)
        message(FATAL_ERROR
            "application-support public header exceeded 8 KiB: ${header_bytes}")
    endif()
endforeach()
foreach(header IN LISTS detail_headers)
    file(SIZE "${header}" header_bytes)
    math(EXPR detail_header_bytes "${detail_header_bytes} + ${header_bytes}")
endforeach()
# The nine focused 0.7 docking headers total 23,136 bytes. Keep the aggregate
# ceiling close to the measured 190,071-byte public core surface.
if(core_header_bytes GREATER 204800)
    message(FATAL_ERROR
        "core top-level public headers exceeded 200 KiB: ${core_header_bytes}")
endif()
if(optional_header_bytes GREATER 73728)
    message(FATAL_ERROR
        "component public headers exceeded 72 KiB: ${optional_header_bytes}")
endif()
if(foundation_header_bytes GREATER 65536)
    message(FATAL_ERROR
        "foundation public headers exceeded 64 KiB: ${foundation_header_bytes}")
endif()
if(detail_header_bytes GREATER 8192)
    message(FATAL_ERROR
        "detail public header implementation exceeded 8 KiB: ${detail_header_bytes}")
endif()

file(STRINGS "${PROJECT_ROOT}/include/mwfl/window.h" window_lines)
list(LENGTH window_lines window_line_count)
# The native class-registration and HWND-binding engine replaces the former
# external frame base while keeping the public template self-contained.
if(window_line_count GREATER 540)
    message(FATAL_ERROR
        "window.h exceeded its native-engine 540-line budget: ${window_line_count}")
endif()

message(STATUS
    "build budget: library=${library_bytes}; core headers=${core_header_bytes}; optional headers=${optional_header_bytes}; foundation headers=${foundation_header_bytes}; detail headers=${detail_header_bytes}; window.h=${window_line_count} lines")
