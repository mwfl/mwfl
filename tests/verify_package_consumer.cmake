if(NOT DEFINED MWTL_BUILD_DIR OR NOT DEFINED MWTL_SOURCE_DIR OR
   NOT DEFINED MWTL_INSTALL_DIR OR NOT DEFINED MWTL_CONFIGURATION OR
   NOT DEFINED MWTL_GENERATOR OR NOT DEFINED MWTL_PLATFORM OR
   NOT DEFINED MWTL_EXPECT_NOTEPAD OR NOT DEFINED MWTL_EXPECT_SCINTILLA OR
   NOT DEFINED MWTL_EXPECT_WEBVIEW2)
    message(FATAL_ERROR "Package consumer verification arguments are incomplete")
endif()

file(REMOVE_RECURSE "${MWTL_INSTALL_DIR}" "${MWTL_BUILD_DIR}/package-consumer-build")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${MWTL_BUILD_DIR}"
            --config "${MWTL_CONFIGURATION}" --prefix "${MWTL_INSTALL_DIR}"
    RESULT_VARIABLE install_result)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "mwtl install failed: ${install_result}")
endif()
if(MWTL_EXPECT_NOTEPAD AND NOT EXISTS "${MWTL_INSTALL_DIR}/bin/mwtl_notepad.exe")
    message(FATAL_ERROR "installed package is missing bin/mwtl_notepad.exe")
endif()
if(MWTL_EXPECT_SCINTILLA AND NOT EXISTS "${MWTL_INSTALL_DIR}/bin/Scintilla.dll")
    message(FATAL_ERROR "installed package is missing pinned bin/Scintilla.dll")
endif()

foreach(method IN ITEMS subdirectory fetchcontent)
    set(method_build "${MWTL_BUILD_DIR}/package-${method}-build")
    file(REMOVE_RECURSE "${method_build}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
                -S "${MWTL_SOURCE_DIR}/tests/package_${method}"
                -B "${method_build}"
                -G "${MWTL_GENERATOR}" -A "${MWTL_PLATFORM}"
                "-DMWTL_SOURCE_DIR=${MWTL_SOURCE_DIR}"
                "-DMWTL_WTL_SOURCE_DIR=${MWTL_WTL_SOURCE_DIR}"
                "-DMWTL_WIL_SOURCE_DIR=${MWTL_WIL_SOURCE_DIR}"
                "-DMWTL_DEPENDENCY_MODE=SYSTEM"
        RESULT_VARIABLE method_configure_result)
    if(NOT method_configure_result EQUAL 0)
        message(FATAL_ERROR "${method} consumer configure failed: ${method_configure_result}")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${method_build}"
                --config "${MWTL_CONFIGURATION}"
        RESULT_VARIABLE method_build_result)
    if(NOT method_build_result EQUAL 0)
        message(FATAL_ERROR "${method} consumer build failed: ${method_build_result}")
    endif()
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
            -S "${MWTL_SOURCE_DIR}/tests/package_consumer"
            -B "${MWTL_BUILD_DIR}/package-consumer-build"
            -G "${MWTL_GENERATOR}" -A "${MWTL_PLATFORM}"
            "-DCMAKE_PREFIX_PATH=${MWTL_INSTALL_DIR}"
            "-DMWTL_ENABLE_ASAN=${MWTL_ENABLE_ASAN}"
            "-DMWTL_TEST_SCINTILLA=${MWTL_EXPECT_SCINTILLA}"
            "-DMWTL_TEST_WEBVIEW2=${MWTL_EXPECT_WEBVIEW2}"
            "-DMWTL_WTL_SOURCE_DIR=${MWTL_WTL_SOURCE_DIR}"
            "-DMWTL_WIL_SOURCE_DIR=${MWTL_WIL_SOURCE_DIR}"
            "-DMWTL_DEPENDENCY_MODE=SYSTEM"
    RESULT_VARIABLE configure_result)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR "Installed-package consumer configure failed: ${configure_result}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${MWTL_BUILD_DIR}/package-consumer-build"
            --config "${MWTL_CONFIGURATION}"
    RESULT_VARIABLE build_result)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "Installed-package consumer build failed: ${build_result}")
endif()

if(MWTL_EXPECT_SCINTILLA)
    execute_process(
        COMMAND "${MWTL_BUILD_DIR}/package-consumer-build/${MWTL_CONFIGURATION}/mwtl_scintilla_package_consumer.exe"
        RESULT_VARIABLE scintilla_run_result)
    if(NOT scintilla_run_result EQUAL 0)
        message(FATAL_ERROR "Installed Scintilla consumer failed: ${scintilla_run_result}")
    endif()
endif()

if(MWTL_EXPECT_WEBVIEW2)
    if(NOT EXISTS "${MWTL_INSTALL_DIR}/lib/WebView2LoaderStatic.lib")
        message(FATAL_ERROR "installed package is missing pinned WebView2 static loader")
    endif()
    execute_process(
        COMMAND "${MWTL_BUILD_DIR}/package-consumer-build/${MWTL_CONFIGURATION}/mwtl_webview2_package_consumer.exe"
        RESULT_VARIABLE webview2_run_result)
    if(NOT webview2_run_result EQUAL 0)
        message(FATAL_ERROR "Installed WebView2 consumer failed: ${webview2_run_result}")
    endif()
endif()

execute_process(
    COMMAND "${MWTL_BUILD_DIR}/package-consumer-build/${MWTL_CONFIGURATION}/mwtl_package_consumer.exe"
    RESULT_VARIABLE run_result)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "Installed-package consumer failed: ${run_result}")
endif()
