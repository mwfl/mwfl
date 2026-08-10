cmake_minimum_required(VERSION 3.21)
project(mwfl_api_probe LANGUAGES CXX)

if(NOT MWFL_SOURCE_DIR)
    message(FATAL_ERROR "Set MWFL_SOURCE_DIR to the mwfl source directory")
endif()
add_subdirectory("${MWFL_SOURCE_DIR}" mwfl-source EXCLUDE_FROM_ALL)
add_executable(mwfl_api_probe WIN32 main.cpp ../../examples/example.manifest)
set_property(SOURCE ../../examples/example.manifest PROPERTY VS_TOOL_OVERRIDE "Manifest")
target_compile_features(mwfl_api_probe PRIVATE cxx_std_20)
target_compile_options(mwfl_api_probe PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_api_probe PRIVATE mwfl::mwfl)

