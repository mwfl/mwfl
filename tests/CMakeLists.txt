add_executable(mwtl_lifecycle_test lifecycle_test.cpp)
target_compile_features(mwtl_lifecycle_test PRIVATE cxx_std_20)
target_compile_options(mwtl_lifecycle_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_compile_definitions(mwtl_lifecycle_test PRIVATE MWTL_TESTING)
target_include_directories(mwtl_lifecycle_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(mwtl_lifecycle_test PRIVATE mwtl::mwtl)

foreach(mode IN ITEMS
        normal
        module_init_failure
        loop_registration_failure
        create_failure
        allocation_failure
        message_failure
        destruction_failure)
    add_test(NAME mwtl.lifecycle.${mode} COMMAND mwtl_lifecycle_test ${mode})
    set(test_environment "MWTL_TEST_NO_DIALOGS=1")
    if(mode STREQUAL "module_init_failure")
        list(APPEND test_environment "MWTL_TEST_FAIL_MODULE_INIT=1")
    elseif(mode STREQUAL "loop_registration_failure")
        list(APPEND test_environment "MWTL_TEST_FAIL_LOOP_REGISTRATION=1")
    endif()
    set_tests_properties(mwtl.lifecycle.${mode} PROPERTIES
        ENVIRONMENT "${test_environment}"
        TIMEOUT 20)
endforeach()

foreach(header IN ITEMS application command_controls concepts control_batch control_host control_resources controls desktop document dpi events input_controls layout message_pump must navigation_controls text_file text_history text_search timer window window_options wakeup umbrella)
    # Every public header must remain independently includable.
    add_library(mwtl_header_${header} OBJECT header_${header}.cpp)
    target_compile_features(mwtl_header_${header} PRIVATE cxx_std_20)
    target_compile_options(mwtl_header_${header} PRIVATE /W4 /permissive- /EHsc /utf-8)
    target_link_libraries(mwtl_header_${header} PRIVATE mwtl::mwtl)
endforeach()

add_library(mwtl_header_appearance OBJECT header_appearance.cpp)
target_compile_features(mwtl_header_appearance PRIVATE cxx_std_20)
target_compile_options(mwtl_header_appearance PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_header_appearance PRIVATE mwtl::mwtl)

add_library(mwtl_header_binding OBJECT header_binding.cpp)
target_compile_features(mwtl_header_binding PRIVATE cxx_std_20)
target_compile_options(mwtl_header_binding PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_header_binding PRIVATE mwtl::mwtl)

add_library(mwtl_header_command OBJECT header_command.cpp)
target_compile_features(mwtl_header_command PRIVATE cxx_std_20)
target_compile_options(mwtl_header_command PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_header_command PRIVATE mwtl::mwtl)

add_library(mwtl_header_error OBJECT header_error.cpp)
target_compile_features(mwtl_header_error PRIVATE cxx_std_20)
target_compile_options(mwtl_header_error PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_header_error PRIVATE mwtl::mwtl)

add_library(mwtl_consumer_cpp20 OBJECT consumer_cpp20.cpp)
target_compile_features(mwtl_consumer_cpp20 PRIVATE cxx_std_20)
target_compile_options(mwtl_consumer_cpp20 PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_consumer_cpp20 PRIVATE mwtl::mwtl)

# Keep the coding-agent public API probe compiled with every repository build.
add_library(mwtl_agent_api_probe OBJECT "${PROJECT_SOURCE_DIR}/tools/api-probe/main.cpp")
target_compile_features(mwtl_agent_api_probe PRIVATE cxx_std_20)
target_compile_options(mwtl_agent_api_probe PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_agent_api_probe PRIVATE mwtl::mwtl)

add_library(mwtl_01_compat OBJECT consumer_01_compat.cpp)
target_compile_features(mwtl_01_compat PRIVATE cxx_std_20)
target_compile_options(mwtl_01_compat PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_01_compat PRIVATE mwtl::mwtl)

add_library(mwtl_direct_create_test OBJECT direct_create_test.cpp)
target_compile_features(mwtl_direct_create_test PRIVATE cxx_std_20)
target_compile_options(mwtl_direct_create_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_direct_create_test PRIVATE mwtl::mwtl)

add_executable(mwtl_dpi_test dpi_test.cpp)
target_compile_features(mwtl_dpi_test PRIVATE cxx_std_20)
target_compile_options(mwtl_dpi_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_dpi_test PRIVATE mwtl::mwtl)
add_test(NAME mwtl.dpi COMMAND mwtl_dpi_test)
set_tests_properties(mwtl.dpi PROPERTIES TIMEOUT 20)

add_executable(mwtl_must_test must_test.cpp)
target_compile_features(mwtl_must_test PRIVATE cxx_std_20)
target_compile_options(mwtl_must_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_must_test PRIVATE mwtl::mwtl)
add_test(NAME mwtl.must COMMAND mwtl_must_test)
set_tests_properties(mwtl.must PROPERTIES TIMEOUT 20)

add_executable(mwtl_control_batch_test control_batch_test.cpp)
target_compile_features(mwtl_control_batch_test PRIVATE cxx_std_20)
target_compile_options(mwtl_control_batch_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_control_batch_test PRIVATE mwtl::mwtl)
add_test(NAME mwtl.control_batch COMMAND mwtl_control_batch_test)
set_tests_properties(mwtl.control_batch PROPERTIES TIMEOUT 20)

add_executable(mwtl_layout_test layout_test.cpp)
target_compile_features(mwtl_layout_test PRIVATE cxx_std_20)
target_compile_options(mwtl_layout_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_layout_test PRIVATE mwtl::mwtl)
add_test(NAME mwtl.layout COMMAND mwtl_layout_test)
set_tests_properties(mwtl.layout PROPERTIES TIMEOUT 20)

add_executable(mwtl_event_matching_test event_matching_test.cpp)
target_compile_features(mwtl_event_matching_test PRIVATE cxx_std_20)
target_compile_options(mwtl_event_matching_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_event_matching_test PRIVATE mwtl::mwtl)
add_test(NAME mwtl.event_matching COMMAND mwtl_event_matching_test)
set_tests_properties(mwtl.event_matching PROPERTIES TIMEOUT 20)

add_executable(mwtl_message_decode_test message_decode_test.cpp)
target_compile_features(mwtl_message_decode_test PRIVATE cxx_std_20)
target_compile_options(mwtl_message_decode_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_message_decode_test PRIVATE mwtl::mwtl)
add_test(NAME mwtl.message_decode COMMAND mwtl_message_decode_test)
set_tests_properties(mwtl.message_decode PROPERTIES TIMEOUT 20)

add_executable(mwtl_command_test command_test.cpp)
target_compile_features(mwtl_command_test PRIVATE cxx_std_20)
target_compile_options(mwtl_command_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_command_test PRIVATE mwtl::mwtl)
add_test(NAME mwtl.command COMMAND mwtl_command_test)
set_tests_properties(mwtl.command PROPERTIES TIMEOUT 20)

add_executable(mwtl_binding_test binding_test.cpp)
target_compile_features(mwtl_binding_test PRIVATE cxx_std_20)
target_compile_options(mwtl_binding_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_binding_test PRIVATE mwtl::mwtl)
add_test(NAME mwtl.binding COMMAND mwtl_binding_test)
set_tests_properties(mwtl.binding PROPERTIES TIMEOUT 20)

add_executable(mwtl_resource_leak_test resource_leak_test.cpp)
target_compile_features(mwtl_resource_leak_test PRIVATE cxx_std_20)
target_compile_options(mwtl_resource_leak_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_resource_leak_test PRIVATE mwtl::mwtl)
add_test(NAME mwtl.resource_leak COMMAND mwtl_resource_leak_test)
set_tests_properties(mwtl.resource_leak PROPERTIES TIMEOUT 20)

add_executable(mwtl_appearance_test appearance_test.cpp)
target_compile_features(mwtl_appearance_test PRIVATE cxx_std_20)
target_compile_options(mwtl_appearance_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_appearance_test PRIVATE mwtl::mwtl)
add_test(NAME mwtl.appearance COMMAND mwtl_appearance_test)
set_tests_properties(mwtl.appearance PROPERTIES TIMEOUT 20)

add_executable(mwtl_layout_quality_test layout_quality_test.cpp)
target_compile_features(mwtl_layout_quality_test PRIVATE cxx_std_20)
target_compile_options(mwtl_layout_quality_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_layout_quality_test PRIVATE mwtl::mwtl)
add_test(NAME mwtl.layout_quality COMMAND mwtl_layout_quality_test)
set_tests_properties(mwtl.layout_quality PROPERTIES TIMEOUT 20)

add_test(NAME mwtl.api_surface
    COMMAND "${CMAKE_COMMAND}" "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_api_surface.cmake")
set_tests_properties(mwtl.api_surface PROPERTIES TIMEOUT 20)

add_test(NAME mwtl.build_budget
    COMMAND "${CMAKE_COMMAND}"
        "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        "-DLIBRARY=$<TARGET_FILE:mwtl>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_build_budget.cmake")
set_tests_properties(mwtl.build_budget PROPERTIES TIMEOUT 20)

add_executable(mwtl_advanced_lifecycle_test advanced_lifecycle_test.cpp)
target_compile_features(mwtl_advanced_lifecycle_test PRIVATE cxx_std_20)
target_compile_options(mwtl_advanced_lifecycle_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_compile_definitions(mwtl_advanced_lifecycle_test PRIVATE MWTL_TESTING)
target_include_directories(mwtl_advanced_lifecycle_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(mwtl_advanced_lifecycle_test PRIVATE mwtl::mwtl)
foreach(mode IN ITEMS
        custom_window wait_handle wakeup wake_stress wake_latency idle_efficiency com_sta com_conflict
        pump_exception after_dispatch_exception wait_callback_exception
        wait_failure exact_exit dpi_change)
    add_test(NAME mwtl.advanced.${mode} COMMAND mwtl_advanced_lifecycle_test ${mode})
    set_tests_properties(mwtl.advanced.${mode} PROPERTIES
        ENVIRONMENT "MWTL_TEST_NO_DIALOGS=1"
        TIMEOUT 20)
endforeach()

add_executable(mwtl_modern_api_test modern_api_test.cpp)
target_sources(mwtl_modern_api_test PRIVATE "${PROJECT_SOURCE_DIR}/examples/example.manifest")
target_compile_features(mwtl_modern_api_test PRIVATE cxx_std_20)
target_compile_options(mwtl_modern_api_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_modern_api_test PRIVATE mwtl::mwtl)
add_test(NAME mwtl.modern_api COMMAND mwtl_modern_api_test)
set_tests_properties(mwtl.modern_api PROPERTIES
    ENVIRONMENT "MWTL_TEST_NO_DIALOGS=1"
    TIMEOUT 20)

add_executable(mwtl_desktop_test desktop_test.cpp)
target_compile_features(mwtl_desktop_test PRIVATE cxx_std_20)
target_compile_options(mwtl_desktop_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_desktop_test PRIVATE mwtl::mwtl)
add_test(NAME mwtl.desktop COMMAND mwtl_desktop_test --skip-clipboard)
set_tests_properties(mwtl.desktop PROPERTIES TIMEOUT 20)

add_executable(mwtl_document_test document_test.cpp)
target_compile_features(mwtl_document_test PRIVATE cxx_std_20)
target_compile_options(mwtl_document_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_document_test PRIVATE mwtl::mwtl)
add_test(NAME mwtl.document COMMAND mwtl_document_test)
set_tests_properties(mwtl.document PROPERTIES TIMEOUT 20)

add_executable(mwtl_text_file_test text_file_test.cpp)
target_compile_features(mwtl_text_file_test PRIVATE cxx_std_20)
target_compile_options(mwtl_text_file_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_text_file_test PRIVATE mwtl::mwtl)
add_test(NAME mwtl.text_file COMMAND mwtl_text_file_test)
set_tests_properties(mwtl.text_file PROPERTIES TIMEOUT 30)

add_executable(mwtl_text_search_test text_search_test.cpp)
target_compile_features(mwtl_text_search_test PRIVATE cxx_std_20)
target_compile_options(mwtl_text_search_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_text_search_test PRIVATE mwtl::mwtl)
add_test(NAME mwtl.text_search COMMAND mwtl_text_search_test)
set_tests_properties(mwtl.text_search PROPERTIES TIMEOUT 20)

add_executable(mwtl_text_history_test text_history_test.cpp)
target_compile_features(mwtl_text_history_test PRIVATE cxx_std_20)
target_compile_options(mwtl_text_history_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwtl_text_history_test PRIVATE mwtl::mwtl)
add_test(NAME mwtl.text_history COMMAND mwtl_text_history_test)
set_tests_properties(mwtl.text_history PROPERTIES TIMEOUT 20)

add_executable(mwtl_hot_corner_model_test
    hot_corner_model_test.cpp
    "${PROJECT_SOURCE_DIR}/examples/hot_corners/hot_corner_model.cpp")
target_compile_features(mwtl_hot_corner_model_test PRIVATE cxx_std_20)
target_compile_options(mwtl_hot_corner_model_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_include_directories(mwtl_hot_corner_model_test PRIVATE
    "${PROJECT_SOURCE_DIR}/examples/hot_corners")
add_test(NAME mwtl.hot_corner_model COMMAND mwtl_hot_corner_model_test)
set_tests_properties(mwtl.hot_corner_model PROPERTIES TIMEOUT 20)

if(TARGET mwtl_hello)
    set(mwtl_mt_tool "")
    if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
        set(mwtl_mt_tool
            "C:/Program Files (x86)/Windows Kits/10/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64/mt.exe")
    endif()
    if(NOT EXISTS "${mwtl_mt_tool}" AND EXISTS "${CMAKE_MT}")
        set(mwtl_mt_tool "${CMAKE_MT}")
    endif()
    if(NOT EXISTS "${mwtl_mt_tool}")
        find_program(mwtl_mt_tool NAMES mt.exe)
    endif()
    foreach(example_target IN ITEMS
            mwtl_hello
            mwtl_application_demo
            mwtl_window_demo
            mwtl_native_message_demo
            mwtl_keyboard_demo
            mwtl_mouse_demo
            mwtl_resize_demo
            mwtl_timer_demo
            mwtl_paint_demo
            mwtl_minmax_demo
            mwtl_close_policy_demo
            mwtl_window_state_demo
            mwtl_dpi_demo
            mwtl_window_options_demo
            mwtl_wait_aware_demo
            mwtl_wakeup_demo
            mwtl_com_sta_demo
            mwtl_controls_demo
            mwtl_common_controls_demo
            mwtl_self_drawn_host_demo
            mwtl_system_lifecycle_demo
            mwtl_hot_corners_demo
            mwtl_form_binding_demo
            mwtl_commands_demo
            mwtl_desktop_integration_demo
            mwtl_document_state_demo
            mwtl_notepad
            mwtl_appearance_demo
            mwtl_layout_gallery_demo)
        add_test(
            NAME mwtl.manifest.${example_target}
            COMMAND "${CMAKE_COMMAND}"
                "-DMT_TOOL=${mwtl_mt_tool}"
                "-DEXECUTABLE=$<TARGET_FILE:${example_target}>"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/${example_target}-manifest.xml"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_manifest.cmake")
        set_tests_properties(mwtl.manifest.${example_target} PROPERTIES TIMEOUT 20)
    endforeach()
    get_filename_component(
        mwtl_test_compiler_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(mwtl_test_runtime_path "${mwtl_test_compiler_directory}")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        file(GLOB mwtl_clang_runtime_directories LIST_DIRECTORIES true
            "${mwtl_test_compiler_directory}/../lib/clang/*/lib/windows")
        list(APPEND mwtl_test_runtime_path ${mwtl_clang_runtime_directories})
    endif()
    list(JOIN mwtl_test_runtime_path ";" mwtl_test_runtime_path)
    find_program(mwtl_powershell NAMES pwsh powershell REQUIRED)
    add_test(NAME mwtl.hot_corners_self_test
        COMMAND "${mwtl_powershell}" -NoProfile -NonInteractive
            -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/run_gui_self_test.ps1"
            -Executable $<TARGET_FILE:mwtl_hot_corners_demo>
            -RuntimePath "${mwtl_test_runtime_path}")
    set_tests_properties(mwtl.hot_corners_self_test PROPERTIES TIMEOUT 10)
endif()

add_test(
    NAME mwtl.site_links
    COMMAND "${CMAKE_COMMAND}"
        "-DSITE_ROOT=${PROJECT_SOURCE_DIR}/site"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_site.cmake")
set_tests_properties(mwtl.site_links PROPERTIES TIMEOUT 20)

add_test(
    NAME mwtl.examples.catalog
    COMMAND "${CMAKE_COMMAND}"
        "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_examples.cmake")
set_tests_properties(mwtl.examples.catalog PROPERTIES TIMEOUT 20)

add_test(
    NAME mwtl.package_consumer
    COMMAND "${CMAKE_COMMAND}"
        "-DMWTL_BUILD_DIR=${PROJECT_BINARY_DIR}"
        "-DMWTL_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DMWTL_INSTALL_DIR=${PROJECT_BINARY_DIR}/package-install"
        "-DMWTL_CONFIGURATION=$<CONFIG>"
        "-DMWTL_GENERATOR=${CMAKE_GENERATOR}"
        "-DMWTL_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DMWTL_ENABLE_ASAN=${MWTL_ENABLE_ASAN}"
        "-DMWTL_WTL_SOURCE_DIR=${MWTL_RESOLVED_WTL_SOURCE_DIR}"
        "-DMWTL_WIL_SOURCE_DIR=${MWTL_RESOLVED_WIL_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_package_consumer.cmake")
set_tests_properties(mwtl.package_consumer PROPERTIES TIMEOUT 120)

add_test(
    NAME mwtl.release_metadata
    COMMAND "${CMAKE_COMMAND}"
        "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_release_metadata.cmake")
set_tests_properties(mwtl.release_metadata PROPERTIES TIMEOUT 20)

add_test(
    NAME mwtl.agent_assets
    COMMAND "${CMAKE_COMMAND}"
        "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_agent_assets.cmake")
set_tests_properties(mwtl.agent_assets PROPERTIES TIMEOUT 20)

add_test(
    NAME mwtl.developer_assets
    COMMAND "${CMAKE_COMMAND}"
        "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_developer_assets.cmake")
set_tests_properties(mwtl.developer_assets PROPERTIES TIMEOUT 20)

add_test(
    NAME mwtl.agent_evals
    COMMAND "${CMAKE_COMMAND}"
        "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_agent_evals.cmake")
set_tests_properties(mwtl.agent_evals PROPERTIES TIMEOUT 20)

add_test(NAME mwtl.api_index
    COMMAND "${CMAKE_COMMAND}" "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_api_index.cmake")
set_tests_properties(mwtl.api_index PROPERTIES TIMEOUT 20)

find_program(mwtl_test_powershell NAMES pwsh powershell REQUIRED)
add_test(NAME mwtl.change_selector
    COMMAND "${mwtl_test_powershell}" -NoProfile -NonInteractive
        -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/verify_change_selector.ps1"
        -ProjectRoot "${PROJECT_SOURCE_DIR}")
set_tests_properties(mwtl.change_selector PROPERTIES TIMEOUT 20)

add_test(NAME mwtl.architecture_boundaries
    COMMAND "${CMAKE_COMMAND}" "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_architecture_boundaries.cmake")
set_tests_properties(mwtl.architecture_boundaries PROPERTIES TIMEOUT 20)

add_test(NAME mwtl.parity_roadmap
    COMMAND "${CMAKE_COMMAND}" "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_parity_roadmap.cmake")
set_tests_properties(mwtl.parity_roadmap PROPERTIES TIMEOUT 20)

# Every test has semantic labels so local agents can run a predictable subset.
get_property(mwtl_all_tests DIRECTORY PROPERTY TESTS)
foreach(test_name IN LISTS mwtl_all_tests)
    if(test_name MATCHES "^mwtl\\.(lifecycle|advanced\\.)")
        set(labels "lifecycle;fast")
    elseif(test_name MATCHES "^mwtl\\.manifest\\.")
        set(labels "metadata;fast")
    elseif(test_name STREQUAL "mwtl.hot_corners_self_test")
        set(labels "gui;integration")
    elseif(test_name MATCHES "^mwtl\\.(site_links|examples\\.catalog|release_metadata|agent_assets|developer_assets|agent_evals|api_index|change_selector|architecture_boundaries|parity_roadmap)$")
        set(labels "docs;metadata;fast")
    elseif(test_name STREQUAL "mwtl.package_consumer")
        set(labels "package;integration;slow")
    elseif(test_name MATCHES "^mwtl\\.(modern_api|desktop)$")
        set(labels "integration;fast")
    else()
        set(labels "unit;fast")
    endif()
    set_tests_properties(${test_name} PROPERTIES LABELS "${labels}")
endforeach()

foreach(test_name IN LISTS mwtl_all_tests)
    get_test_property(${test_name} LABELS labels)
    if(NOT labels)
        message(FATAL_ERROR "CTest test has no semantic labels: ${test_name}")
    endif()
endforeach()
