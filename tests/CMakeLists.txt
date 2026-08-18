add_executable(mwfl_lifecycle_test lifecycle_test.cpp)
target_compile_features(mwfl_lifecycle_test PRIVATE cxx_std_20)
target_compile_options(mwfl_lifecycle_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_compile_definitions(mwfl_lifecycle_test PRIVATE MWFL_TESTING)
target_include_directories(mwfl_lifecycle_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(mwfl_lifecycle_test PRIVATE mwfl::ui)

foreach(mode IN ITEMS
        normal
        module_init_failure
        loop_registration_failure
        create_failure
        allocation_failure
        message_failure
        destruction_failure)
    add_test(NAME mwfl.lifecycle.${mode} COMMAND mwfl_lifecycle_test ${mode})
    set(test_environment "MWFL_TEST_NO_DIALOGS=1")
    if(mode STREQUAL "module_init_failure")
        list(APPEND test_environment "MWFL_TEST_FAIL_MODULE_INIT=1")
    elseif(mode STREQUAL "loop_registration_failure")
        list(APPEND test_environment "MWFL_TEST_FAIL_LOOP_REGISTRATION=1")
    endif()
    set_tests_properties(mwfl.lifecycle.${mode} PROPERTIES
        ENVIRONMENT "${test_environment}"
        TIMEOUT 20)
endforeach()

add_executable(mwfl_window_core_test window_core_test.cpp)
target_compile_features(mwfl_window_core_test PRIVATE cxx_std_20)
target_compile_options(mwfl_window_core_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_window_core_test PRIVATE mwfl::ui)
add_test(NAME mwfl.window_core COMMAND mwfl_window_core_test)

add_executable(mwfl_core_test core_test.cpp)
target_compile_features(mwfl_core_test PRIVATE cxx_std_20)
target_compile_options(mwfl_core_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_core_test PRIVATE mwfl::core)
add_test(NAME mwfl.core COMMAND mwfl_core_test)
set_tests_properties(mwfl.core PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_library(mwfl_header_core OBJECT header_core.cpp)
target_compile_features(mwfl_header_core PRIVATE cxx_std_20)
target_compile_options(mwfl_header_core PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_core PRIVATE mwfl::core)
add_library(mwfl_header_ui_foundation OBJECT composition_ui_foundation.cpp)
target_compile_features(mwfl_header_ui_foundation PRIVATE cxx_std_20)
target_compile_options(mwfl_header_ui_foundation PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_ui_foundation PRIVATE mwfl::ui mwfl::deployment mwfl::security)

add_executable(mwfl_service_test service_test.cpp)
target_compile_features(mwfl_service_test PRIVATE cxx_std_20)
target_compile_options(mwfl_service_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_service_test PRIVATE mwfl::service)
add_test(NAME mwfl.service COMMAND mwfl_service_test)
set_tests_properties(mwfl.service PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_executable(mwfl_process_test process_test.cpp)
target_compile_features(mwfl_process_test PRIVATE cxx_std_20)
target_compile_options(mwfl_process_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_process_test PRIVATE mwfl::process)
add_test(NAME mwfl.process COMMAND mwfl_process_test)
set_tests_properties(mwfl.process PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_executable(mwfl_ipc_test ipc_test.cpp)
target_compile_features(mwfl_ipc_test PRIVATE cxx_std_20)
target_compile_options(mwfl_ipc_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_ipc_test PRIVATE mwfl::ipc)
add_test(NAME mwfl.ipc COMMAND mwfl_ipc_test)
set_tests_properties(mwfl.ipc PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_executable(mwfl_diagnostics_test diagnostics_test.cpp)
target_compile_features(mwfl_diagnostics_test PRIVATE cxx_std_20)
target_compile_options(mwfl_diagnostics_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_diagnostics_test PRIVATE mwfl::diagnostics)
add_test(NAME mwfl.diagnostics_public COMMAND mwfl_diagnostics_test)
set_tests_properties(mwfl.diagnostics_public PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_executable(mwfl_security_test security_test.cpp)
target_compile_features(mwfl_security_test PRIVATE cxx_std_20)
target_compile_options(mwfl_security_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_security_test PRIVATE mwfl::security)
add_test(NAME mwfl.security_public COMMAND mwfl_security_test)
set_tests_properties(mwfl.security_public PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_executable(mwfl_deployment_test deployment_test.cpp)
target_compile_features(mwfl_deployment_test PRIVATE cxx_std_20)
target_compile_options(mwfl_deployment_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_deployment_test PRIVATE mwfl::deployment)
add_test(NAME mwfl.deployment_public COMMAND mwfl_deployment_test)
set_tests_properties(mwfl.deployment_public PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_executable(mwfl_scheduler_test scheduler_test.cpp)
target_compile_features(mwfl_scheduler_test PRIVATE cxx_std_20)
target_compile_options(mwfl_scheduler_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_scheduler_test PRIVATE mwfl::scheduler)
add_test(NAME mwfl.scheduler COMMAND mwfl_scheduler_test)
set_tests_properties(mwfl.scheduler PROPERTIES TIMEOUT 20 LABELS "fast;unit")

foreach(component IN ITEMS service process ipc diagnostics security deployment scheduler)
    add_library(mwfl_header_foundation_${component} OBJECT header_${component}.cpp)
    target_compile_features(mwfl_header_foundation_${component} PRIVATE cxx_std_20)
    target_compile_options(mwfl_header_foundation_${component} PRIVATE /W4 /permissive- /EHsc /utf-8)
endforeach()
target_link_libraries(mwfl_header_foundation_service PRIVATE mwfl::service)
target_link_libraries(mwfl_header_foundation_process PRIVATE mwfl::process)
target_link_libraries(mwfl_header_foundation_ipc PRIVATE mwfl::ipc)
target_link_libraries(mwfl_header_foundation_diagnostics PRIVATE mwfl::diagnostics)
target_link_libraries(mwfl_header_foundation_security PRIVATE mwfl::security)
target_link_libraries(mwfl_header_foundation_deployment PRIVATE mwfl::deployment)
target_link_libraries(mwfl_header_foundation_scheduler PRIVATE mwfl::scheduler)
set_tests_properties(mwfl.window_core PROPERTIES
    ENVIRONMENT "MWFL_TEST_NO_DIALOGS=1"
    TIMEOUT 20 LABELS "fast;native;lifecycle")

foreach(header IN ITEMS application async command_controls concepts control_batch control_host control_resources controls desktop dialog document dpi events input_controls layout message_pump must native_host navigation_controls property_sheet recent_files selection single_instance splitter tab_workspace text_file text_history text_search timer tray_icon window window_options wakeup umbrella)
    # Every public header must remain independently includable.
    add_library(mwfl_header_${header} OBJECT header_${header}.cpp)
    target_compile_features(mwfl_header_${header} PRIVATE cxx_std_20)
    target_compile_options(mwfl_header_${header} PRIVATE /W4 /permissive- /EHsc /utf-8)
    target_link_libraries(mwfl_header_${header} PRIVATE mwfl::ui)
endforeach()

add_library(mwfl_header_appearance OBJECT header_appearance.cpp)
target_compile_features(mwfl_header_appearance PRIVATE cxx_std_20)
target_compile_options(mwfl_header_appearance PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_appearance PRIVATE mwfl::ui)

add_library(mwfl_header_binding OBJECT header_binding.cpp)
target_compile_features(mwfl_header_binding PRIVATE cxx_std_20)
target_compile_options(mwfl_header_binding PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_binding PRIVATE mwfl::ui)

add_library(mwfl_header_ribbon OBJECT header_ribbon.cpp)
target_compile_features(mwfl_header_ribbon PRIVATE cxx_std_20)
target_compile_options(mwfl_header_ribbon PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_ribbon PRIVATE mwfl::ribbon)

add_executable(mwfl_ribbon_model_test ribbon_model_test.cpp)
target_compile_features(mwfl_ribbon_model_test PRIVATE cxx_std_20)
target_compile_options(mwfl_ribbon_model_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_ribbon_model_test PRIVATE mwfl::ribbon)
add_test(NAME mwfl.ribbon_model COMMAND mwfl_ribbon_model_test)
set_tests_properties(mwfl.ribbon_model PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_library(mwfl_header_mdi OBJECT header_mdi.cpp)
target_compile_features(mwfl_header_mdi PRIVATE cxx_std_20)
target_compile_options(mwfl_header_mdi PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_mdi PRIVATE mwfl::mdi)

add_executable(mwfl_mdi_model_test mdi_model_test.cpp)
target_compile_features(mwfl_mdi_model_test PRIVATE cxx_std_20)
target_compile_options(mwfl_mdi_model_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_mdi_model_test PRIVATE mwfl::mdi)
add_test(NAME mwfl.mdi_model COMMAND mwfl_mdi_model_test)
set_tests_properties(mwfl.mdi_model PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_executable(mwfl_mdi_native_test mdi_native_test.cpp)
target_compile_features(mwfl_mdi_native_test PRIVATE cxx_std_20)
target_compile_options(mwfl_mdi_native_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_mdi_native_test PRIVATE mwfl::mdi)
add_test(NAME mwfl.mdi_native COMMAND mwfl_mdi_native_test)
set_tests_properties(mwfl.mdi_native PROPERTIES
    ENVIRONMENT "MWFL_TEST_NO_DIALOGS=1" TIMEOUT 30 LABELS "native;integration")

if(TARGET mwfl_mdi_workspace)
    add_test(NAME mwfl.mdi_workspace_gui
        COMMAND "${CMAKE_COMMAND}"
            "-DMDI_EXE=$<TARGET_FILE:mwfl_mdi_workspace>"
            "-DRESULT_FILE=${CMAKE_CURRENT_BINARY_DIR}/mdi-workspace-$<CONFIG>.result"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_mdi_workspace_gui.cmake")
set_tests_properties(mwfl.mdi_workspace_gui PROPERTIES
        ENVIRONMENT "MWFL_TEST_NO_DIALOGS=1" TIMEOUT 30 LABELS "gui;integration")
endif()

add_library(mwfl_header_graphics OBJECT header_graphics.cpp)
target_compile_features(mwfl_header_graphics PRIVATE cxx_std_20)
target_compile_options(mwfl_header_graphics PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_graphics PRIVATE mwfl::graphics)

add_executable(mwfl_graphics_test graphics_test.cpp)
target_compile_features(mwfl_graphics_test PRIVATE cxx_std_20)
target_compile_options(mwfl_graphics_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_graphics_test PRIVATE mwfl::graphics)
add_test(NAME mwfl.graphics COMMAND mwfl_graphics_test)
set_tests_properties(mwfl.graphics PROPERTIES TIMEOUT 30 LABELS "native;integration")

if(TARGET mwfl_graphics_interop)
    add_test(NAME mwfl.graphics_interop_gui
        COMMAND "${CMAKE_COMMAND}"
            "-DGRAPHICS_EXE=$<TARGET_FILE:mwfl_graphics_interop>"
            "-DRESULT_FILE=${CMAKE_CURRENT_BINARY_DIR}/graphics-interop-$<CONFIG>.result"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_graphics_interop_gui.cmake")
    set_tests_properties(mwfl.graphics_interop_gui PROPERTIES
        ENVIRONMENT "MWFL_TEST_NO_DIALOGS=1" TIMEOUT 30 LABELS "gui;integration")
endif()

add_library(mwfl_header_help OBJECT header_help.cpp)
target_compile_features(mwfl_header_help PRIVATE cxx_std_20)
target_compile_options(mwfl_header_help PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_help PRIVATE mwfl::shell)

add_executable(mwfl_help_test help_test.cpp)
target_compile_features(mwfl_help_test PRIVATE cxx_std_20)
target_compile_options(mwfl_help_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_help_test PRIVATE mwfl::shell)
add_test(NAME mwfl.help COMMAND mwfl_help_test)
set_tests_properties(mwfl.help PROPERTIES TIMEOUT 20 LABELS "fast;unit")

find_program(MWFL_UICC_EXECUTABLE NAMES uicc.exe
    HINTS
        "C:/Program Files (x86)/Windows Kits/10/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x86"
        "C:/Program Files (x86)/Windows Kits/10/bin/${CMAKE_SYSTEM_VERSION}/x86")
if(MWFL_UICC_EXECUTABLE)
    set(mwfl_ribbon_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/ribbon-generated")
    set(mwfl_ribbon_binary "${mwfl_ribbon_generated_dir}/ribbon_test.bml")
    set(mwfl_ribbon_resource "${mwfl_ribbon_generated_dir}/ribbon_test.rc")
    set(mwfl_ribbon_header "${mwfl_ribbon_generated_dir}/ribbon_test.h")
    add_custom_command(
        OUTPUT "${mwfl_ribbon_binary}" "${mwfl_ribbon_resource}" "${mwfl_ribbon_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${mwfl_ribbon_generated_dir}"
        COMMAND "${MWFL_UICC_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/ribbon_test.ribbon"
            "${mwfl_ribbon_binary}"
            "/header:${mwfl_ribbon_header}"
            "/res:${mwfl_ribbon_resource}"
            /name:TEST_RIBBON /W2
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/ribbon_test.ribbon"
        VERBATIM)
    add_executable(mwfl_ribbon_native_test ribbon_native_test.cpp
        "${mwfl_ribbon_resource}")
    target_compile_features(mwfl_ribbon_native_test PRIVATE cxx_std_20)
    target_compile_options(mwfl_ribbon_native_test PRIVATE /W4 /permissive- /EHsc /utf-8)
    target_link_libraries(mwfl_ribbon_native_test PRIVATE mwfl::ribbon)
    add_test(NAME mwfl.ribbon_native COMMAND mwfl_ribbon_native_test)
set_tests_properties(mwfl.ribbon_native PROPERTIES
        ENVIRONMENT "MWFL_TEST_NO_DIALOGS=1"
        TIMEOUT 30 LABELS "native;integration")
else()
    message(WARNING "uicc.exe not found; Ribbon native integration test is unavailable")
endif()

if(TARGET mwfl_ribbon_workspace)
    add_test(NAME mwfl.ribbon_workspace_gui
        COMMAND "${CMAKE_COMMAND}"
            "-DRIBBON_EXE=$<TARGET_FILE:mwfl_ribbon_workspace>"
            "-DRESULT_FILE=${CMAKE_CURRENT_BINARY_DIR}/ribbon-workspace-$<CONFIG>.result"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_ribbon_workspace_gui.cmake")
    set_tests_properties(mwfl.ribbon_workspace_gui PROPERTIES
        ENVIRONMENT "MWFL_TEST_NO_DIALOGS=1" TIMEOUT 30 LABELS "gui;integration")
endif()

add_library(mwfl_header_command OBJECT header_command.cpp)
target_compile_features(mwfl_header_command PRIVATE cxx_std_20)
target_compile_options(mwfl_header_command PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_command PRIVATE mwfl::ui)

add_library(mwfl_header_error OBJECT header_error.cpp)
target_compile_features(mwfl_header_error PRIVATE cxx_std_20)
target_compile_options(mwfl_header_error PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_error PRIVATE mwfl::ui)

add_library(mwfl_consumer_cpp20 OBJECT consumer_cpp20.cpp)
target_compile_features(mwfl_consumer_cpp20 PRIVATE cxx_std_20)
target_compile_options(mwfl_consumer_cpp20 PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_consumer_cpp20 PRIVATE mwfl::ui)

# This representative public surface becomes the v0.1 source baseline when the
# replacement release is tagged. Keep it compiling in every repository build.
add_library(mwfl_public_baseline OBJECT consumer_public_baseline.cpp)
target_compile_features(mwfl_public_baseline PRIVATE cxx_std_20)
target_compile_options(mwfl_public_baseline PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_public_baseline PRIVATE mwfl::ui)

# Keep the coding-agent public API probe compiled with every repository build.
add_library(mwfl_agent_api_probe OBJECT "${PROJECT_SOURCE_DIR}/tools/api-probe/main.cpp")
target_compile_features(mwfl_agent_api_probe PRIVATE cxx_std_20)
target_compile_options(mwfl_agent_api_probe PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_agent_api_probe PRIVATE mwfl::ui)

add_test(NAME mwfl.new_app_script
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/new_app_script_test.ps1"
        -ProjectRoot "${PROJECT_SOURCE_DIR}")
set_tests_properties(mwfl.new_app_script PROPERTIES TIMEOUT 20 LABELS "fast;docs")

add_library(mwfl_direct_create_test OBJECT direct_create_test.cpp)
target_compile_features(mwfl_direct_create_test PRIVATE cxx_std_20)
target_compile_options(mwfl_direct_create_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_direct_create_test PRIVATE mwfl::ui)

add_executable(mwfl_dpi_test dpi_test.cpp)
target_compile_features(mwfl_dpi_test PRIVATE cxx_std_20)
target_compile_options(mwfl_dpi_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_dpi_test PRIVATE mwfl::ui)
add_test(NAME mwfl.dpi COMMAND mwfl_dpi_test)
set_tests_properties(mwfl.dpi PROPERTIES TIMEOUT 20)

add_executable(mwfl_must_test must_test.cpp)
target_compile_features(mwfl_must_test PRIVATE cxx_std_20)
target_compile_options(mwfl_must_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_must_test PRIVATE mwfl::ui)
add_test(NAME mwfl.must COMMAND mwfl_must_test)
set_tests_properties(mwfl.must PROPERTIES TIMEOUT 20)

add_executable(mwfl_control_batch_test control_batch_test.cpp)
target_compile_features(mwfl_control_batch_test PRIVATE cxx_std_20)
target_compile_options(mwfl_control_batch_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_control_batch_test PRIVATE mwfl::ui)
add_test(NAME mwfl.control_batch COMMAND mwfl_control_batch_test)
set_tests_properties(mwfl.control_batch PROPERTIES TIMEOUT 20)

add_executable(mwfl_selection_test selection_test.cpp)
target_compile_features(mwfl_selection_test PRIVATE cxx_std_20)
target_compile_options(mwfl_selection_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_selection_test PRIVATE mwfl::ui)
add_test(NAME mwfl.selection COMMAND mwfl_selection_test)
set_tests_properties(mwfl.selection PROPERTIES TIMEOUT 20)

add_executable(mwfl_layout_test layout_test.cpp)
target_compile_features(mwfl_layout_test PRIVATE cxx_std_20)
target_compile_options(mwfl_layout_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_layout_test PRIVATE mwfl::ui)
add_test(NAME mwfl.layout COMMAND mwfl_layout_test)
set_tests_properties(mwfl.layout PROPERTIES TIMEOUT 20)

add_executable(mwfl_event_matching_test event_matching_test.cpp)
target_compile_features(mwfl_event_matching_test PRIVATE cxx_std_20)
target_compile_options(mwfl_event_matching_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_event_matching_test PRIVATE mwfl::ui)
add_test(NAME mwfl.event_matching COMMAND mwfl_event_matching_test)
set_tests_properties(mwfl.event_matching PROPERTIES TIMEOUT 20)

add_executable(mwfl_message_decode_test message_decode_test.cpp)
target_compile_features(mwfl_message_decode_test PRIVATE cxx_std_20)
target_compile_options(mwfl_message_decode_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_message_decode_test PRIVATE mwfl::ui)
add_test(NAME mwfl.message_decode COMMAND mwfl_message_decode_test)
set_tests_properties(mwfl.message_decode PROPERTIES TIMEOUT 20)

add_executable(mwfl_command_test command_test.cpp)
target_compile_features(mwfl_command_test PRIVATE cxx_std_20)
target_compile_options(mwfl_command_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_command_test PRIVATE mwfl::ui)
add_test(NAME mwfl.command COMMAND mwfl_command_test)
set_tests_properties(mwfl.command PROPERTIES TIMEOUT 20)

add_executable(mwfl_binding_test binding_test.cpp)
target_compile_features(mwfl_binding_test PRIVATE cxx_std_20)
target_compile_options(mwfl_binding_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_binding_test PRIVATE mwfl::ui)
add_test(NAME mwfl.binding COMMAND mwfl_binding_test)
set_tests_properties(mwfl.binding PROPERTIES TIMEOUT 20)

add_executable(mwfl_resource_leak_test resource_leak_test.cpp)
target_compile_features(mwfl_resource_leak_test PRIVATE cxx_std_20)
target_compile_options(mwfl_resource_leak_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_resource_leak_test PRIVATE mwfl::ui)
add_test(NAME mwfl.resource_leak COMMAND mwfl_resource_leak_test)
set_tests_properties(mwfl.resource_leak PROPERTIES TIMEOUT 20)

add_executable(mwfl_appearance_test appearance_test.cpp)
target_compile_features(mwfl_appearance_test PRIVATE cxx_std_20)
target_compile_options(mwfl_appearance_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_appearance_test PRIVATE mwfl::ui)
add_test(NAME mwfl.appearance COMMAND mwfl_appearance_test)
set_tests_properties(mwfl.appearance PROPERTIES TIMEOUT 20)

add_executable(mwfl_appearance_lifecycle_test WIN32 appearance_lifecycle_test.cpp)
target_compile_features(mwfl_appearance_lifecycle_test PRIVATE cxx_std_20)
target_compile_options(mwfl_appearance_lifecycle_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_appearance_lifecycle_test PRIVATE mwfl::ui)
add_test(NAME mwfl.appearance_lifecycle COMMAND mwfl_appearance_lifecycle_test)
set_tests_properties(mwfl.appearance_lifecycle PROPERTIES TIMEOUT 20 LABELS "native;unit;lifecycle")

add_executable(mwfl_layout_quality_test layout_quality_test.cpp)
target_compile_features(mwfl_layout_quality_test PRIVATE cxx_std_20)
target_compile_options(mwfl_layout_quality_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_layout_quality_test PRIVATE mwfl::ui)
add_test(NAME mwfl.layout_quality COMMAND mwfl_layout_quality_test)
set_tests_properties(mwfl.layout_quality PROPERTIES TIMEOUT 20)

add_test(NAME mwfl.api_surface
    COMMAND "${CMAKE_COMMAND}" "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_api_surface.cmake")
set_tests_properties(mwfl.api_surface PROPERTIES TIMEOUT 20)

add_test(NAME mwfl.header_probe_coverage
    COMMAND "${CMAKE_COMMAND}" "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_header_probes.cmake")
set_tests_properties(mwfl.header_probe_coverage PROPERTIES
    TIMEOUT 20 LABELS "fast;metadata")

add_test(NAME mwfl.build_budget
    COMMAND "${CMAKE_COMMAND}"
        "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        "-DLIBRARY=$<TARGET_FILE:mwfl>"
        "-DMWFL_ARCHITECTURE=${CMAKE_GENERATOR_PLATFORM}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_build_budget.cmake")
set_tests_properties(mwfl.build_budget PROPERTIES TIMEOUT 20)
if(MWFL_ENABLE_ASAN)
    # Instrumented archives contain sanitizer metadata and are not release-size
    # artifacts. Normal Debug/Release configurations retain the strict budget.
    set_tests_properties(mwfl.build_budget PROPERTIES DISABLED TRUE)
endif()

add_executable(mwfl_advanced_lifecycle_test advanced_lifecycle_test.cpp)
target_compile_features(mwfl_advanced_lifecycle_test PRIVATE cxx_std_20)
target_compile_options(mwfl_advanced_lifecycle_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_compile_definitions(mwfl_advanced_lifecycle_test PRIVATE MWFL_TESTING)
target_include_directories(mwfl_advanced_lifecycle_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(mwfl_advanced_lifecycle_test PRIVATE mwfl::ui)
foreach(mode IN ITEMS
        custom_window wait_handle wakeup wake_stress wake_latency idle_efficiency com_sta ole_sta com_conflict
        pump_exception after_dispatch_exception wait_callback_exception
        wait_failure exact_exit dpi_change)
    add_test(NAME mwfl.advanced.${mode} COMMAND mwfl_advanced_lifecycle_test ${mode})
    set_tests_properties(mwfl.advanced.${mode} PROPERTIES
        ENVIRONMENT "MWFL_TEST_NO_DIALOGS=1"
        TIMEOUT 20)
endforeach()

add_executable(mwfl_message_loop_test message_loop_test.cpp)
target_compile_features(mwfl_message_loop_test PRIVATE cxx_std_20)
target_compile_options(mwfl_message_loop_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_message_loop_test PRIVATE mwfl::ui)
# `unit` covers MessageLoop/MessageFilter/WaitAwareMessagePump semantics;
# `order` proves a modeless dialog registered after the main window's
# accelerator filter still sees keystrokes first; `twice` runs Application
# repeatedly in one process without leaked process-wide framework state.
foreach(mode IN ITEMS unit order twice)
    add_test(NAME mwfl.message_loop.${mode} COMMAND mwfl_message_loop_test ${mode})
    set_tests_properties(mwfl.message_loop.${mode} PROPERTIES
        ENVIRONMENT "MWFL_TEST_NO_DIALOGS=1"
        TIMEOUT 30)
endforeach()

add_executable(mwfl_modern_api_test modern_api_test.cpp)
target_sources(mwfl_modern_api_test PRIVATE "${PROJECT_SOURCE_DIR}/examples/example.manifest")
target_compile_features(mwfl_modern_api_test PRIVATE cxx_std_20)
target_compile_options(mwfl_modern_api_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_modern_api_test PRIVATE mwfl::ui)
add_test(NAME mwfl.modern_api COMMAND mwfl_modern_api_test)
set_tests_properties(mwfl.modern_api PROPERTIES
    ENVIRONMENT "MWFL_TEST_NO_DIALOGS=1"
    TIMEOUT 20)

add_executable(mwfl_desktop_test desktop_test.cpp)
target_compile_features(mwfl_desktop_test PRIVATE cxx_std_20)
target_compile_options(mwfl_desktop_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_desktop_test PRIVATE mwfl::ui)
add_test(NAME mwfl.desktop COMMAND mwfl_desktop_test --skip-clipboard)
set_tests_properties(mwfl.desktop PROPERTIES TIMEOUT 20)

add_executable(mwfl_document_test document_test.cpp)
target_compile_features(mwfl_document_test PRIVATE cxx_std_20)
target_compile_options(mwfl_document_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_document_test PRIVATE mwfl::ui)
add_test(NAME mwfl.document COMMAND mwfl_document_test)
set_tests_properties(mwfl.document PROPERTIES TIMEOUT 20)

add_executable(mwfl_text_file_test text_file_test.cpp)
target_compile_features(mwfl_text_file_test PRIVATE cxx_std_20)
target_compile_options(mwfl_text_file_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_text_file_test PRIVATE mwfl::ui)
add_test(NAME mwfl.text_file COMMAND mwfl_text_file_test)
set_tests_properties(mwfl.text_file PROPERTIES TIMEOUT 30)

add_executable(mwfl_text_search_test text_search_test.cpp)
target_compile_features(mwfl_text_search_test PRIVATE cxx_std_20)
target_compile_options(mwfl_text_search_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_text_search_test PRIVATE mwfl::ui)
add_test(NAME mwfl.text_search COMMAND mwfl_text_search_test)
set_tests_properties(mwfl.text_search PROPERTIES TIMEOUT 20)

add_executable(mwfl_text_history_test text_history_test.cpp)
target_compile_features(mwfl_text_history_test PRIVATE cxx_std_20)
target_compile_options(mwfl_text_history_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_text_history_test PRIVATE mwfl::ui)
add_test(NAME mwfl.text_history COMMAND mwfl_text_history_test)
set_tests_properties(mwfl.text_history PROPERTIES TIMEOUT 20)

add_executable(mwfl_recent_files_test recent_files_test.cpp)
target_compile_features(mwfl_recent_files_test PRIVATE cxx_std_20)
target_compile_options(mwfl_recent_files_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_recent_files_test PRIVATE mwfl::ui)
add_test(NAME mwfl.recent_files COMMAND mwfl_recent_files_test)
set_tests_properties(mwfl.recent_files PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77)

add_executable(mwfl_single_instance_test single_instance_test.cpp)
target_compile_features(mwfl_single_instance_test PRIVATE cxx_std_20)
target_compile_options(mwfl_single_instance_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_single_instance_test PRIVATE mwfl::ui)
add_test(NAME mwfl.single_instance COMMAND mwfl_single_instance_test)
set_tests_properties(mwfl.single_instance PROPERTIES TIMEOUT 20)

add_executable(mwfl_tab_workspace_test tab_workspace_test.cpp)
target_compile_features(mwfl_tab_workspace_test PRIVATE cxx_std_20)
target_compile_options(mwfl_tab_workspace_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_tab_workspace_test PRIVATE mwfl::ui)
add_test(NAME mwfl.tab_workspace COMMAND mwfl_tab_workspace_test)
set_tests_properties(mwfl.tab_workspace PROPERTIES TIMEOUT 20)

add_library(mwfl_header_docking_workspace OBJECT header_docking_workspace.cpp)
target_compile_features(mwfl_header_docking_workspace PRIVATE cxx_std_20)
target_compile_options(mwfl_header_docking_workspace PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_docking_workspace PRIVATE mwfl::ui)

add_library(mwfl_header_docking_session OBJECT header_docking_session.cpp)
target_compile_features(mwfl_header_docking_session PRIVATE cxx_std_20)
target_compile_options(mwfl_header_docking_session PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_docking_session PRIVATE mwfl::ui)

add_library(mwfl_header_docking_monitor OBJECT header_docking_monitor.cpp)
target_compile_features(mwfl_header_docking_monitor PRIVATE cxx_std_20)
target_compile_options(mwfl_header_docking_monitor PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_docking_monitor PRIVATE mwfl::ui)

add_library(mwfl_header_docking_drag OBJECT header_docking_drag.cpp)
target_compile_features(mwfl_header_docking_drag PRIVATE cxx_std_20)
target_compile_options(mwfl_header_docking_drag PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_docking_drag PRIVATE mwfl::ui)

add_library(mwfl_header_docking_auto_hide OBJECT header_docking_auto_hide.cpp)
target_compile_features(mwfl_header_docking_auto_hide PRIVATE cxx_std_20)
target_compile_options(mwfl_header_docking_auto_hide PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_docking_auto_hide PRIVATE mwfl::ui)

add_library(mwfl_header_docking_keyboard OBJECT header_docking_keyboard.cpp)
target_compile_features(mwfl_header_docking_keyboard PRIVATE cxx_std_20)
target_compile_options(mwfl_header_docking_keyboard PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_docking_keyboard PRIVATE mwfl::ui)

add_library(mwfl_header_docking_floating OBJECT header_docking_floating.cpp)
target_compile_features(mwfl_header_docking_floating PRIVATE cxx_std_20)
target_compile_options(mwfl_header_docking_floating PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_docking_floating PRIVATE mwfl::ui)

add_library(mwfl_header_docking_preview OBJECT header_docking_preview.cpp)
target_compile_features(mwfl_header_docking_preview PRIVATE cxx_std_20)
target_compile_options(mwfl_header_docking_preview PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_docking_preview PRIVATE mwfl::ui)

add_library(mwfl_header_docking_native OBJECT header_docking_native.cpp)
target_compile_features(mwfl_header_docking_native PRIVATE cxx_std_20)
target_compile_options(mwfl_header_docking_native PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_docking_native PRIVATE mwfl::ui)

add_executable(mwfl_docking_workspace_test docking_workspace_test.cpp)
target_compile_features(mwfl_docking_workspace_test PRIVATE cxx_std_20)
target_compile_options(mwfl_docking_workspace_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_docking_workspace_test PRIVATE mwfl::ui)
add_test(NAME mwfl.docking_workspace COMMAND mwfl_docking_workspace_test)
set_tests_properties(mwfl.docking_workspace PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_executable(mwfl_docking_session_test docking_session_test.cpp)
target_compile_features(mwfl_docking_session_test PRIVATE cxx_std_20)
target_compile_options(mwfl_docking_session_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_docking_session_test PRIVATE mwfl::ui)
add_test(NAME mwfl.docking_session COMMAND mwfl_docking_session_test)
set_tests_properties(mwfl.docking_session PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_executable(mwfl_docking_monitor_test docking_monitor_test.cpp)
target_compile_features(mwfl_docking_monitor_test PRIVATE cxx_std_20)
target_compile_options(mwfl_docking_monitor_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_docking_monitor_test PRIVATE mwfl::ui)
add_test(NAME mwfl.docking_monitor COMMAND mwfl_docking_monitor_test)
set_tests_properties(mwfl.docking_monitor PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_executable(mwfl_docking_drag_test docking_drag_test.cpp)
target_compile_features(mwfl_docking_drag_test PRIVATE cxx_std_20)
target_compile_options(mwfl_docking_drag_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_docking_drag_test PRIVATE mwfl::ui)
add_test(NAME mwfl.docking_drag COMMAND mwfl_docking_drag_test)
set_tests_properties(mwfl.docking_drag PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_executable(mwfl_docking_preview_native_test docking_preview_native_test.cpp)
target_compile_features(mwfl_docking_preview_native_test PRIVATE cxx_std_20)
target_compile_options(mwfl_docking_preview_native_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_docking_preview_native_test PRIVATE mwfl::ui)
add_test(NAME mwfl.docking_preview_native COMMAND mwfl_docking_preview_native_test)
set_tests_properties(mwfl.docking_preview_native PROPERTIES TIMEOUT 20 LABELS "native;unit")

add_executable(mwfl_docking_native_test docking_native_test.cpp)
target_compile_features(mwfl_docking_native_test PRIVATE cxx_std_20)
target_compile_options(mwfl_docking_native_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_docking_native_test PRIVATE mwfl::ui)
add_test(NAME mwfl.docking_native COMMAND mwfl_docking_native_test)
set_tests_properties(mwfl.docking_native PROPERTIES TIMEOUT 20 LABELS "native;unit")

add_executable(mwfl_docking_auto_hide_test docking_auto_hide_test.cpp)
target_compile_features(mwfl_docking_auto_hide_test PRIVATE cxx_std_20)
target_compile_options(mwfl_docking_auto_hide_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_docking_auto_hide_test PRIVATE mwfl::ui)
add_test(NAME mwfl.docking_auto_hide COMMAND mwfl_docking_auto_hide_test)
set_tests_properties(mwfl.docking_auto_hide PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_executable(mwfl_docking_keyboard_test docking_keyboard_test.cpp)
target_compile_features(mwfl_docking_keyboard_test PRIVATE cxx_std_20)
target_compile_options(mwfl_docking_keyboard_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_docking_keyboard_test PRIVATE mwfl::ui)
add_test(NAME mwfl.docking_keyboard COMMAND mwfl_docking_keyboard_test)
set_tests_properties(mwfl.docking_keyboard PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_executable(mwfl_docking_floating_native_test docking_floating_native_test.cpp)
target_compile_features(mwfl_docking_floating_native_test PRIVATE cxx_std_20)
target_compile_options(mwfl_docking_floating_native_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_docking_floating_native_test PRIVATE mwfl::ui)
add_test(NAME mwfl.docking_floating_native COMMAND mwfl_docking_floating_native_test)
set_tests_properties(mwfl.docking_floating_native PROPERTIES TIMEOUT 20 LABELS "native;unit")

add_executable(mwfl_docking_stress_native_test docking_stress_native_test.cpp)
target_compile_features(mwfl_docking_stress_native_test PRIVATE cxx_std_20)
target_compile_options(mwfl_docking_stress_native_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_docking_stress_native_test PRIVATE mwfl::ui)
add_test(NAME mwfl.docking_stress_native COMMAND mwfl_docking_stress_native_test)
set_tests_properties(mwfl.docking_stress_native PROPERTIES TIMEOUT 30 LABELS "native;lifecycle")

add_library(mwfl_header_document_workspace OBJECT header_document_workspace.cpp)
target_compile_features(mwfl_header_document_workspace PRIVATE cxx_std_20)
target_compile_options(mwfl_header_document_workspace PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_document_workspace PRIVATE mwfl::ui)

add_library(mwfl_header_document_coordination OBJECT header_document_coordination.cpp)
target_compile_features(mwfl_header_document_coordination PRIVATE cxx_std_20)
target_compile_options(mwfl_header_document_coordination PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_document_coordination PRIVATE mwfl::ui)

add_library(mwfl_header_document_session OBJECT header_document_session.cpp)
target_compile_features(mwfl_header_document_session PRIVATE cxx_std_20)
target_compile_options(mwfl_header_document_session PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_document_session PRIVATE mwfl::ui)

add_library(mwfl_header_document_tabs OBJECT header_document_tabs.cpp)
target_compile_features(mwfl_header_document_tabs PRIVATE cxx_std_20)
target_compile_options(mwfl_header_document_tabs PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_document_tabs PRIVATE mwfl::ui)

add_executable(mwfl_document_workspace_test document_workspace_test.cpp)
target_compile_features(mwfl_document_workspace_test PRIVATE cxx_std_20)
target_compile_options(mwfl_document_workspace_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_document_workspace_test PRIVATE mwfl::ui)
add_test(NAME mwfl.document_workspace COMMAND mwfl_document_workspace_test)
set_tests_properties(mwfl.document_workspace PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_executable(mwfl_document_coordination_test document_coordination_test.cpp)
target_compile_features(mwfl_document_coordination_test PRIVATE cxx_std_20)
target_compile_options(mwfl_document_coordination_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_document_coordination_test PRIVATE mwfl::ui)
add_test(NAME mwfl.document_coordination COMMAND mwfl_document_coordination_test)
set_tests_properties(mwfl.document_coordination PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_executable(mwfl_document_session_test document_session_test.cpp)
target_compile_features(mwfl_document_session_test PRIVATE cxx_std_20)
target_compile_options(mwfl_document_session_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_document_session_test PRIVATE mwfl::ui)
add_test(NAME mwfl.document_session COMMAND mwfl_document_session_test)
set_tests_properties(mwfl.document_session PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_executable(mwfl_session_parser_fuzz_test session_parser_fuzz_test.cpp)
target_compile_features(mwfl_session_parser_fuzz_test PRIVATE cxx_std_20)
target_compile_options(mwfl_session_parser_fuzz_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_session_parser_fuzz_test PRIVATE mwfl::ui)
add_test(NAME mwfl.session_parser_fuzz COMMAND mwfl_session_parser_fuzz_test)
set_tests_properties(mwfl.session_parser_fuzz PROPERTIES TIMEOUT 30 LABELS "fast;unit;fuzz")

add_executable(mwfl_document_workspace_stress_test document_workspace_stress_test.cpp)
target_compile_features(mwfl_document_workspace_stress_test PRIVATE cxx_std_20)
target_compile_options(mwfl_document_workspace_stress_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_document_workspace_stress_test PRIVATE mwfl::ui)
add_test(NAME mwfl.document_workspace_stress COMMAND mwfl_document_workspace_stress_test)
set_tests_properties(mwfl.document_workspace_stress PROPERTIES TIMEOUT 30 LABELS "lifecycle;unit")

add_executable(mwfl_document_tabs_native_test document_tabs_native_test.cpp)
target_compile_features(mwfl_document_tabs_native_test PRIVATE cxx_std_20)
target_compile_options(mwfl_document_tabs_native_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_document_tabs_native_test PRIVATE mwfl::ui)
add_test(NAME mwfl.document_tabs_native COMMAND mwfl_document_tabs_native_test)
set_tests_properties(mwfl.document_tabs_native PROPERTIES TIMEOUT 20 LABELS "native;unit")

add_executable(mwfl_window_quit_policy_test WIN32 window_quit_policy_test.cpp)
target_compile_features(mwfl_window_quit_policy_test PRIVATE cxx_std_20)
target_compile_options(mwfl_window_quit_policy_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_window_quit_policy_test PRIVATE mwfl::ui)
add_test(NAME mwfl.window_quit_policy COMMAND mwfl_window_quit_policy_test)
set_tests_properties(mwfl.window_quit_policy PROPERTIES TIMEOUT 20 LABELS "native;unit")

add_executable(mwfl_splitter_test splitter_test.cpp)
target_compile_features(mwfl_splitter_test PRIVATE cxx_std_20)
target_compile_options(mwfl_splitter_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_splitter_test PRIVATE mwfl::ui)
add_test(NAME mwfl.splitter COMMAND mwfl_splitter_test)
set_tests_properties(mwfl.splitter PROPERTIES TIMEOUT 20)

add_executable(mwfl_splitter_native_test splitter_native_test.cpp)
target_compile_features(mwfl_splitter_native_test PRIVATE cxx_std_20)
target_compile_options(mwfl_splitter_native_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_splitter_native_test PRIVATE mwfl::ui oleacc)
add_test(NAME mwfl.splitter_native COMMAND mwfl_splitter_native_test)
set_tests_properties(mwfl.splitter_native PROPERTIES TIMEOUT 20)

add_executable(mwfl_native_host_model_test native_host_model_test.cpp)
target_compile_features(mwfl_native_host_model_test PRIVATE cxx_std_20)
target_compile_options(mwfl_native_host_model_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_native_host_model_test PRIVATE mwfl::ui)
add_test(NAME mwfl.native_host_model COMMAND mwfl_native_host_model_test)
set_tests_properties(mwfl.native_host_model PROPERTIES LABELS "fast;unit")

add_executable(mwfl_native_host_native_test native_host_native_test.cpp)
target_sources(mwfl_native_host_native_test PRIVATE "${PROJECT_SOURCE_DIR}/examples/example.manifest")
target_compile_features(mwfl_native_host_native_test PRIVATE cxx_std_20)
target_compile_options(mwfl_native_host_native_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_native_host_native_test PRIVATE mwfl::ui)
add_test(NAME mwfl.native_host_native COMMAND mwfl_native_host_native_test)
set_tests_properties(mwfl.native_host_native PROPERTIES TIMEOUT 30 LABELS "fast;unit")

add_executable(mwfl_async_test async_test.cpp)
target_compile_features(mwfl_async_test PRIVATE cxx_std_20)
target_compile_options(mwfl_async_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_async_test PRIVATE mwfl::ui)
add_test(NAME mwfl.async COMMAND mwfl_async_test)
set_tests_properties(mwfl.async PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_library(mwfl_header_d2d_host OBJECT header_d2d_host.cpp)
target_compile_features(mwfl_header_d2d_host PRIVATE cxx_std_20)
target_compile_options(mwfl_header_d2d_host PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_d2d_host PRIVATE mwfl::d2d)

add_executable(mwfl_d2d_host_model_test d2d_host_model_test.cpp)
target_compile_features(mwfl_d2d_host_model_test PRIVATE cxx_std_20)
target_compile_options(mwfl_d2d_host_model_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_d2d_host_model_test PRIVATE mwfl::d2d)
add_test(NAME mwfl.d2d_host_model COMMAND mwfl_d2d_host_model_test)
set_tests_properties(mwfl.d2d_host_model PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_executable(mwfl_d2d_host_native_test WIN32 d2d_host_native_test.cpp)
target_sources(mwfl_d2d_host_native_test PRIVATE "${PROJECT_SOURCE_DIR}/examples/example.manifest")
target_compile_features(mwfl_d2d_host_native_test PRIVATE cxx_std_20)
target_compile_options(mwfl_d2d_host_native_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_d2d_host_native_test PRIVATE mwfl::d2d)
add_test(NAME mwfl.d2d_host_native COMMAND mwfl_d2d_host_native_test)
set_tests_properties(mwfl.d2d_host_native PROPERTIES TIMEOUT 30 LABELS "fast;unit")

add_executable(mwfl_drawing_model_test
    drawing_model_test.cpp
    "${PROJECT_SOURCE_DIR}/examples/drawing/drawing_model.cpp")
target_include_directories(mwfl_drawing_model_test PRIVATE "${PROJECT_SOURCE_DIR}/examples/drawing")
target_compile_features(mwfl_drawing_model_test PRIVATE cxx_std_20)
target_compile_options(mwfl_drawing_model_test PRIVATE /W4 /permissive- /EHsc /utf-8)
add_test(NAME mwfl.drawing_model COMMAND mwfl_drawing_model_test)
set_tests_properties(mwfl.drawing_model PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_library(mwfl_header_d3d_host OBJECT header_d3d_host.cpp)
target_compile_features(mwfl_header_d3d_host PRIVATE cxx_std_20)
target_compile_options(mwfl_header_d3d_host PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_d3d_host PRIVATE mwfl::d3d)

add_executable(mwfl_d3d_host_model_test d3d_host_model_test.cpp)
target_compile_features(mwfl_d3d_host_model_test PRIVATE cxx_std_20)
target_compile_options(mwfl_d3d_host_model_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_d3d_host_model_test PRIVATE mwfl::d3d)
add_test(NAME mwfl.d3d_host_model COMMAND mwfl_d3d_host_model_test)
set_tests_properties(mwfl.d3d_host_model PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_executable(mwfl_d3d_host_native_test WIN32 d3d_host_native_test.cpp)
target_sources(mwfl_d3d_host_native_test PRIVATE "${PROJECT_SOURCE_DIR}/examples/example.manifest")
target_compile_features(mwfl_d3d_host_native_test PRIVATE cxx_std_20)
target_compile_options(mwfl_d3d_host_native_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_d3d_host_native_test PRIVATE mwfl::d3d)
add_test(NAME mwfl.d3d_host_native COMMAND mwfl_d3d_host_native_test)
set_tests_properties(mwfl.d3d_host_native PROPERTIES TIMEOUT 30 LABELS "fast;unit")

add_library(mwfl_header_imaging OBJECT header_imaging.cpp)
target_compile_features(mwfl_header_imaging PRIVATE cxx_std_20)
target_compile_options(mwfl_header_imaging PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_imaging PRIVATE mwfl::imaging)

add_executable(mwfl_imaging_model_test imaging_model_test.cpp)
target_compile_features(mwfl_imaging_model_test PRIVATE cxx_std_20)
target_compile_options(mwfl_imaging_model_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_imaging_model_test PRIVATE mwfl::imaging)
add_test(NAME mwfl.imaging_model COMMAND mwfl_imaging_model_test)
set_tests_properties(mwfl.imaging_model PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_library(mwfl_header_printing OBJECT header_printing.cpp)
target_compile_features(mwfl_header_printing PRIVATE cxx_std_20)
target_compile_options(mwfl_header_printing PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_printing PRIVATE mwfl::printing)

add_executable(mwfl_printing_model_test printing_model_test.cpp)
target_compile_features(mwfl_printing_model_test PRIVATE cxx_std_20)
target_compile_options(mwfl_printing_model_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_printing_model_test PRIVATE mwfl::printing)
add_test(NAME mwfl.printing_model COMMAND mwfl_printing_model_test)
set_tests_properties(mwfl.printing_model PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_executable(mwfl_printing_native_test printing_native_test.cpp)
target_compile_features(mwfl_printing_native_test PRIVATE cxx_std_20)
target_compile_options(mwfl_printing_native_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_printing_native_test PRIVATE mwfl::printing)
add_test(NAME mwfl.printing_native COMMAND mwfl_printing_native_test)
set_tests_properties(mwfl.printing_native PROPERTIES TIMEOUT 20 LABELS "native")

add_library(mwfl_header_printing_native OBJECT header_printing_native.cpp)
target_compile_features(mwfl_header_printing_native PRIVATE cxx_std_20)
target_compile_options(mwfl_header_printing_native PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_printing_native PRIVATE mwfl::printing)

add_executable(mwfl_printing_job_test printing_job_test.cpp)
target_compile_features(mwfl_printing_job_test PRIVATE cxx_std_20)
target_compile_options(mwfl_printing_job_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_printing_job_test PRIVATE mwfl::printing)
add_test(NAME mwfl.printing_job COMMAND mwfl_printing_job_test)
set_tests_properties(mwfl.printing_job PROPERTIES TIMEOUT 20 LABELS "fast;unit")

add_library(mwfl_header_printing_settings OBJECT header_printing_settings.cpp)
target_compile_features(mwfl_header_printing_settings PRIVATE cxx_std_20)
target_compile_options(mwfl_header_printing_settings PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_printing_settings PRIVATE mwfl::printing)

add_executable(mwfl_printing_settings_test printing_settings_test.cpp)
target_compile_features(mwfl_printing_settings_test PRIVATE cxx_std_20)
target_compile_options(mwfl_printing_settings_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_printing_settings_test PRIVATE mwfl::printing)
add_test(NAME mwfl.printing_settings COMMAND mwfl_printing_settings_test)
set_tests_properties(mwfl.printing_settings PROPERTIES TIMEOUT 20 LABELS "native")

add_library(mwfl_header_ole_data OBJECT header_ole_data.cpp)
target_compile_features(mwfl_header_ole_data PRIVATE cxx_std_20)
target_compile_options(mwfl_header_ole_data PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_ole_data PRIVATE mwfl::ole)

add_executable(mwfl_ole_data_test ole_data_test.cpp)
target_compile_features(mwfl_ole_data_test PRIVATE cxx_std_20)
target_compile_options(mwfl_ole_data_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_ole_data_test PRIVATE mwfl::ole)
add_test(NAME mwfl.ole_data COMMAND mwfl_ole_data_test)
set_tests_properties(mwfl.ole_data PROPERTIES TIMEOUT 20 LABELS "native;unit")

add_library(mwfl_header_ole_drag_drop OBJECT header_ole_drag_drop.cpp)
target_compile_features(mwfl_header_ole_drag_drop PRIVATE cxx_std_20)
target_compile_options(mwfl_header_ole_drag_drop PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_ole_drag_drop PRIVATE mwfl::ole)

add_executable(mwfl_ole_drag_drop_test ole_drag_drop_test.cpp)
target_compile_features(mwfl_ole_drag_drop_test PRIVATE cxx_std_20)
target_compile_options(mwfl_ole_drag_drop_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_ole_drag_drop_test PRIVATE mwfl::ole)
add_test(NAME mwfl.ole_drag_drop COMMAND mwfl_ole_drag_drop_test)
set_tests_properties(mwfl.ole_drag_drop PROPERTIES TIMEOUT 20 LABELS "native;unit")

add_library(mwfl_header_settings_store OBJECT header_settings_store.cpp)
target_compile_features(mwfl_header_settings_store PRIVATE cxx_std_20)
target_compile_options(mwfl_header_settings_store PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_settings_store PRIVATE mwfl::shell)

add_executable(mwfl_settings_store_test settings_store_test.cpp)
target_compile_features(mwfl_settings_store_test PRIVATE cxx_std_20)
target_compile_options(mwfl_settings_store_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_settings_store_test PRIVATE mwfl::shell)
add_test(NAME mwfl.settings_store COMMAND mwfl_settings_store_test)
set_tests_properties(mwfl.settings_store PROPERTIES TIMEOUT 20 LABELS "native;unit")

add_library(mwfl_header_file_association OBJECT header_file_association.cpp)
target_compile_features(mwfl_header_file_association PRIVATE cxx_std_20)
target_compile_options(mwfl_header_file_association PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_file_association PRIVATE mwfl::shell)

add_executable(mwfl_file_association_test file_association_test.cpp)
target_compile_features(mwfl_file_association_test PRIVATE cxx_std_20)
target_compile_options(mwfl_file_association_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_file_association_test PRIVATE mwfl::shell)
add_test(NAME mwfl.file_association COMMAND mwfl_file_association_test)
set_tests_properties(mwfl.file_association PROPERTIES TIMEOUT 20 LABELS "native;unit")

add_library(mwfl_header_shell_integration OBJECT header_shell_integration.cpp)
target_compile_features(mwfl_header_shell_integration PRIVATE cxx_std_20)
target_compile_options(mwfl_header_shell_integration PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_header_shell_integration PRIVATE mwfl::shell)

add_executable(mwfl_shell_integration_test shell_integration_test.cpp)
target_compile_features(mwfl_shell_integration_test PRIVATE cxx_std_20)
target_compile_options(mwfl_shell_integration_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_shell_integration_test PRIVATE mwfl::shell)
add_test(NAME mwfl.shell_integration COMMAND mwfl_shell_integration_test)
set_tests_properties(mwfl.shell_integration PROPERTIES TIMEOUT 30 LABELS "native;integration")

add_executable(mwfl_integration_stress_test integration_stress_test.cpp)
target_compile_features(mwfl_integration_stress_test PRIVATE cxx_std_20)
target_compile_options(mwfl_integration_stress_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_integration_stress_test PRIVATE
    mwfl::printing mwfl::ole mwfl::shell mwfl::ribbon mwfl::mdi mwfl::graphics)
add_test(NAME mwfl.integration_stress COMMAND mwfl_integration_stress_test)
set_tests_properties(mwfl.integration_stress PROPERTIES TIMEOUT 60 LABELS "native;lifecycle")

add_executable(mwfl_imaging_decode_test imaging_decode_test.cpp)
target_compile_features(mwfl_imaging_decode_test PRIVATE cxx_std_20)
target_compile_options(mwfl_imaging_decode_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_imaging_decode_test PRIVATE mwfl::imaging)
add_test(NAME mwfl.imaging_decode COMMAND mwfl_imaging_decode_test)
set_tests_properties(mwfl.imaging_decode PROPERTIES TIMEOUT 20 LABELS "fast;unit")

if(MWFL_BUILD_SCINTILLA)
    add_library(mwfl_header_scintilla OBJECT header_scintilla.cpp)
    target_compile_features(mwfl_header_scintilla PRIVATE cxx_std_20)
    target_compile_options(mwfl_header_scintilla PRIVATE /W4 /permissive- /EHsc /utf-8)
    target_link_libraries(mwfl_header_scintilla PRIVATE mwfl::scintilla)

    add_executable(mwfl_scintilla_text_test scintilla_text_test.cpp)
    target_compile_features(mwfl_scintilla_text_test PRIVATE cxx_std_20)
    target_compile_options(mwfl_scintilla_text_test PRIVATE /W4 /permissive- /EHsc /utf-8)
    target_link_libraries(mwfl_scintilla_text_test PRIVATE mwfl::scintilla)
    add_test(NAME mwfl.scintilla_text COMMAND mwfl_scintilla_text_test)
    set_tests_properties(mwfl.scintilla_text PROPERTIES TIMEOUT 20 LABELS "fast;unit")

    add_executable(mwfl_scintilla_native_test WIN32 scintilla_native_test.cpp)
    target_sources(mwfl_scintilla_native_test PRIVATE "${PROJECT_SOURCE_DIR}/examples/example.manifest")
    target_compile_features(mwfl_scintilla_native_test PRIVATE cxx_std_20)
    target_compile_options(mwfl_scintilla_native_test PRIVATE /W4 /permissive- /EHsc /utf-8)
    target_link_libraries(mwfl_scintilla_native_test PRIVATE mwfl::scintilla oleacc)
    mwfl_deploy_scintilla(mwfl_scintilla_native_test)
    add_test(NAME mwfl.scintilla_native COMMAND mwfl_scintilla_native_test)
    set_tests_properties(mwfl.scintilla_native PROPERTIES TIMEOUT 30 LABELS "fast;unit")

    find_program(mwfl_scintilla_test_powershell NAMES pwsh powershell REQUIRED)
    add_test(NAME mwfl.code_editor_gui
        COMMAND "${mwfl_scintilla_test_powershell}" -NoProfile -NonInteractive
            -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/run_gui_self_test.ps1"
            -Executable $<TARGET_FILE:mwfl_code_editor_demo>
            -RuntimePath "$<TARGET_FILE_DIR:mwfl_code_editor_demo>")
    set_tests_properties(mwfl.code_editor_gui PROPERTIES TIMEOUT 20 LABELS "gui;integration")
endif()

if(MWFL_BUILD_WEBVIEW2)
    add_library(mwfl_header_webview2 OBJECT header_webview2.cpp)
    target_compile_features(mwfl_header_webview2 PRIVATE cxx_std_20)
    target_compile_options(mwfl_header_webview2 PRIVATE /W4 /permissive- /EHsc /utf-8)
    target_link_libraries(mwfl_header_webview2 PRIVATE mwfl::webview2)

    add_executable(mwfl_webview2_model_test webview2_model_test.cpp)
    target_compile_features(mwfl_webview2_model_test PRIVATE cxx_std_20)
    target_compile_options(mwfl_webview2_model_test PRIVATE /W4 /permissive- /EHsc /utf-8)
    target_link_libraries(mwfl_webview2_model_test PRIVATE mwfl::webview2)
    add_test(NAME mwfl.webview2_model COMMAND mwfl_webview2_model_test)
    set_tests_properties(mwfl.webview2_model PROPERTIES TIMEOUT 20 LABELS "fast;unit")

    add_executable(mwfl_webview2_native_test WIN32 webview2_native_test.cpp)
    target_sources(mwfl_webview2_native_test PRIVATE "${PROJECT_SOURCE_DIR}/examples/example.manifest")
    target_compile_features(mwfl_webview2_native_test PRIVATE cxx_std_20)
    target_compile_options(mwfl_webview2_native_test PRIVATE /W4 /permissive- /EHsc /utf-8)
    target_link_libraries(mwfl_webview2_native_test PRIVATE mwfl::webview2)
    add_test(NAME mwfl.webview2_native COMMAND mwfl_webview2_native_test)
    set_tests_properties(mwfl.webview2_native PROPERTIES TIMEOUT 45 LABELS "integration;native")

    find_program(mwfl_webview2_test_powershell NAMES pwsh powershell REQUIRED)
    add_test(NAME mwfl.browser_gui
        COMMAND "${mwfl_webview2_test_powershell}" -NoProfile -NonInteractive
            -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/run_gui_self_test.ps1"
            -Executable $<TARGET_FILE:mwfl_browser_demo>
            -RuntimePath "$<TARGET_FILE_DIR:mwfl_browser_demo>")
    set_tests_properties(mwfl.browser_gui PROPERTIES TIMEOUT 45 LABELS "gui;integration")
endif()

add_executable(mwfl_property_sheet_model_test property_sheet_model_test.cpp)
target_compile_features(mwfl_property_sheet_model_test PRIVATE cxx_std_20)
target_compile_options(mwfl_property_sheet_model_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_property_sheet_model_test PRIVATE mwfl::ui)
add_test(NAME mwfl.property_sheet_model COMMAND mwfl_property_sheet_model_test)
set_tests_properties(mwfl.property_sheet_model PROPERTIES TIMEOUT 20)

add_executable(mwfl_settings_application_model_test
    settings_application_model_test.cpp
    "${PROJECT_SOURCE_DIR}/examples/property_sheet/settings_model.cpp")
target_include_directories(mwfl_settings_application_model_test PRIVATE
    "${PROJECT_SOURCE_DIR}/examples/property_sheet")
target_compile_features(mwfl_settings_application_model_test PRIVATE cxx_std_20)
target_compile_options(mwfl_settings_application_model_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_settings_application_model_test PRIVATE mwfl::ui)
add_test(NAME mwfl.settings_application_model COMMAND mwfl_settings_application_model_test)
set_tests_properties(mwfl.settings_application_model PROPERTIES TIMEOUT 20)

add_executable(mwfl_property_sheet_native_test property_sheet_native_test.cpp)
target_sources(mwfl_property_sheet_native_test PRIVATE "${PROJECT_SOURCE_DIR}/examples/example.manifest")
target_compile_features(mwfl_property_sheet_native_test PRIVATE cxx_std_20)
target_compile_options(mwfl_property_sheet_native_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_property_sheet_native_test PRIVATE mwfl::ui)
add_test(NAME mwfl.property_sheet_native COMMAND mwfl_property_sheet_native_test)
set_tests_properties(mwfl.property_sheet_native PROPERTIES
    ENVIRONMENT "MWFL_TEST_NO_DIALOGS=1"
    TIMEOUT 20)

add_executable(mwfl_task_dialog_test task_dialog_test.cpp)
target_sources(mwfl_task_dialog_test PRIVATE "${PROJECT_SOURCE_DIR}/examples/example.manifest")
target_compile_features(mwfl_task_dialog_test PRIVATE cxx_std_20)
target_compile_options(mwfl_task_dialog_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_task_dialog_test PRIVATE mwfl::ui)
add_test(NAME mwfl.task_dialog COMMAND mwfl_task_dialog_test)
set_tests_properties(mwfl.task_dialog PROPERTIES TIMEOUT 20)

add_executable(mwfl_dialog_native_test dialog_native_test.cpp)
target_sources(mwfl_dialog_native_test PRIVATE "${PROJECT_SOURCE_DIR}/examples/example.manifest")
target_compile_features(mwfl_dialog_native_test PRIVATE cxx_std_20)
target_compile_options(mwfl_dialog_native_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_dialog_native_test PRIVATE mwfl::ui)
add_test(NAME mwfl.dialog_native COMMAND mwfl_dialog_native_test)
set_tests_properties(mwfl.dialog_native PROPERTIES TIMEOUT 20)

add_executable(mwfl_tray_icon_native_test tray_icon_native_test.cpp)
target_sources(mwfl_tray_icon_native_test PRIVATE "${PROJECT_SOURCE_DIR}/examples/example.manifest")
target_compile_features(mwfl_tray_icon_native_test PRIVATE cxx_std_20)
target_compile_options(mwfl_tray_icon_native_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_tray_icon_native_test PRIVATE mwfl::ui)
add_test(NAME mwfl.tray_icon_native COMMAND mwfl_tray_icon_native_test)
set_tests_properties(mwfl.tray_icon_native PROPERTIES TIMEOUT 30)

add_executable(mwfl_tray_icon_state_test tray_icon_state_test.cpp)
target_compile_features(mwfl_tray_icon_state_test PRIVATE cxx_std_20)
target_compile_options(mwfl_tray_icon_state_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_tray_icon_state_test PRIVATE mwfl::ui)
add_test(NAME mwfl.tray_icon_state COMMAND mwfl_tray_icon_state_test)
set_tests_properties(mwfl.tray_icon_state PROPERTIES LABELS "fast;unit")

add_executable(mwfl_control_resources_native_test control_resources_native_test.cpp)
target_sources(mwfl_control_resources_native_test PRIVATE
    "${PROJECT_SOURCE_DIR}/examples/example.manifest")
target_compile_features(mwfl_control_resources_native_test PRIVATE cxx_std_20)
target_compile_options(mwfl_control_resources_native_test PRIVATE
    /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_control_resources_native_test PRIVATE mwfl::ui)
add_test(NAME mwfl.control_resources_native COMMAND mwfl_control_resources_native_test)
set_tests_properties(mwfl.control_resources_native PROPERTIES TIMEOUT 30)

add_executable(mwfl_navigation_native_test navigation_native_test.cpp)
target_sources(mwfl_navigation_native_test PRIVATE
    "${PROJECT_SOURCE_DIR}/examples/example.manifest")
target_compile_features(mwfl_navigation_native_test PRIVATE cxx_std_20)
target_compile_options(mwfl_navigation_native_test PRIVATE
    /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_navigation_native_test PRIVATE mwfl::ui)
add_test(NAME mwfl.navigation_native COMMAND mwfl_navigation_native_test)
set_tests_properties(mwfl.navigation_native PROPERTIES TIMEOUT 30)

add_executable(mwfl_navigation_model_test navigation_model_test.cpp)
target_compile_features(mwfl_navigation_model_test PRIVATE cxx_std_20)
target_compile_options(mwfl_navigation_model_test PRIVATE
    /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_navigation_model_test PRIVATE mwfl::ui)
add_test(NAME mwfl.navigation_model COMMAND mwfl_navigation_model_test)
set_tests_properties(mwfl.navigation_model PROPERTIES TIMEOUT 20)

add_executable(mwfl_explorer_model_test
    explorer_model_test.cpp
    "${PROJECT_SOURCE_DIR}/examples/explorer/explorer_model.cpp")
target_include_directories(mwfl_explorer_model_test PRIVATE
    "${PROJECT_SOURCE_DIR}/examples/explorer")
target_compile_features(mwfl_explorer_model_test PRIVATE cxx_std_20)
target_compile_options(mwfl_explorer_model_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(mwfl_explorer_model_test PRIVATE mwfl::ui)
add_test(NAME mwfl.explorer_model COMMAND mwfl_explorer_model_test)
set_tests_properties(mwfl.explorer_model PROPERTIES TIMEOUT 20)

add_executable(mwfl_hot_corner_model_test
    hot_corner_model_test.cpp
    "${PROJECT_SOURCE_DIR}/examples/hot_corners/hot_corner_model.cpp")
target_compile_features(mwfl_hot_corner_model_test PRIVATE cxx_std_20)
target_compile_options(mwfl_hot_corner_model_test PRIVATE /W4 /permissive- /EHsc /utf-8)
target_include_directories(mwfl_hot_corner_model_test PRIVATE
    "${PROJECT_SOURCE_DIR}/examples/hot_corners")
add_test(NAME mwfl.hot_corner_model COMMAND mwfl_hot_corner_model_test)
set_tests_properties(mwfl.hot_corner_model PROPERTIES TIMEOUT 20)

if(TARGET mwfl_hello)
    set(mwfl_mt_tool "")
    if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
        set(mwfl_mt_tool
            "C:/Program Files (x86)/Windows Kits/10/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64/mt.exe")
    endif()
    if(NOT EXISTS "${mwfl_mt_tool}" AND EXISTS "${CMAKE_MT}")
        set(mwfl_mt_tool "${CMAKE_MT}")
    endif()
    if(NOT EXISTS "${mwfl_mt_tool}")
        find_program(mwfl_mt_tool NAMES mt.exe)
    endif()
    foreach(example_target IN ITEMS
            mwfl_hello
            mwfl_application_demo
            mwfl_window_demo
            mwfl_native_message_demo
            mwfl_keyboard_demo
            mwfl_mouse_demo
            mwfl_resize_demo
            mwfl_timer_demo
            mwfl_paint_demo
            mwfl_minmax_demo
            mwfl_close_policy_demo
            mwfl_window_state_demo
            mwfl_dpi_demo
            mwfl_window_options_demo
            mwfl_wait_aware_demo
            mwfl_wakeup_demo
            mwfl_com_sta_demo
            mwfl_controls_demo
            mwfl_common_controls_demo
            mwfl_self_drawn_host_demo
            mwfl_system_lifecycle_demo
            mwfl_hot_corners_demo
            mwfl_form_binding_demo
            mwfl_commands_demo
            mwfl_desktop_integration_demo
            mwfl_document_state_demo
            mwfl_notepad
            mwfl_appearance_demo
            mwfl_layout_gallery_demo
            mwfl_property_sheet_demo
            mwfl_explorer_demo
            mwfl_drawing_demo
            mwfl_image_viewer_demo
            mwfl_printing_demo
            mwfl_ole_drag_drop_demo
            mwfl_docking_workspace_demo
            mwfl_code_editor_demo
            mwfl_browser_demo)
        if(NOT TARGET ${example_target})
            continue()
        endif()
        add_test(
            NAME mwfl.manifest.${example_target}
            COMMAND "${CMAKE_COMMAND}"
                "-DMT_TOOL=${mwfl_mt_tool}"
                "-DEXECUTABLE=$<TARGET_FILE:${example_target}>"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/${example_target}-manifest.xml"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_manifest.cmake")
        set_tests_properties(mwfl.manifest.${example_target} PROPERTIES TIMEOUT 20)
    endforeach()
    get_filename_component(
        mwfl_test_compiler_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(mwfl_test_runtime_path "${mwfl_test_compiler_directory}")
    list(JOIN mwfl_test_runtime_path ";" mwfl_test_runtime_path)
    find_program(mwfl_powershell NAMES pwsh powershell REQUIRED)
    add_test(NAME mwfl.hot_corners_self_test
        COMMAND "${mwfl_powershell}" -NoProfile -NonInteractive
            -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/run_gui_self_test.ps1"
            -Executable $<TARGET_FILE:mwfl_hot_corners_demo>
            -RuntimePath "${mwfl_test_runtime_path}")
    set_tests_properties(mwfl.hot_corners_self_test PROPERTIES TIMEOUT 10)
    add_test(NAME mwfl.settings_application_gui
        COMMAND "${mwfl_powershell}" -NoProfile -NonInteractive
            -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/run_gui_self_test.ps1"
            -Executable $<TARGET_FILE:mwfl_property_sheet_demo>
            -RuntimePath "${mwfl_test_runtime_path}")
    set_tests_properties(mwfl.settings_application_gui PROPERTIES TIMEOUT 15)
    add_test(NAME mwfl.explorer_gui
        COMMAND "${mwfl_powershell}" -NoProfile -NonInteractive
            -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/run_gui_self_test.ps1"
            -Executable $<TARGET_FILE:mwfl_explorer_demo>
            -RuntimePath "${mwfl_test_runtime_path}")
    set_tests_properties(mwfl.explorer_gui PROPERTIES TIMEOUT 15)
    add_test(NAME mwfl.drawing_gui
        COMMAND "${mwfl_powershell}" -NoProfile -NonInteractive
            -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/run_gui_self_test.ps1"
            -Executable $<TARGET_FILE:mwfl_drawing_demo>
            -RuntimePath "${mwfl_test_runtime_path}")
    set_tests_properties(mwfl.drawing_gui PROPERTIES TIMEOUT 15)
    add_test(NAME mwfl.image_viewer_gui
        COMMAND "${mwfl_powershell}" -NoProfile -NonInteractive
            -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/run_gui_self_test.ps1"
            -Executable $<TARGET_FILE:mwfl_image_viewer_demo>
            -RuntimePath "${mwfl_test_runtime_path}")
    set_tests_properties(mwfl.image_viewer_gui PROPERTIES TIMEOUT 15)
    add_test(NAME mwfl.printing_gui
        COMMAND "${mwfl_powershell}" -NoProfile -NonInteractive
            -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/run_gui_self_test.ps1"
            -Executable $<TARGET_FILE:mwfl_printing_demo>
            -RuntimePath "${mwfl_test_runtime_path}")
    set_tests_properties(mwfl.printing_gui PROPERTIES TIMEOUT 15)
    add_test(NAME mwfl.ole_drag_drop_gui
        COMMAND "${mwfl_powershell}" -NoProfile -NonInteractive
            -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/run_gui_self_test.ps1"
            -Executable $<TARGET_FILE:mwfl_ole_drag_drop_demo>
            -RuntimePath "${mwfl_test_runtime_path}")
    set_tests_properties(mwfl.ole_drag_drop_gui PROPERTIES TIMEOUT 15)
    add_test(NAME mwfl.shell_integration_gui
        COMMAND "${mwfl_powershell}" -NoProfile -NonInteractive
            -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/run_gui_self_test.ps1"
            -Executable $<TARGET_FILE:mwfl_shell_integration_demo>
            -RuntimePath "${mwfl_test_runtime_path}")
    set_tests_properties(mwfl.shell_integration_gui PROPERTIES TIMEOUT 15)
    add_test(NAME mwfl.docking_workspace_gui
        COMMAND "${mwfl_powershell}" -NoProfile -NonInteractive
            -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/run_gui_self_test.ps1"
            -Executable $<TARGET_FILE:mwfl_docking_workspace_demo>
            -RuntimePath "${mwfl_test_runtime_path}")
    set_tests_properties(mwfl.docking_workspace_gui PROPERTIES TIMEOUT 15)
endif()

add_test(
    NAME mwfl.site_links
    COMMAND "${CMAKE_COMMAND}"
        "-DSITE_ROOT=${PROJECT_SOURCE_DIR}/site"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_site.cmake")
set_tests_properties(mwfl.site_links PROPERTIES TIMEOUT 20)

add_test(
    NAME mwfl.examples.catalog
    COMMAND "${CMAKE_COMMAND}"
        "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_examples.cmake")
set_tests_properties(mwfl.examples.catalog PROPERTIES TIMEOUT 20)

add_test(
    NAME mwfl.package_consumer
    COMMAND "${CMAKE_COMMAND}"
        "-DMWFL_BUILD_DIR=${PROJECT_BINARY_DIR}"
        "-DMWFL_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DMWFL_INSTALL_DIR=${PROJECT_BINARY_DIR}/package-install"
        "-DMWFL_CONFIGURATION=$<CONFIG>"
        "-DMWFL_GENERATOR=${CMAKE_GENERATOR}"
        "-DMWFL_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DMWFL_ENABLE_ASAN=${MWFL_ENABLE_ASAN}"
        "-DMWFL_EXPECT_NOTEPAD=$<BOOL:$<TARGET_EXISTS:mwfl_notepad>>"
        "-DMWFL_EXPECT_SCINTILLA=$<BOOL:$<TARGET_EXISTS:mwfl_scintilla>>"
        "-DMWFL_EXPECT_WEBVIEW2=$<BOOL:$<TARGET_EXISTS:mwfl_webview2>>"
        "-DMWFL_WIL_SOURCE_DIR=${MWFL_RESOLVED_WIL_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_package_consumer.cmake")
set_tests_properties(mwfl.package_consumer PROPERTIES TIMEOUT 600)

add_test(
    NAME mwfl.release_metadata
    COMMAND "${CMAKE_COMMAND}"
        "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_release_metadata.cmake")
set_tests_properties(mwfl.release_metadata PROPERTIES TIMEOUT 20)

add_test(
    NAME mwfl.agent_assets
    COMMAND "${CMAKE_COMMAND}"
        "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_agent_assets.cmake")
set_tests_properties(mwfl.agent_assets PROPERTIES TIMEOUT 20)

add_test(
    NAME mwfl.developer_assets
    COMMAND "${CMAKE_COMMAND}"
        "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_developer_assets.cmake")
set_tests_properties(mwfl.developer_assets PROPERTIES TIMEOUT 20)

add_test(
    NAME mwfl.agent_evals
    COMMAND "${CMAKE_COMMAND}"
        "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_agent_evals.cmake")
set_tests_properties(mwfl.agent_evals PROPERTIES TIMEOUT 20)

add_test(NAME mwfl.api_index
    COMMAND "${CMAKE_COMMAND}" "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_api_index.cmake")
set_tests_properties(mwfl.api_index PROPERTIES TIMEOUT 20)

find_program(mwfl_test_powershell NAMES pwsh powershell REQUIRED)
add_test(NAME mwfl.change_selector
    COMMAND "${mwfl_test_powershell}" -NoProfile -NonInteractive
        -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/verify_change_selector.ps1"
        -ProjectRoot "${PROJECT_SOURCE_DIR}")
set_tests_properties(mwfl.change_selector PROPERTIES TIMEOUT 20)

add_test(NAME mwfl.architecture_boundaries
    COMMAND "${CMAKE_COMMAND}" "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_architecture_boundaries.cmake")
set_tests_properties(mwfl.architecture_boundaries PROPERTIES TIMEOUT 20)

add_test(NAME mwfl.parity_roadmap
    COMMAND "${CMAKE_COMMAND}" "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_parity_roadmap.cmake")
set_tests_properties(mwfl.parity_roadmap PROPERTIES TIMEOUT 20)

if(TARGET mwfl_notepad)
    add_test(NAME mwfl.notepad_gui
        COMMAND "${CMAKE_COMMAND}"
            "-DNOTEPAD_EXE=$<TARGET_FILE:mwfl_notepad>"
            "-DRESULT_FILE=$<TARGET_FILE_DIR:mwfl_notepad>/notepad-self-test.txt"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_notepad_gui.cmake")
    set_tests_properties(mwfl.notepad_gui PROPERTIES TIMEOUT 30)
endif()

if(TARGET mwfl_document_workspace)
    add_test(NAME mwfl.document_workspace_gui
        COMMAND "${CMAKE_COMMAND}"
            "-DWORKSPACE_EXE=$<TARGET_FILE:mwfl_document_workspace>"
            "-DRESULT_FILE=$<TARGET_FILE_DIR:mwfl_document_workspace>/document-workspace-self-test.txt"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_document_workspace_gui.cmake")
    set_tests_properties(mwfl.document_workspace_gui PROPERTIES TIMEOUT 30)
endif()

# Every test has semantic labels so local agents can run a predictable subset.
get_property(mwfl_all_tests DIRECTORY PROPERTY TESTS)
foreach(test_name IN LISTS mwfl_all_tests)
    if(test_name MATCHES "^mwfl\\.(lifecycle|advanced\\.|message_loop\\.)" OR
       test_name STREQUAL "mwfl.integration_stress" OR
       test_name STREQUAL "mwfl.docking_stress_native")
        set(labels "lifecycle;fast")
    elseif(test_name STREQUAL "mwfl.webview2_native")
        set(labels "integration;native")
    elseif(test_name MATCHES "^mwfl\\.manifest\\.")
        set(labels "metadata;fast")
    elseif(test_name STREQUAL "mwfl.hot_corners_self_test" OR
           test_name STREQUAL "mwfl.settings_application_gui" OR
           test_name STREQUAL "mwfl.explorer_gui" OR
           test_name STREQUAL "mwfl.drawing_gui" OR
           test_name STREQUAL "mwfl.image_viewer_gui" OR
           test_name STREQUAL "mwfl.printing_gui" OR
           test_name STREQUAL "mwfl.ole_drag_drop_gui" OR
           test_name STREQUAL "mwfl.shell_integration_gui" OR
           test_name STREQUAL "mwfl.docking_workspace_gui" OR
           test_name STREQUAL "mwfl.code_editor_gui" OR
           test_name STREQUAL "mwfl.browser_gui" OR
           test_name STREQUAL "mwfl.notepad_gui" OR
           test_name STREQUAL "mwfl.document_workspace_gui")
        set(labels "gui;integration")
    elseif(test_name MATCHES "^mwfl\\.(site_links|examples\\.catalog|release_metadata|agent_assets|developer_assets|agent_evals|api_index|change_selector|architecture_boundaries|parity_roadmap)$")
        set(labels "docs;metadata;fast")
    elseif(test_name STREQUAL "mwfl.package_consumer")
        set(labels "package;integration;slow")
    elseif(test_name MATCHES "^mwfl\\.(modern_api|desktop)$")
        set(labels "integration;fast")
    else()
        set(labels "unit;fast")
    endif()
    set_tests_properties(${test_name} PROPERTIES LABELS "${labels}")
endforeach()

foreach(test_name IN LISTS mwfl_all_tests)
    get_test_property(${test_name} LABELS labels)
    if(NOT labels)
        message(FATAL_ERROR "CTest test has no semantic labels: ${test_name}")
    endif()
endforeach()
