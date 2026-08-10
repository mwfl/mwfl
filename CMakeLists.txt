cmake_minimum_required(VERSION 3.21)

project(mwtl VERSION 0.1.0 DESCRIPTION "Modern Windows Thin Layer" LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(FetchContent)

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
    option(MWTL_BUILD_SCINTILLA "Build the optional pinned Scintilla component" OFF)
    option(MWTL_BUILD_WEBVIEW2 "Build the optional pinned WebView2 component" OFF)
else()
    option(MWTL_BUILD_EXAMPLES "Build mwtl examples" OFF)
    option(MWTL_BUILD_TESTS "Build mwtl tests" OFF)
    option(MWTL_WARNINGS_AS_ERRORS "Treat mwtl warnings as errors" OFF)
    option(MWTL_ENABLE_ASAN "Enable AddressSanitizer for repository validation" OFF)
    option(MWTL_BUILD_SCINTILLA "Build the optional pinned Scintilla component" OFF)
    option(MWTL_BUILD_WEBVIEW2 "Build the optional pinned WebView2 component" OFF)
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
    src/dialog.cpp
    src/docking_auto_hide.cpp
    src/docking_drag.cpp
    src/docking_floating.cpp
    src/docking_keyboard.cpp
    src/docking_monitor.cpp
    src/docking_native.cpp
    src/docking_preview.cpp
    src/docking_session.cpp
    src/docking_workspace.cpp
    src/document.cpp
    src/document_coordination.cpp
    src/document_session.cpp
    src/document_tabs.cpp
    src/document_workspace.cpp
    src/detail/diagnostics.cpp
    src/message_pump.cpp
    src/native_host.cpp
    src/recent_files.cpp
    src/property_sheet.cpp
    src/single_instance.cpp
    src/splitter.cpp
    src/tab_workspace.cpp
    src/tray_icon.cpp
    src/module.cpp
    src/timer.cpp
    src/text_file.cpp
    src/text_history.cpp
    src/text_search.cpp
    src/window_options.cpp)
add_library(mwtl::mwtl ALIAS mwtl)

add_library(mwtl_d2d STATIC src/d2d_host.cpp)
add_library(mwtl::d2d ALIAS mwtl_d2d)
set_target_properties(mwtl_d2d PROPERTIES EXPORT_NAME d2d)
target_compile_features(mwtl_d2d PUBLIC cxx_std_20)
target_compile_options(mwtl_d2d PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWTL_WARNINGS_AS_ERRORS)
    target_compile_options(mwtl_d2d PRIVATE /WX)
endif()
target_include_directories(mwtl_d2d PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwtl_d2d PUBLIC mwtl PRIVATE d2d1)

add_library(mwtl_d3d STATIC src/d3d_host.cpp)
add_library(mwtl::d3d ALIAS mwtl_d3d)
set_target_properties(mwtl_d3d PROPERTIES EXPORT_NAME d3d)
target_compile_features(mwtl_d3d PUBLIC cxx_std_20)
target_compile_options(mwtl_d3d PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWTL_WARNINGS_AS_ERRORS)
    target_compile_options(mwtl_d3d PRIVATE /WX)
endif()
target_include_directories(mwtl_d3d PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwtl_d3d PUBLIC mwtl PRIVATE d3d11 dxgi)

add_library(mwtl_imaging STATIC src/imaging.cpp)
add_library(mwtl::imaging ALIAS mwtl_imaging)
set_target_properties(mwtl_imaging PROPERTIES EXPORT_NAME imaging)
target_compile_features(mwtl_imaging PUBLIC cxx_std_20)
target_compile_options(mwtl_imaging PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWTL_WARNINGS_AS_ERRORS)
    target_compile_options(mwtl_imaging PRIVATE /WX)
endif()
target_include_directories(mwtl_imaging PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwtl_imaging PUBLIC mwtl PRIVATE windowscodecs)

add_library(mwtl_printing STATIC
    src/printing.cpp
    src/printing_native.cpp
    src/printing_settings.cpp)
add_library(mwtl::printing ALIAS mwtl_printing)
set_target_properties(mwtl_printing PROPERTIES EXPORT_NAME printing)
target_compile_features(mwtl_printing PUBLIC cxx_std_20)
target_compile_options(mwtl_printing PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWTL_WARNINGS_AS_ERRORS)
    target_compile_options(mwtl_printing PRIVATE /WX)
endif()
target_include_directories(mwtl_printing PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwtl_printing PUBLIC mwtl PRIVATE winspool gdi32 comdlg32)

add_library(mwtl_ole STATIC src/ole_data.cpp src/ole_drag_drop.cpp)
add_library(mwtl::ole ALIAS mwtl_ole)
set_target_properties(mwtl_ole PROPERTIES EXPORT_NAME ole)
target_compile_features(mwtl_ole PUBLIC cxx_std_20)
target_compile_options(mwtl_ole PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWTL_WARNINGS_AS_ERRORS)
    target_compile_options(mwtl_ole PRIVATE /WX)
endif()
target_include_directories(mwtl_ole PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwtl_ole PUBLIC mwtl PRIVATE ole32 shell32)

add_library(mwtl_shell STATIC
    src/settings_store.cpp
    src/file_association.cpp
    src/help.cpp
    src/shell_integration.cpp)
add_library(mwtl::shell ALIAS mwtl_shell)
set_target_properties(mwtl_shell PROPERTIES EXPORT_NAME shell)
target_compile_features(mwtl_shell PUBLIC cxx_std_20)
target_compile_options(mwtl_shell PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWTL_WARNINGS_AS_ERRORS)
    target_compile_options(mwtl_shell PRIVATE /WX)
endif()
target_include_directories(mwtl_shell PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwtl_shell PUBLIC mwtl PRIVATE advapi32 shell32 ole32 propsys htmlhelp)

add_library(mwtl_ribbon STATIC src/ribbon.cpp)
add_library(mwtl::ribbon ALIAS mwtl_ribbon)
set_target_properties(mwtl_ribbon PROPERTIES EXPORT_NAME ribbon)
target_compile_features(mwtl_ribbon PUBLIC cxx_std_20)
target_compile_options(mwtl_ribbon PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWTL_WARNINGS_AS_ERRORS)
    target_compile_options(mwtl_ribbon PRIVATE /WX)
endif()
target_include_directories(mwtl_ribbon PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwtl_ribbon PUBLIC mwtl PRIVATE ole32 propsys)

add_library(mwtl_mdi STATIC src/mdi.cpp)
add_library(mwtl::mdi ALIAS mwtl_mdi)
set_target_properties(mwtl_mdi PROPERTIES EXPORT_NAME mdi)
target_compile_features(mwtl_mdi PUBLIC cxx_std_20)
target_compile_options(mwtl_mdi PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWTL_WARNINGS_AS_ERRORS)
    target_compile_options(mwtl_mdi PRIVATE /WX)
endif()
target_include_directories(mwtl_mdi PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwtl_mdi PUBLIC mwtl)

add_library(mwtl_graphics STATIC src/graphics.cpp)
add_library(mwtl::graphics ALIAS mwtl_graphics)
set_target_properties(mwtl_graphics PROPERTIES EXPORT_NAME graphics)
target_compile_features(mwtl_graphics PUBLIC cxx_std_20)
target_compile_options(mwtl_graphics PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWTL_WARNINGS_AS_ERRORS)
    target_compile_options(mwtl_graphics PRIVATE /WX)
endif()
target_include_directories(mwtl_graphics PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwtl_graphics PUBLIC mwtl PRIVATE gdiplus gdi32)

if(MWTL_BUILD_SCINTILLA)
    FetchContent_Declare(mwtl_scintilla_source
        URL https://www.scintilla.org/scintilla565.zip
        URL_HASH SHA256=345140A60BF4CEEA3340942E8205E6A8FBDA8DB13EB48F827126B9BE15BD3DA1
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_Declare(mwtl_scintilla_binary
        URL https://www.scintilla.org/wscite565.zip
        URL_HASH SHA256=9B5A7AF4BEB2D61BA6A5F62FA678FF68C96D175D19664FC1A801F444B357303B
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(mwtl_scintilla_source mwtl_scintilla_binary)
    set(MWTL_SCINTILLA_RUNTIME
        "${mwtl_scintilla_binary_SOURCE_DIR}/Scintilla.dll")
    if(NOT EXISTS "${MWTL_SCINTILLA_RUNTIME}" OR
       NOT EXISTS "${mwtl_scintilla_source_SOURCE_DIR}/include/Scintilla.h")
        message(FATAL_ERROR "Pinned Scintilla 5.6.5 package layout is invalid")
    endif()

    add_library(mwtl_scintilla STATIC src/scintilla.cpp)
    add_library(mwtl::scintilla ALIAS mwtl_scintilla)
    set_target_properties(mwtl_scintilla PROPERTIES EXPORT_NAME scintilla)
    target_compile_features(mwtl_scintilla PUBLIC cxx_std_20)
    target_compile_options(mwtl_scintilla PRIVATE /W4 /permissive- /EHsc /utf-8)
    if(MWTL_WARNINGS_AS_ERRORS)
        target_compile_options(mwtl_scintilla PRIVATE /WX)
    endif()
    target_include_directories(mwtl_scintilla
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE "${mwtl_scintilla_source_SOURCE_DIR}/include")
    target_link_libraries(mwtl_scintilla PUBLIC mwtl)

    function(mwtl_deploy_scintilla target)
        if(NOT TARGET "${target}")
            message(FATAL_ERROR "mwtl_deploy_scintilla target does not exist: ${target}")
        endif()
        add_custom_command(TARGET "${target}" POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${MWTL_SCINTILLA_RUNTIME}" "$<TARGET_FILE_DIR:${target}>/Scintilla.dll")
    endfunction()
endif()

if(MWTL_BUILD_WEBVIEW2)
    FetchContent_Declare(mwtl_webview2_sdk
        URL https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/1.0.4129.50/microsoft.web.webview2.1.0.4129.50.nupkg
        URL_HASH SHA256=D3934F482D484B89FB4825DF720C710664E1143A1E90F7B3A60794EF33F473D2
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(mwtl_webview2_sdk)
    set(MWTL_WEBVIEW2_INCLUDE_DIR
        "${mwtl_webview2_sdk_SOURCE_DIR}/build/native/include")
    set(MWTL_WEBVIEW2_LOADER
        "${mwtl_webview2_sdk_SOURCE_DIR}/build/native/x64/WebView2LoaderStatic.lib")
    if(NOT EXISTS "${MWTL_WEBVIEW2_INCLUDE_DIR}/WebView2.h" OR
       NOT EXISTS "${MWTL_WEBVIEW2_LOADER}")
        message(FATAL_ERROR "Pinned WebView2 1.0.4129.50 x64 SDK layout is invalid")
    endif()

    add_library(mwtl_webview2 STATIC src/webview2.cpp)
    add_library(mwtl::webview2 ALIAS mwtl_webview2)
    set_target_properties(mwtl_webview2 PROPERTIES EXPORT_NAME webview2)
    target_compile_features(mwtl_webview2 PUBLIC cxx_std_20)
    target_compile_options(mwtl_webview2 PRIVATE /W4 /permissive- /EHsc /utf-8)
    if(MWTL_WARNINGS_AS_ERRORS)
        target_compile_options(mwtl_webview2 PRIVATE /WX)
    endif()
    target_include_directories(mwtl_webview2
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE "${MWTL_WEBVIEW2_INCLUDE_DIR}")
    target_link_libraries(mwtl_webview2 PUBLIC mwtl PRIVATE
        "$<BUILD_INTERFACE:${MWTL_WEBVIEW2_LOADER}>" runtimeobject)
endif()

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

install(TARGETS mwtl mwtl_d2d mwtl_d3d mwtl_imaging mwtl_printing mwtl_ole mwtl_shell mwtl_ribbon mwtl_mdi mwtl_graphics
    EXPORT mwtlTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(MWTL_BUILD_SCINTILLA)
    install(TARGETS mwtl_scintilla EXPORT mwtlTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(FILES "${MWTL_SCINTILLA_RUNTIME}" DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES "${mwtl_scintilla_source_SOURCE_DIR}/License.txt"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mwtl/scintilla-5.6.5)
endif()
if(MWTL_BUILD_WEBVIEW2)
    install(TARGETS mwtl_webview2 EXPORT mwtlTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(FILES "${mwtl_webview2_sdk_SOURCE_DIR}/LICENSE.txt"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mwtl/webview2-1.0.4129.50)
    install(FILES "${MWTL_WEBVIEW2_LOADER}" DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
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
    docs/parity.json
    docs/win32xx-parity-roadmap.md
    docs/api-index.json
    docs/announcement-v0.1.md
    docs/tutorials/notepad.md
    docs/tutorials/image-viewer.md
    docs/tutorials/browser.md
    docs/tutorials/code-editor.md
    docs/tutorials/ribbon.md
    docs/tutorials/mdi.md
    docs/tutorials/graphics-help.md
    docs/reflections/0.4.md
    docs/recipes/notepad-command-setting.md
    docs/recipes/split-pane.md
    docs/recipes/direct2d-drawing.md
    docs/recipes/ribbon.md
    docs/recipes/mdi.md
    docs/recipes/graphics-help.md
    docs/recipes/direct3d-swap-chain.md
    docs/recipes/native-host.md
    docs/recipes/webview2-navigation.md
    docs/recipes/scintilla-notification.md
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
