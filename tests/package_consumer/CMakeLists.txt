cmake_minimum_required(VERSION 3.21)
project(mwtl_package_consumer LANGUAGES CXX)

set(mwtl_components d2d d3d imaging)
if(MWTL_TEST_SCINTILLA)
    list(APPEND mwtl_components scintilla)
endif()
if(MWTL_TEST_WEBVIEW2)
    list(APPEND mwtl_components webview2)
endif()
find_package(mwtl 0.1 CONFIG REQUIRED COMPONENTS ${mwtl_components})

get_target_property(mwtl_core_links mwtl::mwtl INTERFACE_LINK_LIBRARIES)
if("${mwtl_core_links}" MATCHES "d2d|d3d|dxgi|windowscodecs|scintilla|webview")
    message(FATAL_ERROR "core mwtl::mwtl unexpectedly exposes optional rendering linkage")
endif()
if(MWTL_TEST_WEBVIEW2)
    add_executable(mwtl_webview2_package_consumer webview2_main.cpp)
    target_link_libraries(mwtl_webview2_package_consumer PRIVATE mwtl::webview2)
    target_compile_features(mwtl_webview2_package_consumer PRIVATE cxx_std_20)
endif()

add_executable(mwtl_package_consumer main.cpp)
target_link_libraries(mwtl_package_consumer PRIVATE mwtl::mwtl)
target_compile_features(mwtl_package_consumer PRIVATE cxx_std_20)

add_executable(mwtl_d2d_package_consumer d2d_main.cpp)
target_link_libraries(mwtl_d2d_package_consumer PRIVATE mwtl::d2d)
target_compile_features(mwtl_d2d_package_consumer PRIVATE cxx_std_20)

add_executable(mwtl_d3d_package_consumer d3d_main.cpp)
target_link_libraries(mwtl_d3d_package_consumer PRIVATE mwtl::d3d)
target_compile_features(mwtl_d3d_package_consumer PRIVATE cxx_std_20)

add_executable(mwtl_imaging_package_consumer imaging_main.cpp)
target_link_libraries(mwtl_imaging_package_consumer PRIVATE mwtl::imaging)
target_compile_features(mwtl_imaging_package_consumer PRIVATE cxx_std_20)

if(MWTL_TEST_SCINTILLA)
    add_executable(mwtl_scintilla_package_consumer scintilla_main.cpp)
    target_link_libraries(mwtl_scintilla_package_consumer PRIVATE mwtl::scintilla)
    target_compile_features(mwtl_scintilla_package_consumer PRIVATE cxx_std_20)
    mwtl_deploy_scintilla(mwtl_scintilla_package_consumer)
endif()
if(MWTL_ENABLE_ASAN)
    target_compile_options(mwtl_package_consumer PRIVATE /fsanitize=address)
    get_filename_component(compiler_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_file(asan_runtime NAMES clang_rt.asan_dynamic-x86_64.dll
        HINTS "${compiler_directory}" NO_DEFAULT_PATH REQUIRED)
    add_custom_command(TARGET mwtl_package_consumer POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${asan_runtime}" "$<TARGET_FILE_DIR:mwtl_package_consumer>")
endif()
