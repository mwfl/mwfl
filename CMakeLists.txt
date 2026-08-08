cmake_minimum_required(VERSION 3.21)

project(mwtl VERSION 0.6.0 DESCRIPTION "Modern Windows Thin Layer" LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(NOT WIN32)
    message(FATAL_ERROR "mwtl supports Windows only.")
endif()

if(NOT MSVC)
    message(FATAL_ERROR "mwtl requires the MSVC-compatible ABI (MSVC or clang-cl).")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "mwtl supports 64-bit targets only; 32-bit targets are not supported.")
endif()

if(CMAKE_GENERATOR_PLATFORM AND
   NOT CMAKE_GENERATOR_PLATFORM MATCHES "^(x64|ARM64)$")
    message(FATAL_ERROR "mwtl supports x64 and ARM64 generator platforms.")
endif()

if(CMAKE_CXX_COMPILER_ARCHITECTURE_ID AND
   NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "^(x64|ARM64)$")
    message(FATAL_ERROR
        "mwtl supports x64 and ARM64; compiler architecture is ${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}.")
endif()

if(PROJECT_IS_TOP_LEVEL)
    option(MWTL_BUILD_EXAMPLES "Build mwtl examples" ON)
    option(MWTL_BUILD_TESTS "Build mwtl tests" ON)
    option(MWTL_WARNINGS_AS_ERRORS "Treat mwtl warnings as errors" ON)
    option(MWTL_ENABLE_ASAN "Enable AddressSanitizer for repository validation" OFF)
else()
    option(MWTL_BUILD_EXAMPLES "Build mwtl examples" OFF)
    option(MWTL_BUILD_TESTS "Build mwtl tests" OFF)
    option(MWTL_WARNINGS_AS_ERRORS "Treat mwtl warnings as errors" OFF)
    option(MWTL_ENABLE_ASAN "Enable AddressSanitizer for repository validation" OFF)
endif()

if(MWTL_ENABLE_ASAN)
    add_compile_options(/fsanitize=address)
    get_filename_component(mwtl_compiler_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_file(MWTL_ASAN_RUNTIME
        NAMES clang_rt.asan_dynamic-x86_64.dll
        HINTS "${mwtl_compiler_directory}"
        NO_DEFAULT_PATH)
    if(NOT MWTL_ASAN_RUNTIME)
        message(FATAL_ERROR "MSVC AddressSanitizer runtime DLL was not found")
    endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Dependencies)
mwtl_resolve_dependencies()

add_library(mwtl STATIC
    src/appearance.cpp
    src/application.cpp
    src/command.cpp
    src/command_controls.cpp
    src/control_resources.cpp
    src/controls.cpp
    src/input_controls.cpp
    src/layout.cpp
    src/navigation_controls.cpp
    src/dpi.cpp
    src/error.cpp
    src/desktop.cpp
    src/detail/diagnostics.cpp
    src/message_pump.cpp
    src/module.cpp
    src/timer.cpp
    src/window_options.cpp)
add_library(mwtl::mwtl ALIAS mwtl)

get_target_property(mwtl_wtl_include_dirs WTL::WTL INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(mwtl_wil_include_dirs WIL::WIL INTERFACE_INCLUDE_DIRECTORIES)

target_compile_features(mwtl PUBLIC cxx_std_20)
target_compile_definitions(mwtl
    PUBLIC
        UNICODE
        _UNICODE
        WINVER=0x0A00
        _WIN32_WINNT=0x0A00)
target_compile_options(mwtl
    PRIVATE
        /W4
        /permissive-
        /EHsc
        /utf-8)
if(MWTL_WARNINGS_AS_ERRORS)
    target_compile_options(mwtl PRIVATE /WX)
endif()
target_include_directories(mwtl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(mwtl SYSTEM PUBLIC
    $<BUILD_INTERFACE:${mwtl_wtl_include_dirs}>
    $<BUILD_INTERFACE:${mwtl_wil_include_dirs}>)
target_link_libraries(mwtl
    PRIVATE
        comctl32
        comdlg32
        advapi32
        dwmapi
        shell32
        ole32)
target_link_libraries(mwtl PRIVATE oleacc uuid)

install(TARGETS mwtl
    EXPORT mwtlTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES
    LICENSE
    THIRD_PARTY_NOTICES.md
    README.md
    SECURITY.md
    docs/api.md
    docs/design.md
    docs/stability.md
    docs/accessibility.md
    docs/reference.md
    docs/system-message-recipes.md
    docs/agent-usage.md
    docs/agent-reference.md
    docs/common-mistakes.md
    docs/terminology.md
    docs/capabilities.json
    docs/examples.json
    docs/llms.txt
    docs/llms-full.txt
    docs/scope-map.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/mwtl)
install(FILES cmake/Dependencies.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mwtl)

configure_package_config_file(
    cmake/mwtlConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/mwtlConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mwtl)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/mwtlConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/mwtlConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/mwtlConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mwtl)
install(EXPORT mwtlTargets
    FILE mwtlTargets.cmake
    NAMESPACE mwtl::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mwtl)

set(CPACK_PACKAGE_NAME "mwtl")
set(CPACK_PACKAGE_VENDOR "mwtl contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Modern Windows Thin Layer")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "mwtl-${PROJECT_VERSION}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_GENERATOR "ZIP")
set(CPACK_SOURCE_GENERATOR "ZIP")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "mwtl-${PROJECT_VERSION}-source")
set(CPACK_SOURCE_IGNORE_FILES "/[.]git/;/build/;/[.]tmp/;~$")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
include(CPack)

if(MWTL_BUILD_TESTS)
    target_compile_definitions(mwtl PRIVATE MWTL_TESTING)
endif()

if(MWTL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(MWTL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(agent-evals/fixtures)
    add_subdirectory(tests)
    if(MWTL_ENABLE_ASAN)
        add_custom_target(mwtl_asan_runtime ALL
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${PROJECT_BINARY_DIR}/tests/$<CONFIG>"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${MWTL_ASAN_RUNTIME}"
                "${PROJECT_BINARY_DIR}/tests/$<CONFIG>/clang_rt.asan_dynamic-x86_64.dll"
            DEPENDS mwtl)
    endif()
endif()
