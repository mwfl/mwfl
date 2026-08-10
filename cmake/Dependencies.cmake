include(FetchContent)

set(MWFL_DEPENDENCY_MODE "AUTO" CACHE STRING
    "Dependency resolution: AUTO, FETCH, or SYSTEM")
set_property(CACHE MWFL_DEPENDENCY_MODE PROPERTY STRINGS AUTO FETCH SYSTEM)

set(MWFL_WTL_SOURCE_DIR "" CACHE PATH "Path to an existing official WTL source tree")
set(MWFL_WIL_SOURCE_DIR "" CACHE PATH "Path to an existing Microsoft WIL source tree")

set(MWFL_WTL_REPOSITORY "https://git.code.sf.net/p/wtl/git")
set(MWFL_WTL_TAG "10.01")
set(MWFL_WTL_COMMIT "011be908a1122e7bc9fd1106ecc48f22f5f86f00")

set(MWFL_WIL_REPOSITORY "https://github.com/microsoft/wil.git")
set(MWFL_WIL_TAG "v1.0.260126.7")
set(MWFL_WIL_COMMIT "cbf677fb0a942557d08fd129f4c106a76247b2ec")

function(mwfl_add_wtl_target source_dir)
    if(NOT EXISTS "${source_dir}/Include/atlapp.h")
        message(FATAL_ERROR "MWFL_WTL_SOURCE_DIR does not contain Include/atlapp.h: ${source_dir}")
    endif()
    add_library(mwfl_wtl_headers INTERFACE)
    add_library(WTL::WTL ALIAS mwfl_wtl_headers)
    target_include_directories(mwfl_wtl_headers SYSTEM INTERFACE "${source_dir}/Include")
endfunction()

function(mwfl_add_wil_target source_dir)
    if(NOT EXISTS "${source_dir}/include/wil/resource.h")
        message(FATAL_ERROR "MWFL_WIL_SOURCE_DIR does not contain include/wil/resource.h: ${source_dir}")
    endif()
    add_library(mwfl_wil_headers INTERFACE)
    add_library(WIL::WIL ALIAS mwfl_wil_headers)
    target_include_directories(mwfl_wil_headers SYSTEM INTERFACE "${source_dir}/include")
endfunction()

function(mwfl_resolve_dependencies)
    string(TOUPPER "${MWFL_DEPENDENCY_MODE}" dependency_mode)
    if(NOT dependency_mode MATCHES "^(AUTO|FETCH|SYSTEM)$")
        message(FATAL_ERROR
            "MWFL_DEPENDENCY_MODE must be AUTO, FETCH, or SYSTEM")
    endif()

    if(NOT TARGET WTL::WTL)
        if(MWFL_WTL_SOURCE_DIR)
            mwfl_add_wtl_target("${MWFL_WTL_SOURCE_DIR}")
        elseif(dependency_mode STREQUAL "SYSTEM")
            find_path(mwfl_wtl_system_dir Include/atlapp.h REQUIRED)
            mwfl_add_wtl_target("${mwfl_wtl_system_dir}")
        else()
            message(STATUS "Fetching official WTL ${MWFL_WTL_TAG} at ${MWFL_WTL_COMMIT}")
            FetchContent_Declare(mwfl_wtl
                GIT_REPOSITORY "${MWFL_WTL_REPOSITORY}"
                GIT_TAG "${MWFL_WTL_COMMIT}"
                GIT_SHALLOW FALSE
                GIT_PROGRESS TRUE
                SOURCE_SUBDIR _mwfl_headers_only)
            FetchContent_MakeAvailable(mwfl_wtl)
            mwfl_add_wtl_target("${mwfl_wtl_SOURCE_DIR}")
        endif()
    endif()

    if(NOT TARGET WIL::WIL)
        if(MWFL_WIL_SOURCE_DIR)
            mwfl_add_wil_target("${MWFL_WIL_SOURCE_DIR}")
        elseif(dependency_mode STREQUAL "SYSTEM")
            find_path(mwfl_wil_include_dir wil/resource.h REQUIRED)
            get_filename_component(mwfl_wil_system_dir
                "${mwfl_wil_include_dir}" DIRECTORY)
            mwfl_add_wil_target("${mwfl_wil_system_dir}")
        else()
            message(STATUS "Fetching Microsoft WIL ${MWFL_WIL_TAG} at ${MWFL_WIL_COMMIT}")
            FetchContent_Declare(mwfl_wil
                GIT_REPOSITORY "${MWFL_WIL_REPOSITORY}"
                GIT_TAG "${MWFL_WIL_COMMIT}"
                GIT_SHALLOW FALSE
                GIT_PROGRESS TRUE
                SOURCE_SUBDIR _mwfl_headers_only)
            FetchContent_MakeAvailable(mwfl_wil)
            mwfl_add_wil_target("${mwfl_wil_SOURCE_DIR}")
        endif()
    endif()

    get_target_property(resolved_wtl_include WTL::WTL INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(resolved_wil_include WIL::WIL INTERFACE_INCLUDE_DIRECTORIES)
    list(GET resolved_wtl_include 0 resolved_wtl_include)
    list(GET resolved_wil_include 0 resolved_wil_include)
    get_filename_component(resolved_wtl_source "${resolved_wtl_include}" DIRECTORY)
    get_filename_component(resolved_wil_source "${resolved_wil_include}" DIRECTORY)
    set(MWFL_RESOLVED_WTL_SOURCE_DIR "${resolved_wtl_source}" PARENT_SCOPE)
    set(MWFL_RESOLVED_WIL_SOURCE_DIR "${resolved_wil_source}" PARENT_SCOPE)
endfunction()
