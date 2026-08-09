cmake_minimum_required(VERSION 3.21)
project(mwtl_basic_app LANGUAGES CXX)

if(MWTL_SOURCE_DIR)
    add_subdirectory("${MWTL_SOURCE_DIR}" mwtl-source EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(mwtl
        GIT_REPOSITORY https://github.com/everettjf/mwtl.git
        GIT_TAG v0.1.0)
    FetchContent_MakeAvailable(mwtl)
endif()

add_executable(mwtl_basic_app WIN32 main.cpp app.manifest)
set_property(SOURCE app.manifest PROPERTY VS_TOOL_OVERRIDE "Manifest")
target_compile_features(mwtl_basic_app PRIVATE cxx_std_20)
target_compile_options(mwtl_basic_app PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_basic_app PRIVATE mwtl::mwtl)
