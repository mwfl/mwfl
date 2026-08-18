cmake_minimum_required(VERSION 3.21)

project(mwfl VERSION 0.1.9 DESCRIPTION "Modern Windows Foundation Layer" LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(FetchContent)

if(NOT WIN32)
    message(FATAL_ERROR "mwfl supports Windows only.")
endif()

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message(FATAL_ERROR "mwfl requires the Microsoft Visual C++ compiler.")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "mwfl supports 64-bit targets only; 32-bit targets are not supported.")
endif()

if(CMAKE_GENERATOR_PLATFORM AND
   NOT CMAKE_GENERATOR_PLATFORM MATCHES "^(x64|ARM64)$")
    message(FATAL_ERROR "mwfl supports x64 and ARM64 generator platforms.")
endif()

if(CMAKE_CXX_COMPILER_ARCHITECTURE_ID AND
   NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "^(x64|ARM64)$")
    message(FATAL_ERROR
        "mwfl supports x64 and ARM64; compiler architecture is ${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}.")
endif()

if(PROJECT_IS_TOP_LEVEL)
    option(MWFL_BUILD_EXAMPLES "Build mwfl examples" ON)
    option(MWFL_BUILD_TESTS "Build mwfl tests" ON)
    option(MWFL_WARNINGS_AS_ERRORS "Treat mwfl warnings as errors" ON)
    option(MWFL_ENABLE_ASAN "Enable AddressSanitizer for repository validation" OFF)
    option(MWFL_BUILD_SCINTILLA "Build the optional pinned Scintilla component" OFF)
    option(MWFL_BUILD_WEBVIEW2 "Build the optional pinned WebView2 component" OFF)
else()
    option(MWFL_BUILD_EXAMPLES "Build mwfl examples" OFF)
    option(MWFL_BUILD_TESTS "Build mwfl tests" OFF)
    option(MWFL_WARNINGS_AS_ERRORS "Treat mwfl warnings as errors" OFF)
    option(MWFL_ENABLE_ASAN "Enable AddressSanitizer for repository validation" OFF)
    option(MWFL_BUILD_SCINTILLA "Build the optional pinned Scintilla component" OFF)
    option(MWFL_BUILD_WEBVIEW2 "Build the optional pinned WebView2 component" OFF)
endif()

if(MWFL_ENABLE_ASAN)
    add_compile_options(/fsanitize=address)
    get_filename_component(mwfl_compiler_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_file(MWFL_ASAN_RUNTIME
        NAMES clang_rt.asan_dynamic-x86_64.dll
        HINTS "${mwfl_compiler_directory}"
        NO_DEFAULT_PATH)
    if(NOT MWFL_ASAN_RUNTIME)
        message(FATAL_ERROR "MSVC AddressSanitizer runtime DLL was not found")
    endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Dependencies)
mwfl_resolve_dependencies()

add_library(mwfl_core STATIC src/core.cpp)
set_target_properties(mwfl_core PROPERTIES EXPORT_NAME core)
add_library(mwfl::core ALIAS mwfl_core)
target_compile_features(mwfl_core PUBLIC cxx_std_20)
target_compile_definitions(mwfl_core PUBLIC UNICODE _UNICODE WINVER=0x0A00 _WIN32_WINNT=0x0A00)
target_compile_options(mwfl_core PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWFL_WARNINGS_AS_ERRORS)
    target_compile_options(mwfl_core PRIVATE /WX)
endif()
target_include_directories(mwfl_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

add_library(mwfl_service STATIC src/service.cpp)
set_target_properties(mwfl_service PROPERTIES EXPORT_NAME service)
add_library(mwfl::service ALIAS mwfl_service)
target_compile_features(mwfl_service PUBLIC cxx_std_20)
target_compile_options(mwfl_service PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWFL_WARNINGS_AS_ERRORS)
    target_compile_options(mwfl_service PRIVATE /WX)
endif()
target_include_directories(mwfl_service PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwfl_service PUBLIC mwfl_core PRIVATE advapi32)

add_library(mwfl_process STATIC src/process.cpp)
set_target_properties(mwfl_process PROPERTIES EXPORT_NAME process)
add_library(mwfl::process ALIAS mwfl_process)
target_compile_features(mwfl_process PUBLIC cxx_std_20)
target_compile_options(mwfl_process PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWFL_WARNINGS_AS_ERRORS)
    target_compile_options(mwfl_process PRIVATE /WX)
endif()
target_include_directories(mwfl_process PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwfl_process PUBLIC mwfl_core)

add_library(mwfl_ipc STATIC src/ipc.cpp)
set_target_properties(mwfl_ipc PROPERTIES EXPORT_NAME ipc)
add_library(mwfl::ipc ALIAS mwfl_ipc)
target_compile_features(mwfl_ipc PUBLIC cxx_std_20)
target_compile_options(mwfl_ipc PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWFL_WARNINGS_AS_ERRORS)
    target_compile_options(mwfl_ipc PRIVATE /WX)
endif()
target_include_directories(mwfl_ipc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwfl_ipc PUBLIC mwfl_core PRIVATE advapi32)

add_library(mwfl_diagnostics STATIC src/diagnostics.cpp)
set_target_properties(mwfl_diagnostics PROPERTIES EXPORT_NAME diagnostics)
add_library(mwfl::diagnostics ALIAS mwfl_diagnostics)
target_compile_features(mwfl_diagnostics PUBLIC cxx_std_20)
target_compile_options(mwfl_diagnostics PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWFL_WARNINGS_AS_ERRORS)
    target_compile_options(mwfl_diagnostics PRIVATE /WX)
endif()
target_include_directories(mwfl_diagnostics PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwfl_diagnostics PUBLIC mwfl_core PRIVATE advapi32 dbghelp)

add_library(mwfl_security STATIC src/security.cpp)
set_target_properties(mwfl_security PROPERTIES EXPORT_NAME security)
add_library(mwfl::security ALIAS mwfl_security)
target_compile_features(mwfl_security PUBLIC cxx_std_20)
target_compile_options(mwfl_security PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWFL_WARNINGS_AS_ERRORS)
    target_compile_options(mwfl_security PRIVATE /WX)
endif()
target_include_directories(mwfl_security PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwfl_security PUBLIC mwfl_core PRIVATE advapi32 crypt32)

add_library(mwfl_deployment STATIC src/deployment.cpp)
set_target_properties(mwfl_deployment PROPERTIES EXPORT_NAME deployment)
add_library(mwfl::deployment ALIAS mwfl_deployment)
target_compile_features(mwfl_deployment PUBLIC cxx_std_20)
target_compile_options(mwfl_deployment PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWFL_WARNINGS_AS_ERRORS)
    target_compile_options(mwfl_deployment PRIVATE /WX)
endif()
target_include_directories(mwfl_deployment PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwfl_deployment PUBLIC mwfl_core PRIVATE crypt32 version wintrust)

add_library(mwfl_scheduler STATIC src/scheduler.cpp)
set_target_properties(mwfl_scheduler PROPERTIES EXPORT_NAME scheduler)
add_library(mwfl::scheduler ALIAS mwfl_scheduler)
target_compile_features(mwfl_scheduler PUBLIC cxx_std_20)
target_compile_options(mwfl_scheduler PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWFL_WARNINGS_AS_ERRORS)
    target_compile_options(mwfl_scheduler PRIVATE /WX)
endif()
target_include_directories(mwfl_scheduler PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwfl_scheduler PUBLIC mwfl_core PRIVATE taskschd ole32 oleaut32)

add_library(mwfl STATIC
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
    src/detail/window_appearance.cpp
    src/message_pump.cpp
    src/native_host.cpp
    src/recent_files.cpp
    src/property_sheet.cpp
    src/single_instance.cpp
    src/splitter.cpp
    src/tab_workspace.cpp
    src/tray_icon.cpp
    src/timer.cpp
    src/text_file.cpp
    src/text_history.cpp
    src/text_search.cpp
    src/window_options.cpp)
# `mwfl::ui` is the public target for the native Windows UI layer.
set_target_properties(mwfl PROPERTIES EXPORT_NAME ui)
add_library(mwfl::ui ALIAS mwfl)
target_link_libraries(mwfl PUBLIC mwfl_core)

add_library(mwfl_d2d STATIC src/d2d_host.cpp)
add_library(mwfl::d2d ALIAS mwfl_d2d)
set_target_properties(mwfl_d2d PROPERTIES EXPORT_NAME d2d)
target_compile_features(mwfl_d2d PUBLIC cxx_std_20)
target_compile_options(mwfl_d2d PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWFL_WARNINGS_AS_ERRORS)
    target_compile_options(mwfl_d2d PRIVATE /WX)
endif()
target_include_directories(mwfl_d2d PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwfl_d2d PUBLIC mwfl PRIVATE d2d1)

add_library(mwfl_d3d STATIC src/d3d_host.cpp)
add_library(mwfl::d3d ALIAS mwfl_d3d)
set_target_properties(mwfl_d3d PROPERTIES EXPORT_NAME d3d)
target_compile_features(mwfl_d3d PUBLIC cxx_std_20)
target_compile_options(mwfl_d3d PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWFL_WARNINGS_AS_ERRORS)
    target_compile_options(mwfl_d3d PRIVATE /WX)
endif()
target_include_directories(mwfl_d3d PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwfl_d3d PUBLIC mwfl PRIVATE d3d11 dxgi)

add_library(mwfl_imaging STATIC src/imaging.cpp)
add_library(mwfl::imaging ALIAS mwfl_imaging)
set_target_properties(mwfl_imaging PROPERTIES EXPORT_NAME imaging)
target_compile_features(mwfl_imaging PUBLIC cxx_std_20)
target_compile_options(mwfl_imaging PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWFL_WARNINGS_AS_ERRORS)
    target_compile_options(mwfl_imaging PRIVATE /WX)
endif()
target_include_directories(mwfl_imaging PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwfl_imaging PUBLIC mwfl PRIVATE windowscodecs)

add_library(mwfl_printing STATIC
    src/printing.cpp
    src/printing_native.cpp
    src/printing_settings.cpp)
add_library(mwfl::printing ALIAS mwfl_printing)
set_target_properties(mwfl_printing PROPERTIES EXPORT_NAME printing)
target_compile_features(mwfl_printing PUBLIC cxx_std_20)
target_compile_options(mwfl_printing PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWFL_WARNINGS_AS_ERRORS)
    target_compile_options(mwfl_printing PRIVATE /WX)
endif()
target_include_directories(mwfl_printing PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwfl_printing PUBLIC mwfl PRIVATE winspool gdi32 comdlg32)

add_library(mwfl_ole STATIC src/ole_data.cpp src/ole_drag_drop.cpp)
add_library(mwfl::ole ALIAS mwfl_ole)
set_target_properties(mwfl_ole PROPERTIES EXPORT_NAME ole)
target_compile_features(mwfl_ole PUBLIC cxx_std_20)
target_compile_options(mwfl_ole PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWFL_WARNINGS_AS_ERRORS)
    target_compile_options(mwfl_ole PRIVATE /WX)
endif()
target_include_directories(mwfl_ole PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwfl_ole PUBLIC mwfl PRIVATE ole32 shell32)

add_library(mwfl_shell STATIC
    src/settings_store.cpp
    src/file_association.cpp
    src/help.cpp
    src/shell_integration.cpp)
add_library(mwfl::shell ALIAS mwfl_shell)
set_target_properties(mwfl_shell PROPERTIES EXPORT_NAME shell)
target_compile_features(mwfl_shell PUBLIC cxx_std_20)
target_compile_options(mwfl_shell PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWFL_WARNINGS_AS_ERRORS)
    target_compile_options(mwfl_shell PRIVATE /WX)
endif()
target_include_directories(mwfl_shell PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwfl_shell PUBLIC mwfl PRIVATE advapi32 shell32 ole32 propsys htmlhelp)

add_library(mwfl_ribbon STATIC src/ribbon.cpp)
add_library(mwfl::ribbon ALIAS mwfl_ribbon)
set_target_properties(mwfl_ribbon PROPERTIES EXPORT_NAME ribbon)
target_compile_features(mwfl_ribbon PUBLIC cxx_std_20)
target_compile_options(mwfl_ribbon PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWFL_WARNINGS_AS_ERRORS)
    target_compile_options(mwfl_ribbon PRIVATE /WX)
endif()
target_include_directories(mwfl_ribbon PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwfl_ribbon PUBLIC mwfl PRIVATE ole32 propsys)

add_library(mwfl_mdi STATIC src/mdi.cpp)
add_library(mwfl::mdi ALIAS mwfl_mdi)
set_target_properties(mwfl_mdi PROPERTIES EXPORT_NAME mdi)
target_compile_features(mwfl_mdi PUBLIC cxx_std_20)
target_compile_options(mwfl_mdi PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWFL_WARNINGS_AS_ERRORS)
    target_compile_options(mwfl_mdi PRIVATE /WX)
endif()
target_include_directories(mwfl_mdi PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwfl_mdi PUBLIC mwfl)

add_library(mwfl_graphics STATIC src/graphics.cpp)
add_library(mwfl::graphics ALIAS mwfl_graphics)
set_target_properties(mwfl_graphics PROPERTIES EXPORT_NAME graphics)
target_compile_features(mwfl_graphics PUBLIC cxx_std_20)
target_compile_options(mwfl_graphics PRIVATE /W4 /permissive- /EHsc /utf-8)
if(MWFL_WARNINGS_AS_ERRORS)
    target_compile_options(mwfl_graphics PRIVATE /WX)
endif()
target_include_directories(mwfl_graphics PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(mwfl_graphics PUBLIC mwfl PRIVATE gdiplus gdi32)

if(MWFL_BUILD_SCINTILLA)
    FetchContent_Declare(mwfl_scintilla_source
        URL
            https://github.com/mwfl/vendor-assets/releases/download/scintilla-5.6.5/scintilla565.zip
            https://www.scintilla.org/scintilla565.zip
        URL_HASH SHA256=345140A60BF4CEEA3340942E8205E6A8FBDA8DB13EB48F827126B9BE15BD3DA1
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_Declare(mwfl_scintilla_binary
        URL
            https://github.com/mwfl/vendor-assets/releases/download/scintilla-5.6.5/wscite565.zip
            https://www.scintilla.org/wscite565.zip
        URL_HASH SHA256=9B5A7AF4BEB2D61BA6A5F62FA678FF68C96D175D19664FC1A801F444B357303B
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(mwfl_scintilla_source mwfl_scintilla_binary)
    set(MWFL_SCINTILLA_RUNTIME
        "${mwfl_scintilla_binary_SOURCE_DIR}/Scintilla.dll")
    set(MWFL_LEXILLA_RUNTIME
        "${mwfl_scintilla_binary_SOURCE_DIR}/Lexilla.dll")
    if(NOT EXISTS "${MWFL_SCINTILLA_RUNTIME}" OR
       NOT EXISTS "${MWFL_LEXILLA_RUNTIME}" OR
       NOT EXISTS "${mwfl_scintilla_source_SOURCE_DIR}/include/Scintilla.h")
        message(FATAL_ERROR "Pinned Scintilla/Lexilla 5.6.5 package layout is invalid")
    endif()

    add_library(mwfl_scintilla STATIC src/scintilla.cpp)
    add_library(mwfl::scintilla ALIAS mwfl_scintilla)
    set_target_properties(mwfl_scintilla PROPERTIES EXPORT_NAME scintilla)
    target_compile_features(mwfl_scintilla PUBLIC cxx_std_20)
    target_compile_options(mwfl_scintilla PRIVATE /W4 /permissive- /EHsc /utf-8)
    if(MWFL_WARNINGS_AS_ERRORS)
        target_compile_options(mwfl_scintilla PRIVATE /WX)
    endif()
    target_include_directories(mwfl_scintilla
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE "${mwfl_scintilla_source_SOURCE_DIR}/include")
    target_link_libraries(mwfl_scintilla PUBLIC mwfl)

    function(mwfl_deploy_scintilla target)
        if(NOT TARGET "${target}")
            message(FATAL_ERROR "mwfl_deploy_scintilla target does not exist: ${target}")
        endif()
        add_custom_command(TARGET "${target}" POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${MWFL_SCINTILLA_RUNTIME}" "$<TARGET_FILE_DIR:${target}>/Scintilla.dll")
    endfunction()

    function(mwfl_deploy_lexilla target)
        if(NOT TARGET "${target}")
            message(FATAL_ERROR "mwfl_deploy_lexilla target does not exist: ${target}")
        endif()
        add_custom_command(TARGET "${target}" POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${MWFL_LEXILLA_RUNTIME}" "$<TARGET_FILE_DIR:${target}>/Lexilla.dll")
    endfunction()
endif()

if(MWFL_BUILD_WEBVIEW2)
    FetchContent_Declare(mwfl_webview2_sdk
        URL https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/1.0.4129.50/microsoft.web.webview2.1.0.4129.50.nupkg
        URL_HASH SHA256=D3934F482D484B89FB4825DF720C710664E1143A1E90F7B3A60794EF33F473D2
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(mwfl_webview2_sdk)
    set(MWFL_WEBVIEW2_INCLUDE_DIR
        "${mwfl_webview2_sdk_SOURCE_DIR}/build/native/include")
    set(MWFL_WEBVIEW2_LOADER
        "${mwfl_webview2_sdk_SOURCE_DIR}/build/native/x64/WebView2LoaderStatic.lib")
    if(NOT EXISTS "${MWFL_WEBVIEW2_INCLUDE_DIR}/WebView2.h" OR
       NOT EXISTS "${MWFL_WEBVIEW2_LOADER}")
        message(FATAL_ERROR "Pinned WebView2 1.0.4129.50 x64 SDK layout is invalid")
    endif()

    add_library(mwfl_webview2 STATIC src/webview2.cpp)
    add_library(mwfl::webview2 ALIAS mwfl_webview2)
    set_target_properties(mwfl_webview2 PROPERTIES EXPORT_NAME webview2)
    target_compile_features(mwfl_webview2 PUBLIC cxx_std_20)
    target_compile_options(mwfl_webview2 PRIVATE /W4 /permissive- /EHsc /utf-8)
    if(MWFL_WARNINGS_AS_ERRORS)
        target_compile_options(mwfl_webview2 PRIVATE /WX)
    endif()
    target_include_directories(mwfl_webview2
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE "${MWFL_WEBVIEW2_INCLUDE_DIR}")
    target_link_libraries(mwfl_webview2 PUBLIC mwfl PRIVATE
        "$<BUILD_INTERFACE:${MWFL_WEBVIEW2_LOADER}>" runtimeobject)
endif()

get_target_property(mwfl_wil_include_dirs WIL::WIL INTERFACE_INCLUDE_DIRECTORIES)

target_compile_features(mwfl PUBLIC cxx_std_20)
target_compile_definitions(mwfl
    PUBLIC
        UNICODE
        _UNICODE
        WINVER=0x0A00
        _WIN32_WINNT=0x0A00)
target_compile_options(mwfl
    PRIVATE
        /W4
        /permissive-
        /EHsc
        /utf-8)
if(MWFL_WARNINGS_AS_ERRORS)
    target_compile_options(mwfl PRIVATE /WX)
endif()
target_include_directories(mwfl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(mwfl SYSTEM PUBLIC
    $<BUILD_INTERFACE:${mwfl_wil_include_dirs}>)
target_link_libraries(mwfl
    PRIVATE
        comctl32
        comdlg32
        advapi32
        dwmapi
        shell32
        ole32)
target_link_libraries(mwfl PRIVATE oleacc uuid uxtheme)

install(TARGETS mwfl_core mwfl_service mwfl_process mwfl_ipc mwfl_diagnostics mwfl_security mwfl_deployment mwfl_scheduler mwfl mwfl_d2d mwfl_d3d mwfl_imaging mwfl_printing mwfl_ole mwfl_shell mwfl_ribbon mwfl_mdi mwfl_graphics
    EXPORT mwflTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(MWFL_BUILD_SCINTILLA)
    install(TARGETS mwfl_scintilla EXPORT mwflTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(FILES "${MWFL_SCINTILLA_RUNTIME}" DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES "${mwfl_scintilla_source_SOURCE_DIR}/License.txt"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mwfl/scintilla-5.6.5)
endif()
if(MWFL_BUILD_WEBVIEW2)
    install(TARGETS mwfl_webview2 EXPORT mwflTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(FILES "${mwfl_webview2_sdk_SOURCE_DIR}/LICENSE.txt"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mwfl/webview2-1.0.4129.50)
    install(FILES "${MWFL_WEBVIEW2_LOADER}" DESTINATION ${CMAKE_INSTALL_LIBDIR})
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
    docs/public-api-contract-audit.md
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
    docs/foundation-releases.md
    docs/foundation-migration-next.md
    docs/release-readiness.md
    docs/windows-ui-modernization-plan.md
    docs/parity.json
    docs/win32xx-parity-roadmap.md
    docs/api-index.json
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
    DESTINATION ${CMAKE_INSTALL_DATADIR}/mwfl)
install(FILES cmake/Dependencies.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mwfl)

configure_package_config_file(
    cmake/mwflConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/mwflConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mwfl)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/mwflConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/mwflConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/mwflConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mwfl)
install(EXPORT mwflTargets
    FILE mwflTargets.cmake
    NAMESPACE mwfl::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mwfl)

set(CPACK_PACKAGE_NAME "mwfl")
set(CPACK_PACKAGE_VENDOR "mwfl contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Modern Windows Foundation Layer")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "mwfl-${PROJECT_VERSION}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_GENERATOR "ZIP")
set(CPACK_SOURCE_GENERATOR "ZIP")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "mwfl-${PROJECT_VERSION}-source")
set(CPACK_SOURCE_IGNORE_FILES "/[.]git/;/build/;/[.]tmp/;~$")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
include(CPack)

if(MWFL_BUILD_TESTS)
    target_compile_definitions(mwfl PRIVATE MWFL_TESTING)
endif()

if(MWFL_BUILD_TESTS)
    enable_testing()
endif()

if(MWFL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(MWFL_BUILD_TESTS)
    add_subdirectory(agent-evals/fixtures)
    add_subdirectory(tests)
    if(MWFL_ENABLE_ASAN)
        add_custom_target(mwfl_asan_runtime ALL
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${PROJECT_BINARY_DIR}/tests/$<CONFIG>"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${MWFL_ASAN_RUNTIME}"
                "${PROJECT_BINARY_DIR}/tests/$<CONFIG>/clang_rt.asan_dynamic-x86_64.dll"
            DEPENDS mwfl)
    endif()
endif()
