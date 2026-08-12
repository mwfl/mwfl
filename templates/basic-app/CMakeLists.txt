cmake_minimum_required(VERSION 3.21)
project(mwfl_basic_app LANGUAGES CXX)

if(MWFL_SOURCE_DIR)
    add_subdirectory("${MWFL_SOURCE_DIR}" mwfl-source EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(mwfl
        GIT_REPOSITORY https://github.com/mwfl/mwfl.git
        GIT_TAG v0.1.2)
    FetchContent_MakeAvailable(mwfl)
endif()

add_executable(mwfl_basic_app WIN32 main.cpp app.manifest)
set_property(SOURCE app.manifest PROPERTY VS_TOOL_OVERRIDE "Manifest")
target_compile_features(mwfl_basic_app PRIVATE cxx_std_20)
target_compile_options(mwfl_basic_app PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_basic_app PRIVATE mwfl::mwfl)
