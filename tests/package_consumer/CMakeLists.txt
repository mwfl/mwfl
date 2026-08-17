cmake_minimum_required(VERSION 3.21)
project(mwfl_package_consumer LANGUAGES CXX)

set(mwfl_components core service process ipc diagnostics security deployment d2d d3d imaging printing ole shell ribbon mdi graphics)
if(MWFL_TEST_SCINTILLA)
    list(APPEND mwfl_components scintilla)
endif()
if(MWFL_TEST_WEBVIEW2)
    list(APPEND mwfl_components webview2)
endif()
find_package(mwfl 0.1 CONFIG REQUIRED COMPONENTS ${mwfl_components})

get_target_property(mwfl_core_links mwfl::ui INTERFACE_LINK_LIBRARIES)
if("${mwfl_core_links}" MATCHES "d2d|d3d|dxgi|windowscodecs|winspool|scintilla|webview|ribbon|uiribbon|gdiplus|htmlhelp")
    message(FATAL_ERROR "UI target mwfl::ui unexpectedly exposes optional rendering linkage")
endif()
if(MWFL_TEST_WEBVIEW2)
    add_executable(mwfl_webview2_package_consumer webview2_main.cpp)
    target_link_libraries(mwfl_webview2_package_consumer PRIVATE mwfl::webview2)
    target_compile_features(mwfl_webview2_package_consumer PRIVATE cxx_std_20)
endif()

add_executable(mwfl_package_consumer main.cpp)
target_link_libraries(mwfl_package_consumer PRIVATE mwfl::ui)
target_compile_features(mwfl_package_consumer PRIVATE cxx_std_20)

add_executable(mwfl_foundation_package_consumer foundation_main.cpp)
target_link_libraries(mwfl_foundation_package_consumer PRIVATE
    mwfl::core mwfl::service mwfl::process mwfl::ipc
    mwfl::diagnostics mwfl::security mwfl::deployment)
target_compile_features(mwfl_foundation_package_consumer PRIVATE cxx_std_20)

add_executable(mwfl_d2d_package_consumer d2d_main.cpp)
target_link_libraries(mwfl_d2d_package_consumer PRIVATE mwfl::d2d)
target_compile_features(mwfl_d2d_package_consumer PRIVATE cxx_std_20)

add_executable(mwfl_d3d_package_consumer d3d_main.cpp)
target_link_libraries(mwfl_d3d_package_consumer PRIVATE mwfl::d3d)
target_compile_features(mwfl_d3d_package_consumer PRIVATE cxx_std_20)

add_executable(mwfl_imaging_package_consumer imaging_main.cpp)
target_link_libraries(mwfl_imaging_package_consumer PRIVATE mwfl::imaging)
target_compile_features(mwfl_imaging_package_consumer PRIVATE cxx_std_20)

add_executable(mwfl_printing_package_consumer printing_main.cpp)
target_link_libraries(mwfl_printing_package_consumer PRIVATE mwfl::printing)
target_compile_features(mwfl_printing_package_consumer PRIVATE cxx_std_20)

add_executable(mwfl_ole_package_consumer ole_main.cpp)
target_link_libraries(mwfl_ole_package_consumer PRIVATE mwfl::ole)
target_compile_features(mwfl_ole_package_consumer PRIVATE cxx_std_20)

add_executable(mwfl_shell_package_consumer shell_main.cpp)
target_link_libraries(mwfl_shell_package_consumer PRIVATE mwfl::shell)
target_compile_features(mwfl_shell_package_consumer PRIVATE cxx_std_20)

foreach(component IN ITEMS ribbon mdi graphics)
    add_executable(mwfl_${component}_package_consumer ${component}_main.cpp)
    target_link_libraries(mwfl_${component}_package_consumer PRIVATE mwfl::${component})
    target_compile_features(mwfl_${component}_package_consumer PRIVATE cxx_std_20)
endforeach()

if(MWFL_TEST_SCINTILLA)
    add_executable(mwfl_scintilla_package_consumer scintilla_main.cpp)
    target_link_libraries(mwfl_scintilla_package_consumer PRIVATE mwfl::scintilla)
    target_compile_features(mwfl_scintilla_package_consumer PRIVATE cxx_std_20)
    mwfl_deploy_scintilla(mwfl_scintilla_package_consumer)
endif()
if(MWFL_ENABLE_ASAN)
    get_property(mwfl_consumer_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(consumer_target IN LISTS mwfl_consumer_targets)
        get_target_property(consumer_type ${consumer_target} TYPE)
        if(consumer_type STREQUAL "EXECUTABLE")
            target_compile_options(${consumer_target} PRIVATE /fsanitize=address)
        endif()
    endforeach()
    get_filename_component(compiler_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_file(asan_runtime NAMES clang_rt.asan_dynamic-x86_64.dll
        HINTS "${compiler_directory}" NO_DEFAULT_PATH REQUIRED)
    add_custom_command(TARGET mwfl_package_consumer POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${asan_runtime}" "$<TARGET_FILE_DIR:mwfl_package_consumer>")
endif()
